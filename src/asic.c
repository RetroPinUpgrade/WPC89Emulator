#include "mpu89.h"
#include "asic.h"

#ifdef MPU89_BUILD_FOR_COMPUTER  
#include <iostream>
#include <iomanip>
#include <cstring>
#include <chrono>
#endif

#define WPC_PROTECTED_MEMORY_UNLOCK_VALUE 0xB4

// NVRAM Offsets
#define NVRAM_CLOCK_YEAR_HI        0x1800
#define NVRAM_CLOCK_YEAR_LO        0x1801
#define NVRAM_CLOCK_MONTH          0x1802
#define NVRAM_CLOCK_DAY_OF_MONTH   0x1803
#define NVRAM_CLOCK_DAY_OF_WEEK    0x1804
#define NVRAM_CLOCK_HOUR           0x1805
#define NVRAM_CLOCK_IS_VALID       0x1806
#define NVRAM_CLOCK_CHECKSUM_TIME  0x1807
#define NVRAM_CLOCK_CHECKSUM_DATE  0x1808

#define WPC_ZC_BLANK_RESET     0x02
#define WPC_ZC_WATCHDOG_RESET  0x04
#define WPC_ZC_IRQ_ENABLE      0x10
#define WPC_ZC_IRQ_CLEAR       0x80
#define WPC_FIRQ_CLEAR_BIT     0x80

    
uint8_t ASICRomBank;
bool ASICWDReset;
bool ASICPeriodicIRQTimerEnabled;

uint8_t ASICPageMask;
uint8_t ASICRam[0x4000];
bool ASICHardwareHasSecurityPic;
    
int ASICDiagnosticLedToggleCount;
uint8_t ASICOldDiagnostigLedState;
bool ASIC_firqSourceDmd;
int ASICIrqCountGI;
uint8_t ASICZeroCrossFlag;
int ASICTicksZeroCross;
uint8_t ASICMemoryProtectionMask;

int64_t ASICMidnightMadnessMode;
bool ASICMidnightModeEnabled;
bool ASICBlankSignalHigh;
int ASICWatchdogTicks;
int ASICWatchdogExpiredCounter;
uint8_t ASICDipSwitchSetting;
uint8_t ASICCabinetSwitches;

#ifdef MPU89_BUILD_FOR_COMPUTER  
tm ASICGetTime();
#endif

void ASICInit() {
    ASICPageMask = 0x1F;
    
    // Default DipSwitch Setting (USA)
    ASICDipSwitchSetting = 0x00;
    ASICWDReset = false;
    ASICHardwareHasSecurityPic = false; // This value should be read from config or INI
}

void ASICRelease() {
}

void ASICReset() {
    ASICPeriodicIRQTimerEnabled = true;
    ASICRomBank = 0;
    ASICDiagnosticLedToggleCount = 0;
    ASICOldDiagnostigLedState = 0;
    ASIC_firqSourceDmd = false;
    ASICIrqCountGI = 0;
    ASICZeroCrossFlag = 0;
    ASICTicksZeroCross = 0;
    ASICMemoryProtectionMask = 0;

    if (ASICHardwareHasSecurityPic) {
        //securityPic->reset();
    }

#ifdef MPU89_BUILD_FOR_COMPUTER  
    ASICMidnightMadnessMode = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#endif    
    ASICMidnightModeEnabled = false;
    ASICBlankSignalHigh = true;
    ASICWatchdogTicks = WATCHDOG_ARMED_FOR_TICKS;
    ASICWatchdogExpiredCounter = 0;
    ASICCabinetSwitches = 0;
}

uint8_t ASICGetRomBank() {
    return ASICRomBank;
}

bool ASICGetWDReset() {
    return ASICWDReset;
}

void ASICClearWDReset() {
    ASICWDReset = false;
}

void ASICSetZeroCrossFlag() {
    ASICZeroCrossFlag = 0x01;
}

void ASICSetCabinetInput(uint8_t value) {
    ASICCabinetSwitches = value;
#ifdef MPU89_BUILD_FOR_COMPUTER  
    mvprintw(1, 80, "ASIC Cab Switches = %d", ASICCabinetSwitches);
#endif    
}

