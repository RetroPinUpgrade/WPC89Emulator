#include "mpu89.h"
#include "HPSoundCard.h"
// Williams part number A-12742-xx

bool DisplayUseShadowVariables = false;
//extern volatile uint32_t clock_idx;

byte RAM[RAM_SIZE];
byte *ROM;
uint32_t ROMSize;
byte DisplayRAM[8192];
byte DisplayLowPage = 0;
byte DisplayHighPage = 0;
byte *DisplayLowPageStartAddress = NULL;
byte *DisplayHighPageStartAddress = NULL;
byte DisplayActivePage = 0;
byte DisplayNextActivePage = 0;
byte DisplayScanlineTrigger = 31;
byte DisplayCurrentScanline = 0;
bool UseLegacySoundCard = false;
bool DisplayHighPageNeedsOverride = false;
int memoryWrites;
int protectedMemoryWriteAttempts = 0;
int ticksIrq = 0;
static uint32_t elapsedTicks;
byte *ROM;

uint32_t UpperAddressMask = 0x00000000;

#ifndef MPU89_BUILD_FOR_COMPUTER
#include "gd32f4xx.h"


void MCUPortInit() {
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);

    // ========================================================================
    // PORT A
    // ========================================================================
    // WDEN (PA6): Push-Pull Output, Initial=1    
    // IOEN (PA7): Push-Pull Output, Initial=1    
    // IO (PA8): Push-Pull Output, Initial=1    
    uint32_t pa_pp_pins = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
    gpio_bit_set(GPIOA, pa_pp_pins);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pa_pp_pins);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pa_pp_pins);

    // SDIO_CD (PA15): Input (GPIO_PUPD_NONE)
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_15);

    // ========================================================================
    // PORT B
    // ========================================================================
    // RW (PB2): Push-Pull Output, Initial=1
    // DREN (PB4): Push-Pull Output, Initial=1
    // RESET (PB5): Push-Pull Output, Initial=1
    // DISEN (PB13): Push-Pull Output, Initial=1
    // LAMPRow (PB15): Push-Pull Output, Initial = 1 (should be OD but this works cleaner signal-wise)
    uint32_t pb_pp_pins = GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_13 | GPIO_PIN_15;
    gpio_bit_set(GPIOB, pb_pp_pins);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pb_pp_pins);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pb_pp_pins);

    // TRIAC (PB3): Open Drain Output, Initial=1
    // DIS1 (PB8): Open Drain Output, Initial=1
    // DIS2 (PB9): Open Drain Output, Initial=1
    // DIS3 (PB10): Open Drain Output, Initial=1
    // DIS4 (PB11): Open Drain Output, Initial=1
    // DISStrobe (PB12): Open Drain Output, Initial=1
    // LAMPCol (PB14): Open Drain Output, Initial=1    
    uint32_t pb_od_pins = GPIO_PIN_3 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | 
    GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_14;
    gpio_bit_set(GPIOB, pb_od_pins);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, pb_od_pins);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pb_od_pins);

    // IRQ (PB0): Input (GPIO_PUPD_NONE)
    // FIRQ (PB1): Input (GPIO_PUPD_NONE)
    uint32_t pb_input_pins = GPIO_PIN_0 | GPIO_PIN_1;
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, pb_input_pins);

    // ========================================================================
    // PORT C 
    // ========================================================================
    // We should configure the SD card pins here

    // SOL1-4 (PC4, PC5, PC6, PC7): Open Drain Outputs
    // Initial=1
    uint32_t pc_od_pins = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    gpio_bit_set(GPIOC, pc_od_pins);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, pc_od_pins);
    gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pc_od_pins);

    // ========================================================================
    // PORT D
    // ========================================================================
    // E (PD0): Push-Pull Output, Initial = 1
    // Q (PD1): Push-Pull Output, Initial = 1
    // SDIO_CMD: Push-Pull Output, Initial = 1
    // AddressBus (PD3-PD15): Push-Pull Outputs, Initial=0x1FFF
    gpio_bit_set(GPIOD, GPIO_PIN_ALL);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_ALL);
    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_ALL);

    // ========================================================================
    // PORT E
    // ========================================================================
    // Inputs: DataBus (PE0-PE7), WDD (PE12), ZC (PE15)
    gpio_mode_set(GPIOE, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | 
                  GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_12 | GPIO_PIN_15);
  
    // Push-Pull Outputs: SWJMP (PE8), SWROW (PE10), SWDIR (PE11), Blank (PE13)
    // Initial=1
    gpio_bit_set(GPIOE, GPIO_PIN_8 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13);
  
    // SWCOL (PE9): Initial=0
    gpio_bit_reset(GPIOE, GPIO_PIN_9);
  
    uint32_t pe_pp_pins = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13;
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pe_pp_pins);
    gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pe_pp_pins);    
}

