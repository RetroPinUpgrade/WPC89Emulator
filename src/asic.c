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



// Lookup table for xformCol
static const uint8_t xformCol_LUT[256] = {
    0x00, 0x08, 0x04, 0x0C, 0x02, 0x0A, 0x06, 0x0E, 
    0x01, 0x09, 0x05, 0x0D, 0x03, 0x0B, 0x07, 0x0F, 
    0x80, 0x88, 0x84, 0x8C, 0x82, 0x8A, 0x86, 0x8E, 
    0x81, 0x89, 0x85, 0x8D, 0x83, 0x8B, 0x87, 0x8F, 
    0x40, 0x48, 0x44, 0x4C, 0x42, 0x4A, 0x46, 0x4E, 
    0x41, 0x49, 0x45, 0x4D, 0x43, 0x4B, 0x47, 0x4F, 
    0xC0, 0xC8, 0xC4, 0xCC, 0xC2, 0xCA, 0xC6, 0xCE, 
    0xC1, 0xC9, 0xC5, 0xCD, 0xC3, 0xCB, 0xC7, 0xCF, 
    0x20, 0x28, 0x24, 0x2C, 0x22, 0x2A, 0x26, 0x2E, 
    0x21, 0x29, 0x25, 0x2D, 0x23, 0x2B, 0x27, 0x2F, 
    0xA0, 0xA8, 0xA4, 0xAC, 0xA2, 0xAA, 0xA6, 0xAE, 
    0xA1, 0xA9, 0xA5, 0xAD, 0xA3, 0xAB, 0xA7, 0xAF, 
    0x60, 0x68, 0x64, 0x6C, 0x62, 0x6A, 0x66, 0x6E, 
    0x61, 0x69, 0x65, 0x6D, 0x63, 0x6B, 0x67, 0x6F, 
    0xE0, 0xE8, 0xE4, 0xEC, 0xE2, 0xEA, 0xE6, 0xEE, 
    0xE1, 0xE9, 0xE5, 0xED, 0xE3, 0xEB, 0xE7, 0xEF, 
    0x10, 0x18, 0x14, 0x1C, 0x12, 0x1A, 0x16, 0x1E, 
    0x11, 0x19, 0x15, 0x1D, 0x13, 0x1B, 0x17, 0x1F, 
    0x90, 0x98, 0x94, 0x9C, 0x92, 0x9A, 0x96, 0x9E, 
    0x91, 0x99, 0x95, 0x9D, 0x93, 0x9B, 0x97, 0x9F, 
    0x50, 0x58, 0x54, 0x5C, 0x52, 0x5A, 0x56, 0x5E, 
    0x51, 0x59, 0x55, 0x5D, 0x53, 0x5B, 0x57, 0x5F, 
    0xD0, 0xD8, 0xD4, 0xDC, 0xD2, 0xDA, 0xD6, 0xDE, 
    0xD1, 0xD9, 0xD5, 0xDD, 0xD3, 0xDB, 0xD7, 0xDF, 
    0x30, 0x38, 0x34, 0x3C, 0x32, 0x3A, 0x36, 0x3E, 
    0x31, 0x39, 0x35, 0x3D, 0x33, 0x3B, 0x37, 0x3F, 
    0xB0, 0xB8, 0xB4, 0xBC, 0xB2, 0xBA, 0xB6, 0xBE, 
    0xB1, 0xB9, 0xB5, 0xBD, 0xB3, 0xBB, 0xB7, 0xBF, 
    0x70, 0x78, 0x74, 0x7C, 0x72, 0x7A, 0x76, 0x7E, 
    0x71, 0x79, 0x75, 0x7D, 0x73, 0x7B, 0x77, 0x7F, 
    0xF0, 0xF8, 0xF4, 0xFC, 0xF2, 0xFA, 0xF6, 0xFE, 
    0xF1, 0xF9, 0xF5, 0xFD, 0xF3, 0xFB, 0xF7, 0xFF
};

