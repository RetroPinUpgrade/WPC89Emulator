#ifdef MPU89_BUILD_FOR_COMPUTER
#include <ncurses.h>
#else
#include "gd32f4xx.h"
#endif
#include "cpu6809.h"
#include "asic.h"

#define ROM_BANK_SIZE                   16384
#define SERIALIZED_STATE_VERSION        5
#define RAM_SIZE                        0x4000 // this should be 0x2000 if everything is bounded correctly
#define ROM_SIZE                        1048576
#define RAM1_UPPER_ADDRESS              0x3000
#define DISPLAY_RAM_LOWER_PAGE_START    0x3800
#define DISPLAY_RAM_LOWER_PAGE_END      0x39FF
#define DISPLAY_RAM_UPPER_PAGE_START    0x3A00
#define DISPLAY_RAM_UPPER_PAGE_END      0x3BFF
#define RAM2_LOWER_ADDRESS              0x3C00
#define RAM2_UPPER_ADDRESS              0x3FAF
#define HARDWARE_UPPER_ADDRESS          0x3FFF
#define BANKED_ROM_LOWER_ADDRESS        0x4000
#define BANKED_ROM_UPPER_ADDRESS        0x7FFF
#define SYSTEM_ROM_LOWER_ADDRESS        0x8000
#define SYSTEM_ROM_UPPER_ADDRESS        0xFFFF

#define WPC_DMD_HIGH_PAGE               0x3FBC
#define WPC_DMD_SCANLINE                0x3FBD
#define WPC_DMD_LOW_PAGE                0x3FBE
#define WPC_DMD_ACTIVE_PAGE             0x3FBF

#define WPCS_DATA                       0x3FDC
#define WPCS_CONTROL_STATUS             0x3FDD

/*
WPCS_DATA 0x3FDC
R/W: Send/receive a byte of data to/from the sound board.
WPCS_CONTROL_STATUS 0x3FDD
R: WPC sound board read ready 0: R: DCS sound board read ready

The sound board needs the WDEN to go low
*/
typedef uint8_t byte;

bool MPUInit();
void MPURelease();

void MPUReset(bool turnOnBlanking);
void MPUMakePortsSafeForBlanking();
void MPUSetROMAddress(uint8_t *romLocation, uint32_t romSize);
void MPUSetCabinetInput(byte value);
void MPUSetSwitchInput(int switchNr, int optionalValue);
void MPUSetFliptronicsInput(int value, int optionalValue);
void MPUToggleMidnightMadnessMode();
void MPUSetDipSwitchByte(byte dipSwitch);
byte MPUGetDipSwitchByte();
void MPUStart();
unsigned short MPUExecuteCycle(unsigned short ticksToRun, unsigned short tickSteps);
void MPUWriteMemory(unsigned int offset, byte value);
byte MPURead8(unsigned short offset);
void MPUWrite8(unsigned short offset, byte value);
byte MPUBankswitchedRead(unsigned int offset);
void MPUHardwareWrite(unsigned int offset, byte value);
byte MPUHardwareRead(unsigned int offset);
uint8_t *MPUGetNVRAMStart();
uint16_t MPUGetNVRAMSize();

void MPUFIRQ();
void MPUIRQ();

//void SetASIC(CpuBoardAsic *s_asic);
bool MPULoadROM(char *filename);

byte *MPUGetDisplayScanlineData(byte scanLine);
void MPUCurrentScanline(byte curScanLine);
void MPUVerticalRefresh();
byte MPUGetTriggerScanline();
void MPUMPUActivateScanlineTrigger();
bool MPUDisplayHighPageOverride();

void WriteDisplay(uint16_t address, uint8_t data);
uint8_t ReadDisplay(uint16_t address);


// Simple delay helper: ~125ns at 240MHz (approx 30 cycles)
__attribute__((always_inline)) static inline void DelayQuarterCycle(void) {
    __NOP(); __NOP(); __NOP(); __NOP(); 
    __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); 
    __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); 
    __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); 
    __NOP(); __NOP(); 
}