/*
void MCUPortInit(void) {
  rcu_periph_clock_enable(RCU_GPIOA);
  rcu_periph_clock_enable(RCU_GPIOB);
  rcu_periph_clock_enable(RCU_GPIOC);
  rcu_periph_clock_enable(RCU_GPIOD);
  rcu_periph_clock_enable(RCU_GPIOE);

  // ========================================================================
  // PORT A
  // ========================================================================
  // IO (PA8): Push-Pull, Initial=1
  gpio_bit_set(GPIOA, GPIO_PIN_8);
  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
  gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);

  // ========================================================================
  // PORT B
  // ========================================================================
  // Inputs: IRQ (PB0), FIRQ (PB1)
  gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1);

  // Push-Pull Outputs: RW (PB2), DREN (PB4), RESET (PB5), DISEN (PB13), LAMPRow (PB15)
  // Initial=1
  uint32_t pb_pp_pins = GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_13 | GPIO_PIN_15;
  gpio_bit_set(GPIOB, pb_pp_pins);
  gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pb_pp_pins);
  gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pb_pp_pins);

  // Open-Drain Outputs: TRIAC (PB3), DISSTROBE (PB12), LAMPCol (PB14), 
  // DIS1 (PB8), DIS2 (PB9), DIS3 (PB10), DIS4 (PB11)
  // Initial=1
  uint32_t pb_od_pins = GPIO_PIN_3 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | 
                        GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_14;
  gpio_bit_set(GPIOB, pb_od_pins);
  gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, pb_od_pins);
  gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pb_od_pins);

  // ========================================================================
  // PORT C
  // ========================================================================
  // High Impedance (Floating Input): E (PC0), Q (PC1)
  // Setting mode to INPUT with PUPD_NONE disconnects the pin driver and resistors.
  gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1);

  // Push-Pull Outputs: WDEN (PC8), IOEN (PC9)
  // Initial=1
  // (Note: Removed PC0 and PC1 from this list)
  gpio_bit_set(GPIOC, GPIO_PIN_8 | GPIO_PIN_9);
  gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9);
  gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8 | GPIO_PIN_9);

  // Open-Drain Outputs: SOL1-4 (PC4, PC5, PC6, PC7)
  // Initial=1
  uint32_t pc_od_pins = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  gpio_bit_set(GPIOC, pc_od_pins);
  gpio_output_options_set(GPIOC, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, pc_od_pins);
  gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pc_od_pins);

  // ========================================================================
  // PORT D
  // ========================================================================
  // AddressBus (PD0-PD15): Push-Pull, Initial=0xFFFF
  gpio_bit_set(GPIOD, GPIO_PIN_ALL);
  gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_ALL);
  gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_ALL);

  // ========================================================================
  // PORT E
  // ========================================================================
  // Inputs: DataBus (PE0-PE7), WDD (PE12)
  gpio_mode_set(GPIOE, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | 
                GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_12);

  // Push-Pull Outputs: SWJMP (PE8), SWROW (PE10), SWDIR (PE11), Blank (PE13)
  // Initial=1
  gpio_bit_set(GPIOE, GPIO_PIN_8 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13);

  // SWCOL (PE9): Initial=0
  gpio_bit_reset(GPIOE, GPIO_PIN_9);

  uint32_t pe_pp_pins = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13;
  gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pe_pp_pins);
  gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pe_pp_pins);
}

*/
#endif