void ASICSetSwitchInput(int switchNr, int optionalValue) {
}

void ASICSetFliptronicsInput(uint8_t value, int optionalValue) {
}

void ASICFirqSourceDmd(bool fromDmd) {
    ASIC_firqSourceDmd = fromDmd;
}

void ASICToggleMidnightMadnessMode() {
#ifdef MPU89_BUILD_FOR_COMPUTER  
    ASICMidnightMadnessMode = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#endif    
    ASICMidnightModeEnabled = !ASICMidnightModeEnabled;
}

void ASICSetDipSwitchByte(uint8_t dipSwitch) {
    ASICDipSwitchSetting = dipSwitch;
}

uint8_t ASICGetDipSwitchByte() {
    return ASICDipSwitchSetting;
}

void ASICExecuteCycle(int ticksExecuted) {
    ASICTicksZeroCross += ticksExecuted;
    if (ASICTicksZeroCross >= CALL_ZEROCLEAR_AFTER_TICKS) {
        ASICTicksZeroCross -= CALL_ZEROCLEAR_AFTER_TICKS;
        ASICSetZeroCrossFlag();
    }

    ASICWatchdogTicks -= ticksExecuted;
    if (ASICWatchdogTicks < 0) {
//        DEBUG_LOG("WATCHDOG_EXPIRED %d", ASICWatchdogTicks);
        ASICWatchdogTicks = WATCHDOG_ARMED_FOR_TICKS;
        ASICWatchdogExpiredCounter++;
    }

    //outputLampMatrix->executeCycle(ticksExecuted);
    //outputSolenoidMatrix->executeCycle(ticksExecuted);
}

bool ASICIsMemoryProtectionEnabled() {
    return ASICRam[WPC_RAM_LOCK] == WPC_PROTECTED_MEMORY_UNLOCK_VALUE;
}

#ifdef MPU89_BUILD_FOR_COMPUTER  
tm ASICGetTime() {
    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    tm* timePtr = localtime(&now);
    return *timePtr;
}
#endif