// Force inline to ensure no function call overhead
__attribute__((always_inline)) static inline void SetAddressBus(uint16_t addressValue) {
    // The address lines (A0-A15) are driven by PortD (PD0-PD15)
    // Writes all 16 bits of Port D instantly.
    // Since PD0-PD15 covers the entire physical port, 
    // we don't need to mask anything.
    GPIO_OCTL(GPIOD) = (uint32_t)addressValue;
}

// Sets the Data Bus lines to dataValue
__attribute__((always_inline)) static inline void SetDataBus(uint8_t dataValue) {
    // The data lines D0-D7 are driven by PortE (PE0-PE7)
    // 1. Identify bits to SET (The 1s in val)
    uint32_t set_bits = (uint32_t)dataValue;

    // 2. Identify bits to RESET (The 0s in val)
    // We invert val, then mask to 0xFF so we don't accidentally reset PE8-PE15
    uint32_t reset_bits = (uint32_t)(~dataValue & 0xFF);

    // 3. Atomic Write
    // Shift reset_bits to the upper 16 bits
    GPIO_BOP(GPIOE) = set_bits | (reset_bits << 16);    
}

// Reads the state of Data Bus
__attribute__((always_inline)) static inline uint8_t ReadDataBus(void) {
    // Read the whole Input Status register and mask the bottom byte
    return (uint8_t)(GPIO_ISTAT(GPIOE) & 0xFF);
}

__attribute__((always_inline)) static inline void SetDataBusDirection(bool output) {
    // 1. Save interrupt state and disable
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    // 2. Perform the Read-Modify-Write
    uint32_t ctl = GPIO_CTL(GPIOE);
    ctl &= 0xFFFF0000; // Clear PE0-PE7
    if (output) {
        ctl |= 0x00005555; // Set PE0-PE7 to Output
    }
    GPIO_CTL(GPIOE) = ctl;

    // 3. Restore previous interrupt state
    __set_PRIMASK(primask);
}

// Strobes the Switch Column latch to latch the data
__attribute__((always_inline)) static inline void StrobeSwitchColLatch() {
    // 2. Raise the Clock (PE9) - LATCH HAPPENS HERE
    // The '374 latches on the Rising Edge (Low -> High)
    GPIO_BOP(GPIOE) = (1U << 9);

    // 3. Hold High (Pulse Width Delay)
    // At 240MHz, 5 NOPs ~= 20ns. 
    // We add a few extras just to be totally safe against bus jitter.
    DelayQuarterCycle();

    // 4. Lower the Clock (PE9)
    GPIO_BC(GPIOE) = (1U << 9);
}

__attribute__((always_inline)) static inline void SetSwitchRowLine(bool lineValue) {
    if (lineValue) GPIO_BOP(GPIOE) = (1U << 10);
    else GPIO_BC(GPIOE) = (1U << 10);
}

__attribute__((always_inline)) static inline void SetDirectSwitchRowLine(bool lineValue) {
    if (lineValue) GPIO_BOP(GPIOE) = (1U << 11);
    else GPIO_BC(GPIOE) = (1U << 11);
}

__attribute__((always_inline)) static inline void SetJumperSwitchRowLine(bool lineValue) {
    if (lineValue) GPIO_BOP(GPIOE) = (1U << 8);
    else GPIO_BC(GPIOE) = (1U << 8);
}

__attribute__((always_inline)) static inline void SetDRLine(bool lineValue) {
    if (lineValue) GPIO_BOP(GPIOB) = (1U << 4);
    else GPIO_BC(GPIOB) = (1U << 4);
}


// Turns Blanking on/off
// When Blanking is ON, peripherals are OFF
__attribute__((always_inline)) static inline void SetBlanking(bool high) {
    if (high) {
        // Set PE13 High
        GPIO_BOP(GPIOE) = (1U << 13);
    } else {
        // Set PE13 Low
        GPIO_BC(GPIOE) = (1U << 13);
    }
}