void MPUMakePortsSafeForBlanking() {
  // latch lamp rows/cols with nothing
  // latch solenoids to nothing
  // turn off flippers and GI
  // turn off switch rows/cols just for good measure
  // turn off alphanumeric display stuff
}


bool MPUInit() {

  // clear RAM and Display's shadow RAM
  uint32_t *p = (uint32_t *)RAM;
  uint32_t words = RAM_SIZE / 4;  
  while(words--) *p++ = 0;
  p = (uint32_t *)DisplayRAM;
  words = 8192 / 4;  
  while(words--) *p++ = 0;

  MCUPortInit();
  MPUMakePortsSafeForBlanking();
  HPSoundCardInitConnection();
  SetDataBusDirection(true);
  ticksIrq = 0;
  protectedMemoryWriteAttempts = 0;
  memoryWrites = 0;
  UpperAddressMask = 0x00000000;
  ROM = NULL;
  ROMSize = 0;
  UseLegacySoundCard = false;

  return true;
}

void MPUUseLegacySoundCard(bool useLegacy) {
  UseLegacySoundCard = useLegacy;
}


void MPURelease() {
}

void MPUReset(bool turnOnBlanking) {
  ticksIrq = 0;
  protectedMemoryWriteAttempts = 0;
  memoryWrites = 0;
  CPUReset();
  ASICReset(turnOnBlanking);
  if (ASICGetDateTimeMemoryOffset()<RAM_SIZE) {
    ASICSetDateTimeMemoryPointer(&RAM[ASICGetDateTimeMemoryOffset()]);
  }
  DisplayLowPage = 0;
  DisplayHighPage = 0;
  DisplayLowPageStartAddress = &DisplayRAM[DisplayLowPage*512];
  DisplayHighPageStartAddress = &DisplayRAM[DisplayHighPage*512];
}

void MPUSetROMAddress(uint8_t *romLocation, uint32_t romSize) {
  ROM = romLocation;
  ROMSize = romSize;
  UpperAddressMask = (romSize-1) & 0xF0000;
}

void MPUSetCabinetInput(byte value) {
  ASICSetCabinetInput(value);
}

void MPUSetSwitchInput(int switchNr, int optionalValue) {
  ASICSetSwitchInput(switchNr, optionalValue);
}

void MPUSetFliptronicsInput(int value, int optionalValue) {
  ASICSetFliptronicsInput(value, optionalValue);
}

void MPUToggleMidnightMadnessMode() {
  ASICToggleMidnightMadnessMode();
}

void MPUSetDipSwitchByte(byte dipSwitch) {
  ASICSetDipSwitchByte(dipSwitch);
}

byte MPUGetDipSwitchByte() {
  return ASICGetDipSwitchByte();
}

void MPUStart() {
  MPUReset(true);
}

unsigned short MPUExecuteCycle(unsigned short ticksToRun, unsigned short tickSteps) {
  unsigned short ticksExecuted = 0;
  while (ticksExecuted < ticksToRun) {
    unsigned short singleTicks = CPUSteps(tickSteps);
    ticksExecuted += singleTicks;
    elapsedTicks += singleTicks;
    ticksIrq += singleTicks;

    // This is the periodic interrupt for the ISR that reads switches
    // and outputs lamps
    if (ticksIrq>=2048) {
      ticksIrq = 0;
      CPUIRQ();
      HPSoundCardUpdate(); // no need to check this constantly
    }

    ASICExecuteCycle(singleTicks);
  }

  return ticksExecuted;
}

void MPUWriteMemory(unsigned int offset, byte value) {
  //this.memoryWriteHandler.writeMemory(offset, value);
}

