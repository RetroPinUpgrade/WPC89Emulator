#ifndef CPU_BOARD_ASIC_H
#define CPU_BOARD_ASIC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef MPU89_BUILD_FOR_COMPUTER  
#include <cstdint>
#include <vector>
#include <functional>
#include <ctime>
#include <memory>
#include <string>
#include <ncurses.h>
#endif 

// Timing Constants
#define CALL_UPDATELAMP_AFTER_TICKS     100
#define CALL_UPDATESOLENOID_AFTER_TICKS 100
#define WATCHDOG_ARMED_FOR_TICKS        5000
#define CALL_ZEROCLEAR_AFTER_TICKS      2000

// Memory Map Offsets
#define ASIC_RAM_BASE_ADDRESS            0x3FD4
#define WPC_FLIPTRONICS_FLIPPER_PORT_A   0x3FD4
#define WPC_SOLENOID_GEN_OUTPUT          0x3FE0
#define WPC_SOLENOID_HIGHPOWER_OUTPUT    0x3FE1
#define WPC_SOLENOID_FLASH1_OUTPUT       0x3FE2
#define WPC_SOLENOID_LOWPOWER_OUTPUT     0x3FE3
#define WPC_LAMP_ROW_OUTPUT              0x3FE4
#define WPC_LAMP_COL_STROBE              0x3FE5
#define WPC_GI_TRIAC                     0x3FE6
#define WPC_SW_JUMPER_INPUT              0x3FE7
#define WPC_SWITCH_CABINET_INPUT         0x3FE8
// PRE SECURITY PIC
#define WPC_SWITCH_ROW_SELECT            0x3FE9
#define WPC_SWITCH_COL_SELECT            0x3FEA
// SECURITY PIC
#define WPC_PICREAD                      0x3FE9
#define WPC_PICWRITE                     0x3FEA

#define WPC_EXTBOARD1                    0x3FEB
#define WPC_EXTBOARD2                    0x3FEC
#define WPC_EXTBOARD3                    0x3FED
#define WPC95_FLIPPER_COIL_OUTPUT        0x3FEE
#define WPC95_FLIPPER_SWITCH_INPUT       0x3FEF
#define WPC_LEDS                         0x3FF2
#define WPC_RAM_BANK                     0x3FF3
#define WPC_SHIFTADDRH                   0x3FF4
#define WPC_SHIFTADDRL                   0x3FF5
#define WPC_SHIFTBIT                     0x3FF6
#define WPC_SHIFTBIT2                    0x3FF7
#define WPC_PERIPHERAL_TIMER_FIRQ_CLEAR  0x3FF8
#define WPC_ROM_LOCK                     0x3FF9
#define WPC_CLK_HOURS_DAYS               0x3FFA
#define WPC_CLK_MINS                     0x3FFB
#define WPC_ROM_BANK                     0x3FFC
#define WPC_RAM_LOCK                     0x3FFD
#define WPC_RAM_LOCKSIZE                 0x3FFE
#define WPC_ZEROCROSS_IRQ_CLEAR          0x3FFF
#define ASIC_RAM_SIZE                    (0x4000 - ASIC_RAM_BASE_ADDRESS)

#define WPC_ZC_BLANK_RESET     0x02
#define WPC_ZC_WATCHDOG_RESET  0x04
#define WPC_ZC_IRQ_ENABLE      0x10
#define WPC_ZC_IRQ_CLEAR       0x80
#define WPC_FIRQ_CLEAR_BIT     0x80



void ASICInit();
void ASICRelease();
void ASICReset();

int ASICGetWDExpired();
bool ASICGetWDReset();
void ASICClearWDReset();

void ASICSetZeroCrossFlag();
void ASICSetCabinetInput(uint8_t value);
void ASICSetSwitchInput(int switchNr, int optionalValue);
void ASICSetFliptronicsInput(uint8_t value, int optionalValue);
void ASICFirqSourceDmd(bool fromDmd);
void ASICToggleMidnightMadnessMode();
void ASICSetDipSwitchByte(uint8_t dipSwitch);
uint8_t ASICGetDipSwitchByte();
bool ASICIRQTimerEnabled();
void ASICSetCurrentTimeDate(uint16_t year, uint8_t month, uint8_t day, uint8_t DOW, uint8_t hours, uint8_t minutes);
uint16_t ASICGetDateTimeMemoryOffset();
void ASICSetDateTimeMemoryPointer(uint8_t *dataTimeBase);
bool ASICGetBlanking();

void ASICExecuteCycle(int ticksExecuted);
bool ASICIsMemoryProtectionEnabled();
uint8_t ASICGetRomBank();
    
void ASICWrite(uint16_t offset, uint8_t value);
uint8_t ASICRead(uint16_t offset);

#endif // CPU_BOARD_ASIC_H