// Lookup table for xformRow
static const uint8_t xformRow_LUT[256] = {
    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 
    0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 
    0x08, 0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78, 
    0x88, 0x98, 0xA8, 0xB8, 0xC8, 0xD8, 0xE8, 0xF8, 
    0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 
    0x84, 0x94, 0xA4, 0xB4, 0xC4, 0xD4, 0xE4, 0xF4, 
    0x0C, 0x1C, 0x2C, 0x3C, 0x4C, 0x5C, 0x6C, 0x7C, 
    0x8C, 0x9C, 0xAC, 0xBC, 0xCC, 0xDC, 0xEC, 0xFC, 
    0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 
    0x82, 0x92, 0xA2, 0xB2, 0xC2, 0xD2, 0xE2, 0xF2, 
    0x0A, 0x1A, 0x2A, 0x3A, 0x4A, 0x5A, 0x6A, 0x7A, 
    0x8A, 0x9A, 0xAA, 0xBA, 0xCA, 0xDA, 0xEA, 0xFA, 
    0x06, 0x16, 0x26, 0x36, 0x46, 0x56, 0x66, 0x76, 
    0x86, 0x96, 0xA6, 0xB6, 0xC6, 0xD6, 0xE6, 0xF6, 
    0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E, 
    0x8E, 0x9E, 0xAE, 0xBE, 0xCE, 0xDE, 0xEE, 0xFE, 
    0x01, 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71, 
    0x81, 0x91, 0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1, 
    0x09, 0x19, 0x29, 0x39, 0x49, 0x59, 0x69, 0x79, 
    0x89, 0x99, 0xA9, 0xB9, 0xC9, 0xD9, 0xE9, 0xF9, 
    0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65, 0x75, 
    0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5, 
    0x0D, 0x1D, 0x2D, 0x3D, 0x4D, 0x5D, 0x6D, 0x7D, 
    0x8D, 0x9D, 0xAD, 0xBD, 0xCD, 0xDD, 0xED, 0xFD, 
    0x03, 0x13, 0x23, 0x33, 0x43, 0x53, 0x63, 0x73, 
    0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3, 0xF3, 
    0x0B, 0x1B, 0x2B, 0x3B, 0x4B, 0x5B, 0x6B, 0x7B, 
    0x8B, 0x9B, 0xAB, 0xBB, 0xCB, 0xDB, 0xEB, 0xFB, 
    0x07, 0x17, 0x27, 0x37, 0x47, 0x57, 0x67, 0x77, 
    0x87, 0x97, 0xA7, 0xB7, 0xC7, 0xD7, 0xE7, 0xF7, 
    0x0F, 0x1F, 0x2F, 0x3F, 0x4F, 0x5F, 0x6F, 0x7F, 
    0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF, 0xFF
};

// Replace function calls with LUT lookups
__attribute__((always_inline)) static inline uint8_t xformCol(uint8_t x) {
    return xformCol_LUT[x];
}

__attribute__((always_inline)) static inline uint8_t xformRow(uint8_t x) {
    return xformRow_LUT[x];
}


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
    SetBlanking(ASICBlankSignalHigh);
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
                SetDataBus(xformCol(value));
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
            DelayQuarterCycle();
            StrobeLampRow();
            SetDRLine(true);
            break;
        }

        case WPC_LAMP_COL_STROBE:
            SetDataBus(value);
            DelayQuarterCycle();
            DelayQuarterCycle();
            SetDRLine(false);
            DelayQuarterCycle();
            DelayQuarterCycle();
            StrobeLampCol();
            DelayQuarterCycle();
            DelayQuarterCycle();
            SetDRLine(true);
            break;

        case WPC_PERIPHERAL_TIMER_FIRQ_CLEAR:            
            ASIC_firqSourceDmd = false;
            break;

        case WPC_SOLENOID_HIGHPOWER_OUTPUT:
            SetDataBus(value);
            SetDRLine(false);
            StrobeSol2();
            DelayQuarterCycle();
            SetDRLine(true);
            break;
        case WPC_SOLENOID_LOWPOWER_OUTPUT:
            SetDataBus(value);
            SetDRLine(false);
            StrobeSol4();
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
            StrobeSol1();
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
                SetBlanking(ASICBlankSignalHigh);
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
            ASICCabinetSwitches = xformRow(invertedData ^ 0xFF);
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
                return xformRow(invertedData ^ 0xFF);
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
            ASICDipSwitchSetting = xformRow(invertedData ^ 0xFF);
            return ASICDipSwitchSetting;
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