byte MPURead8(unsigned short offset) {
/*
  if (offset==WPC_SWITCH_COL_SELECT) {
    uint8_t test = ReadDataBus();
    if (test) {
      test = 0;
    }
  }
  SetAddressBus(offset);
*/  
  if (offset<RAM1_UPPER_ADDRESS) { // Anything less than 0x3000 (technically 0x2000-0x2FFF is no man's land

    /*
    if (offset>=0x1800 && offset<=0x1808) {
      volatile uint16_t year = RAM[0x1800]*256 + RAM[0x1801];
      volatile uint8_t month = RAM[0x1802];
      volatile uint8_t day = RAM[0x1803];
      volatile uint8_t dow = RAM[0x1804];
      volatile uint8_t hour = RAM[0x1805];
      volatile uint8_t valid = RAM[0x1806];
      volatile uint8_t checksumTime = RAM[0x1807];
      volatile uint8_t checksumDate = RAM[0x1808];
      (void)year;
      (void)month;
      (void)day;
      (void)dow;
      (void)hour;
      (void)valid;
      (void)checksumTime;
      (void)checksumDate;
    }
    */

    return RAM[offset];
  } else if (offset>=RAM2_LOWER_ADDRESS && offset<=RAM2_UPPER_ADDRESS) { // This can probably go?
    return RAM[offset];
  } else if (offset<=HARDWARE_UPPER_ADDRESS) { // Anything less than 0x3FFF
    return MPUHardwareRead(offset);
  } else if (offset<=BANKED_ROM_UPPER_ADDRESS) { // Anything less than 0x8000
    return MPUBankswitchedRead(offset);
  } else {
//    if (offset==0xFFEC) return 0x00;
//    if (offset==0xFFED) return 0xFF;
    uint32_t adjustedAddress = (uint32_t)offset | UpperAddressMask;
    return ROM[adjustedAddress];
  }

  return 0;
}

void MPUWrite8(unsigned short offset, byte value) {
  if (offset<=RAM1_UPPER_ADDRESS) RAM[offset] = value;
  else if (offset>=RAM2_LOWER_ADDRESS && offset<=RAM2_UPPER_ADDRESS) RAM[offset] = value;
  else if (offset<=HARDWARE_UPPER_ADDRESS) MPUHardwareWrite(offset, value);
}

byte MPUBankswitchedRead(unsigned int offset) {
  if (ROM) {
    unsigned int activeBank = ASICGetRomBank();
    unsigned int pageOffset = offset + ((activeBank * ROM_BANK_SIZE)-0x4000);
    return ROM[pageOffset];
  }
  return 0;
}


__attribute__((always_inline)) static inline void WriteLegacySoundCard(uint8_t data) {
  // 1. Setup Phase (Bus is idle, Transceiver is off)
  SetAddressBus(WPCS_DATA);
  SetDataBus(data);
  SetDataBusDirection(true);

  // 2. Wait for the "Safe Zone"
  // We wait for E to go LOW ( !E is HIGH ). 
  // This is the period where the RAM is NOT listening.
  while (ReadESignal()); 
  DelayQuarterCycle();

  // 3. Prepare the Transceiver
  // We set RW and Enable the 4245 while the RAM window is closed.
  SetRWLow();  // RW Low
  SetIOENLow();
  SetWDENLow(); // WDEN low alerts cards on single ribbon cable
  
  // 5. Trigger the Hardware Write
  // Now we wait for E to go HIGH ( !E goes LOW ).
  // The moment this happens, the OR gate (U19A) output drops.
  // !WE goes LOW. The RAM starts its write.
  while (!ReadESignal()); 
  DelayQuarterCycle();
  
  // 6. Stability Window
  // The RAM is now in its transparent write state. 
  // We stay here for a moment to ensure the signal is rock solid.
  __NOP(); __NOP(); __NOP(); __NOP();

  // 7. The Hardware Latch
  // We wait for E to go LOW ( !E goes HIGH ).
  // This hardware edge physically ENDS the write and latches the data.
  // The 4245 is still actively driving the bus.
  while (ReadESignal());
  DelayQuarterCycle();
  
  // 8. Data Hold & Cleanup
  // The write is over. We wait a tiny bit to satisfy RAM hold times
  // before we "let go" of the bus.
  __NOP(); __NOP(); __NOP(); 
  
  SetWDENHigh();    
  SetIOENHigh();
  SetRWHigh();    // RW High (Return to Read/Idle)
}