void ASICWrite(uint16_t offset, uint8_t value) {
    ASICRam[offset] = value;

    switch (offset) {
        case WPC_RAM_LOCK:
        case WPC_RAM_BANK:
        case WPC_CLK_HOURS_DAYS:
        case WPC_CLK_MINS:
        case WPC_SHIFTADDRH:
        case WPC_SHIFTADDRL:
        case WPC_SHIFTBIT:
        case WPC_SHIFTBIT2:
        case WPC_ROM_LOCK:
        case WPC_EXTBOARD1:
        case WPC_EXTBOARD2:
        case WPC_EXTBOARD3:
            break;

        case WPC95_FLIPPER_COIL_OUTPUT:
//            outputSolenoidMatrix->writeFliptronic((value) & 0xFF);
// Fliptronics board cares about WDEN going low
            break;

        case WPC95_FLIPPER_SWITCH_INPUT:
        case WPC_FLIPTRONICS_FLIPPER_PORT_A:
//            outputSolenoidMatrix->writeFliptronic((~value) & 0xFF);
// Fliptronics board cares about WDEN going low
            break;

        case WPC_RAM_LOCKSIZE:
            if (ASICIsMemoryProtectionEnabled()) {
//                memoryProtectionMask = MemoryProtection::getMemoryProtectionMask(value);
            }
            break;

        case WPC_SWITCH_COL_SELECT:
            if (ASICHardwareHasSecurityPic) {
                return;
            } else {
                SetDataBus(value);
                __NOP(); // delay just to account for possible ringing
                __NOP();
                __NOP();
                StrobeSwitchColLatch();
                return;
            }

        case WPC_GI_TRIAC:
            SetDataBus(value);
            SetDRLine(false);
            StrobeTriac();
            DelayQuarterCycle();
            SetDRLine(true);
            break;

        case WPC_LAMP_ROW_OUTPUT: {
            SetDataBus(value);
            SetDRLine(false);
            StrobeLampRow();
            DelayQuarterCycle();
            SetDRLine(true);
            break;
        }

        case WPC_LAMP_COL_STROBE:
            SetDataBus(value);
            SetDRLine(false);
            StrobeLampCol();
            DelayQuarterCycle();
            SetDRLine(true);
            break;

        case WPC_PERIPHERAL_TIMER_FIRQ_CLEAR:
            ASIC_firqSourceDmd = false;
            break;

        case WPC_SOLENOID_HIGHPOWER_OUTPUT:
            SetDataBus(value);
            SetDRLine(false);
            StrobeSol1();
            DelayQuarterCycle();
            SetDRLine(true);
            break;
        case WPC_SOLENOID_LOWPOWER_OUTPUT:
            SetDataBus(value);
            SetDRLine(false);
            StrobeSol2();
            DelayQuarterCycle();
            SetDRLine(true);
            break;
        case WPC_SOLENOID_FLASH1_OUTPUT:
            SetDataBus(value);
            SetDRLine(false);
            StrobeSol3();
            DelayQuarterCycle();
            SetDRLine(true);
            break;
        case WPC_SOLENOID_GEN_OUTPUT:
            SetDataBus(value);
            SetDRLine(false);
            StrobeSol4();
            DelayQuarterCycle();
            SetDRLine(true);
            break;

        case WPC_LEDS:
            if (value != ASICOldDiagnostigLedState) {
                ASICDiagnosticLedToggleCount++;
                ASICOldDiagnostigLedState = value;
            }
            break;

        case WPC_ROM_BANK: {
            uint8_t bank = value & ASICPageMask;
            ASICRomBank = bank;
            break;
        }

        case WPC_ZEROCROSS_IRQ_CLEAR: {
            if (value & WPC_ZC_WATCHDOG_RESET) {
                ASICWatchdogTicks = WATCHDOG_ARMED_FOR_TICKS;
                ASICWDReset = true;
            }

            if (ASICBlankSignalHigh && (value & WPC_ZC_BLANK_RESET)) {
                ASICBlankSignalHigh = false;
            }

            if (value & WPC_ZC_IRQ_CLEAR) {
                ASICIrqCountGI++;
            }

            bool timerEnabled = (value & WPC_ZC_IRQ_ENABLE)?true:false;
            if (timerEnabled != ASICPeriodicIRQTimerEnabled) {
                ASICPeriodicIRQTimerEnabled = timerEnabled;
                ASICPeriodicIRQTimerEnabled = true;
#ifdef MPU89_BUILD_FOR_COMPUTER  
                mvprintw(0, 80, "ASIC IRQ flags = %d", value);
#endif                
            }
            break;
        }

        default:
            break;
    }
}