// This timing was tested with a reproduction Power Driver board
__attribute__((always_inline)) static inline void StrobeLampRow(void) {
    // 1. Drive Low (Prepare for edge)
    GPIO_BC(GPIOB) = (1U << 15);

    // 2. Hold Low 
    DelayQuarterCycle();

    // 3. Release High (LATCH triggers here)
    // Since PB15 is Open Drain, this switches the pin to Hi-Z.
    // The rising edge sharpness depends on your pull-up resistor.
    GPIO_BOP(GPIOB) = (1U << 15);
    DelayQuarterCycle();
    DelayQuarterCycle();
}

// This timing was tested with a reproduction Power Driver board
__attribute__((always_inline)) static inline void StrobeLampCol(void) {
    // 1. Drive Low
    GPIO_BC(GPIOB) = (1U << 14);

    // 2. Hold Low 
    DelayQuarterCycle();

    // 3. Release High (LATCH triggers here)
    GPIO_BOP(GPIOB) = (1U << 14);
    DelayQuarterCycle();
    DelayQuarterCycle();
    DelayQuarterCycle();
    DelayQuarterCycle();
}

__attribute__((always_inline)) static inline void StrobeTriac(void) {
    // 1. Drive Low
    GPIO_BC(GPIOB) = (1U << 3);

    // 2. Hold Low (~33ns)
    DelayQuarterCycle();

    // 3. Release High (LATCH triggers here)
    GPIO_BOP(GPIOB) = (1U << 3);
    DelayQuarterCycle();
    DelayQuarterCycle();
}


__attribute__((always_inline)) static inline void StrobeSol1(void) {
    // 1. Drive Low
    GPIO_BC(GPIOC) = (1U << 4);

    // 2. Hold Low (~33ns)
    DelayQuarterCycle();

    // 3. Release High (LATCH triggers here)
    GPIO_BOP(GPIOC) = (1U << 4);
    DelayQuarterCycle();
    DelayQuarterCycle();
}

__attribute__((always_inline)) static inline void StrobeSol2(void) {
    // 1. Drive Low
    GPIO_BC(GPIOC) = (1U << 5);

    // 2. Hold Low (~33ns)
    DelayQuarterCycle();

    // 3. Release High (LATCH triggers here)
    GPIO_BOP(GPIOC) = (1U << 5);
    DelayQuarterCycle();
    DelayQuarterCycle();
}

__attribute__((always_inline)) static inline void StrobeSol3(void) {
    // 1. Drive Low
    GPIO_BC(GPIOC) = (1U << 6);

    // 2. Hold Low (~33ns)
    DelayQuarterCycle();

    // 3. Release High (LATCH triggers here)
    GPIO_BOP(GPIOC) = (1U << 6);
    DelayQuarterCycle();
    DelayQuarterCycle();
}

__attribute__((always_inline)) static inline void StrobeSol4(void) {
    // 1. Drive Low
    GPIO_BC(GPIOC) = (1U << 7);

    // 2. Hold Low (~33ns)
    DelayQuarterCycle();

    // 3. Release High (LATCH triggers here)
    GPIO_BOP(GPIOC) = (1U << 7);
    DelayQuarterCycle();
    DelayQuarterCycle();
}

__attribute__((always_inline)) static inline bool FIRQTriggered(void) {
    return (GPIO_ISTAT(GPIOB) & GPIO_PIN_1) == 0;
}

__attribute__((always_inline)) static inline bool IRQTriggered(void) {
    return (GPIO_ISTAT(GPIOB) & GPIO_PIN_0) == 0;
}

__attribute__((always_inline)) static inline bool ReadESignal(void) {
    return (GPIO_ISTAT(GPIOA) & GPIO_PIN_10) != 0;
}

__attribute__((always_inline)) static inline bool ReadQSignal(void) {
    return (GPIO_ISTAT(GPIOA) & GPIO_PIN_9) != 0;
}