__attribute__((always_inline)) static inline uint8_t ReadLegacySoundCard(uint16_t address) {
  uint8_t result;

  SetAddressBus(address);    
  // 2. Prepare for Read: Set Port E to Input (High-Z)
  SetDataBusDirection(false); 
  
  // Set RW High (Read Mode)
  SetRWHigh();
  // Set IOEN line Low
  SetIOENLow();

  DelayQuarterCycle();
  DelayQuarterCycle();
  DelayQuarterCycle();

  SetWDENLow();
  DelayQuarterCycle();
  DelayQuarterCycle();
  DelayQuarterCycle();

  // Grab the data now while the board is still driving the bus
  result = ReadDataBus();

  // 7. Cleanup: Disable external hardware BEFORE restoring outputs
  SetWDENHigh();  // IO High
  SetIOENHigh();
  SetRWLow();   // Restore RW to Write (Default)

  // 8. Safety: Give external hardware ~15ns to Hi-Z its buffers
  DelayQuarterCycle();

  // Restore Data Bus to Output
  SetDataBusDirection(true);

  return result;
}


__attribute__((always_inline)) inline void WriteDisplay(uint16_t address, uint8_t data) {
  // 1. Setup Phase (Bus is idle, Transceiver is off)
  SetAddressBus(address);
  SetDataBus(data);
  SetDataBusDirection(true);

  // 2. Wait for the "Safe Zone"
  // We wait for E to go LOW ( !E is HIGH ). 
  // This is the period where the RAM is NOT listening.
  while (ReadESignal()); 
  DelayQuarterCycle();

  // 3. Prepare the Transceiver
  // We set RW and Enable the 4245 while the RAM window is closed.
  SetRWLow();  // RW Low
  SetIOENLow();
  
  // 4. Arm the !AddressValid (PORT)
  // We drop IO now. !WE is still HIGH because !E is still HIGH.
  SetIOLow(); // IO Low
  
  // 5. Trigger the Hardware Write
  // Now we wait for E to go HIGH ( !E goes LOW ).
  // The moment this happens, the OR gate (U19A) output drops.
  // !WE goes LOW. The RAM starts its write.
  while (!ReadESignal()); 
  DelayQuarterCycle();
  
  // 6. Stability Window
  // The RAM is now in its transparent write state. 
  // We stay here for a moment to ensure the signal is rock solid.
  __NOP(); __NOP(); __NOP(); __NOP();

  // 7. The Hardware Latch
  // We wait for E to go LOW ( !E goes HIGH ).
  // This hardware edge physically ENDS the write and latches the data.
  // The 4245 is still actively driving the bus.
  while (ReadESignal());
  DelayQuarterCycle();
  
  // 8. Data Hold & Cleanup
  // The write is over. We wait a tiny bit to satisfy RAM hold times
  // before we "let go" of the bus.
  __NOP(); __NOP(); __NOP(); 
  
  SetIOHigh();    // IO High (!AddressValid released)
  SetIOENHigh();
  SetRWHigh();    // RW High (Return to Read/Idle)
}



void MPUHardwareWrite(unsigned int offset, byte value) {
  // write/mirror value to ram, so its visible using the memory monitor
  RAM[offset] = value;
  
  if (offset>=WPC_FLIPTRONICS_FLIPPER_PORT_A && offset<=WPC_ZEROCROSS_IRQ_CLEAR) {
    if (offset==WPCS_DATA) {
      if (!UseLegacySoundCard) {
        HPSoundCardHandleCommand(value, elapsedTicks);
      } else {
        WriteLegacySoundCard(value);
      }
      return;
    }
    ASICWrite(offset, value);
    return;
  }

  if (offset>=DISPLAY_RAM_LOWER_PAGE_START && offset<=DISPLAY_RAM_LOWER_PAGE_END) {
    // Trying to write to the low page of display
    DisplayLowPageStartAddress[offset-DISPLAY_RAM_LOWER_PAGE_START] = value;
    WriteDisplay(offset, value);
  } else if (offset>=DISPLAY_RAM_UPPER_PAGE_START && offset<=DISPLAY_RAM_UPPER_PAGE_END) {
    // Trying to write to the high page of display
    DisplayHighPageStartAddress[offset-DISPLAY_RAM_UPPER_PAGE_START] = value;
    uint16_t currentPC = CPUGetPC();
    if (currentPC!=0x60DE) {
      WriteDisplay(offset, value);
      DisplayHighPageNeedsOverride = false;
    } else {
      DisplayHighPageNeedsOverride = true;
    }
  } else if (offset==WPC_DMD_LOW_PAGE) {
    DisplayLowPage = value;
    if (DisplayLowPage>15) DisplayLowPage = 15;
    DisplayLowPageStartAddress = &DisplayRAM[DisplayLowPage*512];
    WriteDisplay(offset, value);
  } else if (offset==WPC_DMD_HIGH_PAGE) {
    DisplayHighPage = value;
    if (DisplayHighPage>15) DisplayHighPage = 15;
    DisplayHighPageStartAddress = &DisplayRAM[DisplayHighPage*512];
    WriteDisplay(offset, value);
  } else if (offset==WPC_DMD_ACTIVE_PAGE) {
    DisplayNextActivePage = value;
    if (DisplayNextActivePage>15) DisplayNextActivePage = 15;
    WriteDisplay(offset, value);
  } else if (offset==WPC_DMD_SCANLINE /*|| offset==WPC_PERIPHERAL_TIMER_FIRQ_CLEAR*/) {
    DisplayScanlineTrigger = value;
    //if (offset==WPC_PERIPHERAL_TIMER_FIRQ_CLEAR) ASICFirqSourceDmd(false);
    WriteDisplay(offset, value);
  }


}