uint8_t ASICRead(uint16_t offset) {
    uint8_t temp = 0;
    
    switch (offset) {
        case WPC_LEDS:
        case WPC_RAM_BANK:
        case WPC_ROM_LOCK:
        case WPC_EXTBOARD1:
        case WPC_EXTBOARD2:
        case WPC_EXTBOARD3:
            return ASICRam[offset];

        case WPC95_FLIPPER_COIL_OUTPUT:
        case WPC95_FLIPPER_SWITCH_INPUT:
        case WPC_FLIPTRONICS_FLIPPER_PORT_A:
// Fliptronics needs WDEN to go low        
            return 0;        
//            return inputSwitchMatrix->getFliptronicsKeys();

        case WPC_RAM_LOCK:
        case WPC_RAM_LOCKSIZE:
            return ASICRam[offset];

        case WPC_SWITCH_CABINET_INPUT: {
            SetDirectSwitchRowLine(false); // turn on U15
            SetDataBusDirection(false); // set data lines to Read
            DelayQuarterCycle();
            uint8_t invertedData = ReadDataBus();
            SetDirectSwitchRowLine(true); // turn off U15
            SetDataBusDirection(true); // set data lines to Write
            ASICCabinetSwitches = invertedData ^ 0xFF;            
            return ASICCabinetSwitches;
        }

        case WPC_ROM_BANK:
            return ASICRam[offset] & ASICPageMask;

        case WPC_SWITCH_ROW_SELECT:
            if (ASICHardwareHasSecurityPic) {
                return 0;
            } else {
                SetSwitchRowLine(false); // turn on U13
                SetDataBusDirection(false); // set data lines to Read
                DelayQuarterCycle();
                uint8_t invertedData = ReadDataBus();
                SetSwitchRowLine(true); // turn off U13
                SetDataBusDirection(true); // set data lines to Write
                return invertedData ^ 0xFF;
            }
            break;

        case WPC_SHIFTADDRH:
            temp = (ASICRam[WPC_SHIFTADDRH] +
                    ((ASICRam[WPC_SHIFTADDRL] + (ASICRam[WPC_SHIFTBIT] >> 3)) >> 8)
                   ) & 0xFF;
            return temp;
            
        case WPC_SHIFTADDRL:
            temp = (ASICRam[WPC_SHIFTADDRL] + (ASICRam[WPC_SHIFTBIT] >> 3)) & 0xFF;
            return temp;
            
        case WPC_SHIFTBIT:
        case WPC_SHIFTBIT2:
            return 1 << (ASICRam[offset] & 0x07);

        case WPC_CLK_HOURS_DAYS: {
#ifdef MPU89_BUILD_FOR_COMPUTER              
            tm t = ASICGetTime();
            uint16_t checksum = 0;
            
            int year = t.tm_year + 1900;
            ASICRam[NVRAM_CLOCK_YEAR_HI] = year >> 8;
            checksum += ASICRam[NVRAM_CLOCK_YEAR_HI];
            
            ASICRam[NVRAM_CLOCK_YEAR_LO] = year & 0xFF;
            checksum += ASICRam[NVRAM_CLOCK_YEAR_LO];
            
            ASICRam[NVRAM_CLOCK_MONTH] = t.tm_mon + 1;
            checksum += ASICRam[NVRAM_CLOCK_MONTH];
            
            ASICRam[NVRAM_CLOCK_DAY_OF_MONTH] = t.tm_mday;
            checksum += ASICRam[NVRAM_CLOCK_DAY_OF_MONTH];
            
            ASICRam[NVRAM_CLOCK_DAY_OF_WEEK] = t.tm_wday + 1;
            checksum += ASICRam[NVRAM_CLOCK_DAY_OF_WEEK];
            
            ASICRam[NVRAM_CLOCK_HOUR] = 0;
            checksum += ASICRam[NVRAM_CLOCK_HOUR];
            
            ASICRam[NVRAM_CLOCK_IS_VALID] = 1;
            checksum += ASICRam[NVRAM_CLOCK_IS_VALID];
            
            checksum = 0xFFFF - checksum;
            ASICRam[NVRAM_CLOCK_CHECKSUM_TIME] = checksum >> 8;
            ASICRam[NVRAM_CLOCK_CHECKSUM_DATE] = checksum & 0xFF;
            
            return t.tm_hour;
#else
            return 0;
#endif            
        }

        case WPC_CLK_MINS: {
#ifdef MPU89_BUILD_FOR_COMPUTER  
            tm t = ASICGetTime();
            return t.tm_min;
#else
            return 0;
#endif            
        }

        case WPC_SW_JUMPER_INPUT: {
            SetJumperSwitchRowLine(false); // turn on U11
            SetDataBusDirection(false); // set data lines to Read
            DelayQuarterCycle();
            uint8_t invertedData = ReadDataBus();
            SetJumperSwitchRowLine(true); // turn off U11
            SetDataBusDirection(true); // set data lines to Write
            ASICDipSwitchSetting = invertedData ^ 0xFF;            
            //return ASICDipSwitchSetting;
            return 0;
        }

        case WPC_ZEROCROSS_IRQ_CLEAR:
            if (ASICZeroCrossFlag) {
                ASICIrqCountGI = 0;
            }
            temp = (ASICZeroCrossFlag << 7) | (ASICRam[offset] & 0x7F);
            ASICZeroCrossFlag = 0;
            return temp;

        case WPC_PERIPHERAL_TIMER_FIRQ_CLEAR:
            return ASIC_firqSourceDmd ? 0x00 : WPC_FIRQ_CLEAR_BIT;

        default:
//            DEBUG_LOG("R_NOT_IMPLEMENTED 0x%X", offset);
            return ASICRam[offset];
    }
}

int ASICGetWDExpired() {
    return ASICWatchdogExpiredCounter;
}