__attribute__((always_inline)) inline uint8_t ReadDisplay(uint16_t address) {
  uint8_t result;

  SetAddressBus(address);    
  // 2. Prepare for Read: Set Port E to Input (High-Z)
  SetDataBusDirection(false); 
  
  // Set RW High (Read Mode)
  SetRWHigh();
  // Set IOEN line Low
  SetIOENLow();

  DelayQuarterCycle();
  DelayQuarterCycle();
  DelayQuarterCycle();

  // 4. Trigger: Drop IO to enable the DMD address decoder
  SetIOLow();
  DelayQuarterCycle();
  DelayQuarterCycle();
  DelayQuarterCycle();

  // Grab the data now while the board is still driving the bus
  result = ReadDataBus();

  // 7. Cleanup: Disable external hardware BEFORE restoring outputs
  SetIOHigh();  // IO High
  SetIOENHigh();
  SetRWLow();   // Restore RW to Write (Default)

  // 8. Safety: Give external hardware ~15ns to Hi-Z its buffers
  DelayQuarterCycle();

  // Restore Data Bus to Output
  SetDataBusDirection(true);

  return result;
}

__attribute__((always_inline)) static inline uint8_t ReadDisplay1(uint16_t address) {
  uint8_t result;

  // 1. Setup Phase
  SetAddressBus(address);    
  SetDataBusDirection(false); 
  
  // 2. Initiate Read Mode
  SetRWHigh();
  
  // Wait for RW to physically cross the logic threshold
  DelayQuarterCycle(); 
  DelayQuarterCycle();

  SetIOENLow();

  // Wait for the Safe Zone (E is Low)
  while (ReadESignal()); 
  
  // Now it is safe to drop PORT 
  SetIOLow();

  // Hardware Trigger (E goes High)
  while (!ReadESignal()); 
  
  // Wait for 245 gate propagation 
  DelayQuarterCycle(); 

  // Sample the bus
  result = ReadDataBus();

  // Wait for E to go Low (End of cycle)
  while (ReadESignal());
  
  // 3. Initiate Deselect
  SetIOHigh();  
  SetIOENHigh();
  
  // 4. THE FIX: Match WriteDisplay idle state.
  // Leave RW High so the 245 cannot drive floating 0xFFs into the RAM.
  SetRWHigh();  

  // Turnaround Safety
  __NOP(); __NOP(); 
  SetDataBusDirection(true);

  return result;  
}



byte MPUHardwareRead(unsigned int offset) {

  if (offset>=WPC_FLIPTRONICS_FLIPPER_PORT_A && offset<=WPC_ZEROCROSS_IRQ_CLEAR) {
    if (offset==WPCS_CONTROL_STATUS) {
      if (!UseLegacySoundCard) {
        if (HPSoundCardCheckForOutboundByte(elapsedTicks)) return 0x01;
        else return 0x00;
      } else {
        return ReadLegacySoundCard(offset);
      }
    } else if (offset==WPCS_DATA) {
      if (!UseLegacySoundCard) {
        return HPSoundCardGetOutboundByte();
      } else {
        return ReadLegacySoundCard(offset);
      }
    }
    return ASICRead(offset);
  }

  if (offset>=DISPLAY_RAM_LOWER_PAGE_START && offset<=DISPLAY_RAM_LOWER_PAGE_END) {
    // For Display Reads we use shadow Display RAM
    // so we don't have to hit the bus
    if (!DisplayUseShadowVariables) {
//      return ReadDisplay1(offset);
    }
    return DisplayLowPageStartAddress[offset-0x3800];
  } else if (offset>=DISPLAY_RAM_UPPER_PAGE_START && offset<=DISPLAY_RAM_UPPER_PAGE_END) {
    // For Display Reads we use shadow Display RAM
    // so we don't have to hit the bus
    if (!DisplayUseShadowVariables) {
//      return ReadDisplay1(offset);
    }
    return DisplayHighPageStartAddress[offset-0x3A00];
  } else if (offset==WPC_DMD_LOW_PAGE) {
    if (!DisplayUseShadowVariables) {
      DisplayLowPage = ReadDisplay(offset);
    }
    return DisplayLowPage;
  } else if (offset==WPC_DMD_HIGH_PAGE) {
    if (!DisplayUseShadowVariables) {
      DisplayHighPage = ReadDisplay(offset);
    }
    return DisplayHighPage;
  } else if (offset==WPC_DMD_ACTIVE_PAGE) {
    if (!DisplayUseShadowVariables) {
      DisplayNextActivePage = ReadDisplay(offset);
    }
    return DisplayNextActivePage;
  } else if (offset==WPC_DMD_SCANLINE) {
    if (!DisplayUseShadowVariables) {
      DisplayCurrentScanline = ReadDisplay(offset);
    }
    if (DisplayCurrentScanline & WPC_FIRQ_CLEAR_BIT) ASICFirqSourceDmd(true);
    return DisplayCurrentScanline;
  } 

  return 0;
   
}

#ifdef MPU89_BUILD_FOR_COMPUTER  

bool MPULoadROM(char *filename) {
  long fileSize = 0x80000;

  FILE *fptr;
  // Open a file in read mode
  fptr = fopen(filename, "rb");

  // Print some text if the file does not exist
  if(fptr == NULL) {
    return false;
  }
  fseek(fptr, 0, SEEK_END); // seek to end of file
  fileSize = ftell(fptr); // get current file pointer
  fseek(fptr, 0, SEEK_SET); // seek back to beginning of file

  fread(ROM, 1, fileSize, fptr);

  // Close the file
  fclose(fptr);
  UpperAddressMask = (fileSize-1) & 0xF0000;
  return true;
}
#endif

// This function returns the current data of the
// active page at the given scanline
byte *MPUGetDisplayScanlineData(byte scanLine) {
  if (DisplayRAM==NULL) return NULL;

  byte *displayFrame = &DisplayRAM[DisplayActivePage*512 + scanLine*16];
  return displayFrame;
}

// This function tells the CPU board which
// scanline is currently being drawn by the display
// which is accessed when the CPU reads from WPC_DMD_SCANLINE
void MPUCurrentScanline(byte curScanLine) {
  DisplayCurrentScanline = curScanLine;
}

void MPUVerticalRefresh() {
  DisplayActivePage = DisplayNextActivePage;
}

// This function tells the display which scanline the
// CPU wants an interrupt on
byte MPUGetTriggerScanline() {
  return DisplayScanlineTrigger;
}

void MPUFIRQ() {
  CPUFIRQ();
}

uint8_t *MPUGetRAMAtIndex(uint16_t ramIndex) {
  if (ramIndex>=RAM_SIZE) return NULL;
  return &RAM[ramIndex];
}


uint8_t *MPUGetNVRAMStart() {
  return &RAM[0x16A1];
}

uint16_t MPUGetNVRAMSize() {
  return 0x95F;
}

bool MPUDisplayHighPageOverride() {
  return DisplayHighPageNeedsOverride;
}
