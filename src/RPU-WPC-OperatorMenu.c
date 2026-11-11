#include <stdio.h>
#include <string.h>
#include "mpu89.h"
#include "asic.h"
#include "RPU-WPC-OperatorMenu.h"
#include "RPU-WPC-Display.h"
#include "GameStateAttributes.h"
#include "gd32f4xx.h"
#include "HPSDCardReader.h"
#include "SecurityPIC.h"
#include "HPSoundCard.h"

uint8_t RPUWPCMenuPage = 0;
uint8_t RPUWPCLastMenuPage = 0xFF;
uint8_t RPUWPCSubMenuPage = 0;
uint8_t RPUWPCLastSubMenuPage = 0xFF;
uint8_t RPUWPCVolumeChange = 1;
uint8_t RPUWPCLastCabinetInput = 0;

uint32_t RPUWPCLastCabinetInputTicks = 0;
uint32_t RPUWPCMenuStartTicks = 0;
uint32_t RPUWPCMenuPageChangeTicks = 0;
uint32_t RPUWPCSettingChangeStartTicks = 0;

static uint8_t ROMFlashResult = 0xFF;
static uint16_t TotalRAMAttributes = 0;
static uint16_t CurrentRAMAttribute = 0;
static uint16_t LastRAMAttribute = 0xFFFF;
static uint16_t NumFilesOnSDCard = 0;
int16_t NewROMNum = 0;
int16_t NewGameStateNum = 0;
uint8_t GameStateNamePickerMode = 0;
static int8_t SettingsFileOpResult = -1; // -1 = None, 0 = Error, 1 = Success

static char SDCardFileList[64][13];

uint32_t RPUWPCCurrentTicks = 0;
bool RPUWPCSubItemChanged = false;
bool RPUWPC_ROMChanged = false;
bool RPUWPC_RAMChanged = false;
bool RPUWPCMenuAwaitingRelease = false;

// System stats
extern uint8_t HPDisplayBrightness;
extern uint8_t HPDisplayRefresh;
extern uint8_t HPDisplayCanStoreRefresh;
uint8_t LastBrightness;
uint8_t GetDisplayDMASpeed(void);
void SetDisplayDMASpeed(uint8_t dmaSpeed);
uint8_t GetDMDControllerRefreshSpeed();
void SetDMDControllerRefreshSpeed(uint8_t refreshSpeed);
void SetDMDControllerBrightness(uint8_t newBrightness);

// GI Editor State
uint8_t GIEditorMode = 0;
uint8_t GISelectedString = 0;
extern uint8_t GIBulbTypes[5];
extern uint8_t GIMaxGameBrightness[5];
extern uint8_t GIMaxAttractBrightness[5];
extern uint8_t ACFrequency; 

// Sound Card Editor State
uint8_t SoundCardEditorMode = 0; // 0=Nav, 1=Vol, 2=Bal
uint32_t SoundCardRequestTicks = 0;
bool SoundCardDataValid = false;
bool SoundCardErrorShown = false;
static uint8_t synthVol, synthBal, DACVol, DACBal, CVSDVol, CVSDBal;
int8_t SC_Vol, SC_Bal;
bool SoundCardWaitingForResponse;
static uint8_t LastSoundCardFlashCode = 0xFF;
void SetRelativeVolumeAndBalance(   uint8_t synthVol, uint8_t synthBal,
                                    uint8_t DACVol, uint8_t DACBal,
                                    uint8_t CVSDVol, uint8_t CVSDBal,
                                    uint8_t sendToSoundCard );
void GetRelativeVolumeAndBalance(   uint8_t *synthVol, uint8_t *synthBal, 
                                    uint8_t *DACVol, uint8_t *DACBal, 
                                    uint8_t *CVSDVol, uint8_t *CVSDBal );


// Menu pages
// 0 - Landing
// 1 - Settings
// 2 - ROM
// 3 - General Illumination
// 4 - Sound Card
// 5 - Display
// 6 - Information
// 7 - Tests

#define RPU_WPC_MENU_LANDING_PAGE           0
#define RPU_WPC_MENU_GAME_STATE             1
#define RPU_WPC_MENU_ROM                    2
#define RPU_WPC_MENU_GI                     3
#define RPU_WPC_MENU_SOUND_CARD             4
#define RPU_WPC_MENU_DISPLAY                5
#define RPU_WPC_MENU_INFORMATION            6
#define RPU_WPC_MENU_TESTS                  7

#define RPU_WPC_CABINET_ESCAPE_BUTTON       0x10
#define RPU_WPC_CABINET_MINUS_BUTTON        0x20
#define RPU_WPC_CABINET_PLUS_BUTTON         0x40
#define RPU_WPC_CABINET_ENTER_BUTTON        0x80

#define CABINET_BUTTON_DEBOUNCE_TICKS       200000U
#define RPU_WPC_TICKS_PER_SECOND            2000000U

// Arrow Timing Parameters
#define RPU_WPC_ARROW_WAIT_DURATION_SEC     5
#define RPU_WPC_ARROW_WAIT_TICKS            (RPU_WPC_ARROW_WAIT_DURATION_SEC * RPU_WPC_TICKS_PER_SECOND)
#define RPU_WPC_ARROW_SOLID_TICKS           (1 * RPU_WPC_TICKS_PER_SECOND) // Solid for 1 sec
#define RPU_WPC_ARROW_FLASH_HALF_CYCLE      (RPU_WPC_TICKS_PER_SECOND / 6) // 3 Hz flash (6 toggles per sec)

#define RPU_WPC_DISPLAY_BRIGHTNESS_CHECK_TIME   (RPU_WPC_TICKS_PER_SECOND/2)

// animation directions
// -1 / 1 = left / right
// -2 / 2 = up / down 
static int32_t AnimationDirection = 1;

void RPU_WPC_SetupPorts() {
    // Turn off lamp rows & columns
    SetLampRow(0);
    SetLampCol(0);

    // Turn off all solenoids and flashers
    SetSol1(0x00);
    SetSol2(0x00);
    SetSol3(0x00);
    SetSol4(0x00);
}

bool RPU_WPC_CheckForMenuRequest(bool readFromPort) {
    // If the operator presses both ESCAPE and + during
    // boot, we'll assume they want to enter the
    // service menu
    byte cabInput = 0;
    cabInput = ASICReadUnfilteredCabinetSwitches();
    if ((cabInput & 0x50)==0x50) {
        return true;
    }
    return false;
}

void RPU_WPC_MenuStart() {
    RPUWPCMenuPage = 0;
    RPUWPCLastMenuPage = 0xFF;
    RPUWPCSubMenuPage = 0;
    RPUWPCLastSubMenuPage = 0xFF;
    RPUWPCVolumeChange = 1;
    RPUWPCLastCabinetInput = 0;
    
    // Lock out navigation upon entry
    RPUWPCMenuAwaitingRelease = true;
    
    RPUWPCLastCabinetInputTicks = 0;
    RPUWPCMenuStartTicks = 0;
    RPUWPCMenuPageChangeTicks = 0;
    RPUWPCSettingChangeStartTicks = 0;
    
    ROMFlashResult = 0xFF;
    TotalRAMAttributes = 0;
    CurrentRAMAttribute = 0;
    LastRAMAttribute = 0xFFFF;
    NumFilesOnSDCard = 0;
    GameStateNamePickerMode = 0;

    GIEditorMode = 0;
    GISelectedString = 0;
    SoundCardEditorMode = 0;
    
    RPUWPC_ROMChanged = false;
    AnimationDirection = 1;
    LastBrightness = HPDisplayBrightness;
    RPUWPCMenuPageChangeTicks = 0;

    NewGameStateNum = 0;

    RPU_WPC_SetupPorts();
    RPU_WPC_DisplayInit();
}

void ChangeRelativeVolume(int volToChange, int changeAmount) {
    if (volToChange==0 || volToChange>3) return;
    uint8_t incomingVol = 0xFF;
    incomingVol = (RTC_BKP9>>(8*volToChange)) & 0x000000FF;

    uint8_t outgoingVol = (uint8_t)((int)incomingVol + changeAmount);
    if (outgoingVol<5) outgoingVol = 100;
    if (outgoingVol>100) outgoingVol = 5;

    RTC_BKP9 &= ~(0xFF<<(8*volToChange));
    RTC_BKP9 |= (outgoingVol<<(8*volToChange));
}

void FlashProgressCallback(uint32_t currentBytes, uint32_t totalBytes, uint8_t isErasing) {
    char buf[128];
    RPU_WPC_DisplaySetDrawTarget(0);
    RPU_WPC_DisplayClear();
    RPU_WPC_DrawRectangleDotted(0, false, MAX_BRIGHTNESS_COLOR);

    int progress = (currentBytes*100/totalBytes);

    if (isErasing) {
        snprintf(buf, 128, "   ROM: ERASING");
        RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DrawRectangleXY(6, 22, 115, 7, false, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DrawRectangleXY(6, 22, 115*progress/100, 7, true, MAX_BRIGHTNESS_COLOR);
    } else {
        snprintf(buf, 128, "   ROM: WRITING");
        RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DrawRectangleXY(6, 22, 115, 7, false, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DrawRectangleXY(6, 22, 115*progress/100, 7, true, MAX_BRIGHTNESS_COLOR);
    }

    snprintf(buf, 128, "%.19s", SDCardFileList[NewROMNum]); 
    RPU_WPC_DrawTextToScratchXY((const char *)buf, 6, 12, MAX_BRIGHTNESS_COLOR);

    // Commit statically, no alternative target needed during a blocking operation
    RPU_WPC_DisplaySetAlternateMode(0, false);
    RPU_WPC_DisplayUpdateImmediate();
}


void PopulateGameStateDescription(int16_t descriptionNum, char *buf) {
    switch (descriptionNum) {
        case 0: snprintf(buf, 32, "TOURNEY"); break;
        case 1: snprintf(buf, 32, "GUEST"); break;
        case 2: snprintf(buf, 32, "FREEPLAY"); break;
        case 3: snprintf(buf, 32, "EASY"); break;
        case 4: snprintf(buf, 32, "MEDIUM"); break;
        case 5: snprintf(buf, 32, "HARD"); break;
        case 6: snprintf(buf, 32, "HOME1"); break;
        case 7: snprintf(buf, 32, "HOME2"); break;
        case 8: snprintf(buf, 32, "HOME3"); break;
        case 9: snprintf(buf, 32, "CUSTOM1"); break;
        case 10: snprintf(buf, 32, "CUSTOM2"); break;
        default: snprintf(buf, 32, "unknown"); break;
    }
}

void UpdateSoundCardVolume() {
    if (RPUWPCSubMenuPage == 1) {
        synthVol = SC_Vol;
        synthBal = SC_Bal;
    } else if (RPUWPCSubMenuPage == 2) {
        DACVol = SC_Vol;
        DACBal = SC_Bal;
    } else if (RPUWPCSubMenuPage == 3) {
        CVSDVol = SC_Vol;
        CVSDBal = SC_Bal;
    }
    SetRelativeVolumeAndBalance(synthVol, synthBal, DACVol, DACBal, CVSDVol, CVSDBal, 1);    
}


void RPU_WPC_HandleAdjustment(uint8_t cabinetInput) {
        
    if (RPUWPCMenuPage==RPU_WPC_MENU_GAME_STATE) {
        if (!GameStateNamePickerMode) {
            if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON || cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
                SettingsFileOpResult = -1; // Clear messages on nav
                RPUWPCSubMenuPage = (RPUWPCSubMenuPage == 1) ? 2 : 1;
                AnimationDirection = (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) ? -1 : 1;
            } else if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
                SettingsFileOpResult = -1; // Clear messages on enter
                // Only allow entering picker mode if there are actually files to load
                if (RPUWPCSubMenuPage == 1 && NumFilesOnSDCard == 0) {
                    // Do nothing
                } else {
                    GameStateNamePickerMode = 1;
                    RPUWPCSubItemChanged = true;
                }
            }
        } else {
            if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) {
                NewGameStateNum += 1;
                if (NumFilesOnSDCard > 0 && NewGameStateNum >= NumFilesOnSDCard) NewGameStateNum = 0;
                RPUWPCSubItemChanged = true;
            } else if (cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
                NewGameStateNum -= 1;
                if (NewGameStateNum < 0) {
                    NewGameStateNum = (NumFilesOnSDCard > 0) ? (NumFilesOnSDCard - 1) : 0;
                }
                RPUWPCSubItemChanged = true;
            } else if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
                if (RPUWPCSubMenuPage == 1) {
                    
                    // LOAD MEMORY ACTION
                    if (NumFilesOnSDCard > 0) {
                        const char *romName = HPSDGetROMName();
                        if (romName != NULL && romName[0] != '\0') {
                            
                            // Get base ROM length (strip .ROM if present)
                            size_t baseLen = strlen(romName);
                            if (baseLen >= 4) {
                                const char* ext = &romName[baseLen - 4];
                                if (ext[0] == '.' && (ext[1] == 'R' || ext[1] == 'r') && 
                                    (ext[2] == 'O' || ext[2] == 'o') && (ext[3] == 'M' || ext[3] == 'm')) {
                                    baseLen -= 4;
                                }
                            }
                            
                            // Reconstruct the full filename
                            char loadFilename[64];
                            snprintf(loadFilename, sizeof(loadFilename), "%.*s-%s.RAM", (int)baseLen, romName, SDCardFileList[NewGameStateNum]);
                            
                            bool success = HPSDLoadGameState(loadFilename);
                            if (success) {
                                RPUWPC_RAMChanged = true;
                                SettingsFileOpResult = 1;
                            } else {
                                SettingsFileOpResult = 0;
                            }
                            
                            // Kick out of picker mode to stop flashing
                            GameStateNamePickerMode = 0; 
                        }
                    }
                } else if (RPUWPCSubMenuPage == 2) {
                    
                    // SAVE MEMORY ACTION
                    if (NewGameStateNum<11) {
                        char descBuf[32];
                        PopulateGameStateDescription(NewGameStateNum, descBuf);
                        bool success = HPSDSaveGameState((const char *)descBuf);
                        
                        if (success) {
                            SettingsFileOpResult = 1;
                        } else {
                            SettingsFileOpResult = 0;
                        }
                        
                        // Kick out of picker mode to stop flashing
                        GameStateNamePickerMode = 0; 
                    }
                }
                RPUWPCSubItemChanged = true;
            } else if (cabinetInput&RPU_WPC_CABINET_ESCAPE_BUTTON) {
                GameStateNamePickerMode = 0;
                SettingsFileOpResult = -1;
                RPUWPCSubItemChanged = true;
            }
        }
    
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_ROM) {
        if (RPUWPCSubMenuPage == 2 && (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON || cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON)) {
            AnimationDirection = (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) ? -1 : 1;
            if (NumFilesOnSDCard) {
                NewROMNum -= AnimationDirection;
                if (NewROMNum>=NumFilesOnSDCard) NewROMNum = 0;
                if (NewROMNum<0) NewROMNum = NumFilesOnSDCard - 1;
                RPUWPCSubItemChanged = true;
            }
        } else if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON && RPUWPCSubMenuPage == 1) {
            RPUWPCSubMenuPage = 2;
            RPUWPCSubItemChanged = true;
        } else if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON && RPUWPCSubMenuPage == 2) {
            if (NumFilesOnSDCard) {
                // Install new ROM action
                RPUWPCSettingChangeStartTicks = RPUWPCCurrentTicks;
                ROMFlashResult = 0;
                // Assuming first file for placeholder
                uint32_t newFileSize = HPSDFlashRomFromSD((const char *)SDCardFileList[NewROMNum], FlashProgressCallback);
                if (newFileSize) {
                    HPSDUpdateRomMetadata((const char *)SDCardFileList[NewROMNum], newFileSize);
                    ROMFlashResult = 1;
                    RPUWPC_ROMChanged = true;
                }
                RPUWPCSubMenuPage = 1;
                AnimationDirection = 2;
                RPUWPCSubItemChanged = true;
            }
        } else if (cabinetInput&RPU_WPC_CABINET_ESCAPE_BUTTON) {
            // Handled elsewhere
        }
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_GI) {
        if (!GIEditorMode) {
            if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON || cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
                RPUWPCSubMenuPage += (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) ? 1 : -1;
                if (RPUWPCSubMenuPage<1) RPUWPCSubMenuPage = 3;
                if (RPUWPCSubMenuPage>3) RPUWPCSubMenuPage = 1;
                AnimationDirection = (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) ? -1 : 1;
                RPUWPCSubItemChanged = true;
            } else if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
                GIEditorMode = 1;
                GISelectedString = 0;
                RPUWPCSubItemChanged = true;
            }
        } else {
            if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
                GISelectedString++;
                if (GISelectedString > 4) GISelectedString = 0;
                RPUWPCSubItemChanged = true;
            } else if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) {
                if (RPUWPCSubMenuPage == 1) {
                    GIBulbTypes[GISelectedString] = !GIBulbTypes[GISelectedString];
                } else if (RPUWPCSubMenuPage == 2) {
                    if (GIMaxGameBrightness[GISelectedString] < 8) GIMaxGameBrightness[GISelectedString]++;
                } else if (RPUWPCSubMenuPage == 3) {
                    if (GIMaxAttractBrightness[GISelectedString] < 8) GIMaxAttractBrightness[GISelectedString]++;
                }
                RPUWPCSubItemChanged = true;
            } else if (cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
                if (RPUWPCSubMenuPage == 1) {
                    GIBulbTypes[GISelectedString] = !GIBulbTypes[GISelectedString];
                } else if (RPUWPCSubMenuPage == 2) {
                    if (GIMaxGameBrightness[GISelectedString] > 1) GIMaxGameBrightness[GISelectedString]--;
                } else if (RPUWPCSubMenuPage == 3) {
                    if (GIMaxAttractBrightness[GISelectedString] > 1) GIMaxAttractBrightness[GISelectedString]--;
                }
                RPUWPCSubItemChanged = true;
            }
        }
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_SOUND_CARD) {
        if (!SoundCardEditorMode) {
            if (cabinetInput & RPU_WPC_CABINET_PLUS_BUTTON) {
                RPUWPCSubMenuPage += 1;
                if (RPUWPCSubMenuPage > 4) RPUWPCSubMenuPage = 1;
                AnimationDirection = -1;
                RPUWPCSubItemChanged = true;
            } else if (cabinetInput & RPU_WPC_CABINET_MINUS_BUTTON) {
                RPUWPCSubMenuPage -= 1;
                if (RPUWPCSubMenuPage == 0) RPUWPCSubMenuPage = 4;
                AnimationDirection = 1;
                RPUWPCSubItemChanged = true;
            } else if (cabinetInput & RPU_WPC_CABINET_ENTER_BUTTON) {
                // Only enter edit mode for Synth, Perc, and Voice (Pages 1-3)
                if (RPUWPCSubMenuPage >= 1 && RPUWPCSubMenuPage <= 3) {
                    SoundCardEditorMode = 1; // Start by editing Volume
                    RPUWPCSubItemChanged = true;
                    switch (RPUWPCSubMenuPage) {
                        case 1: SC_Vol = synthVol; SC_Bal = synthBal; break;
                        case 2: SC_Vol = DACVol; SC_Bal = DACBal; break;
                        case 3: SC_Vol = CVSDVol; SC_Bal = CVSDBal; break;
                    }
                } else if (SoundCardDataValid && RPUWPCSubMenuPage == 4) { 
                    SoundCardEditorMode = 1; // Start ROM browsing
                    NewROMNum = 0;
                    HPSoundCardRequestNumROMs();
                    HPSoundCardRequestROMName(NewROMNum); 
                    SoundCardDataValid = false;
                    SoundCardRequestTicks = RPUWPCCurrentTicks;
                    RPUWPCSubItemChanged = true;
                }
            }
       
        } else {
            if (cabinetInput & RPU_WPC_CABINET_ENTER_BUTTON) {
                if (RPUWPCSubMenuPage == 4) {
                    if (SoundCardEditorMode == 1) {
                        HPSoundCardFlashROM(NewROMNum);
                        LastSoundCardFlashCode = 0xFF; // Reset progress tracker
                        SoundCardEditorMode = 2; // Move to Flashing state
                        SoundCardDataValid = false;
                        SoundCardRequestTicks = RPUWPCCurrentTicks;
                        RPUWPCSubItemChanged = true;
                    }
                } else {
                    SoundCardEditorMode++;
                    if (SoundCardEditorMode > 2) SoundCardEditorMode = 0; // Cycle Vol -> Bal -> Done
                    RPUWPCSubItemChanged = true;
                }
            } else if ((cabinetInput & RPU_WPC_CABINET_PLUS_BUTTON) || (cabinetInput & RPU_WPC_CABINET_MINUS_BUTTON)) {
                if (RPUWPCSubMenuPage == 4) {
                    if (SoundCardEditorMode == 1) {
                        uint16_t numRoms = 0;
                        if (HPSoundCardCurNumROMs(&numRoms) && numRoms > 0) {
                            if (cabinetInput & RPU_WPC_CABINET_PLUS_BUTTON) {
                                NewROMNum++;
                                if (NewROMNum >= numRoms) NewROMNum = 0;
                            } else {
                                NewROMNum--;
                                if (NewROMNum < 0) NewROMNum = numRoms - 1;
                            }
                            HPSoundCardRequestROMName(NewROMNum);
                            SoundCardDataValid = false;                 
                            SoundCardRequestTicks = RPUWPCCurrentTicks; 
                            RPUWPCSubItemChanged = true;
                        }
                    }
                } else {
                    int change = (cabinetInput & RPU_WPC_CABINET_PLUS_BUTTON) ? 1 : -1;
                    if (SoundCardEditorMode == 1) { // Adjusting Volume
                        SC_Vol += change;
                        if (SC_Vol > 31) SC_Vol = 31;
                        if (SC_Vol < 0) SC_Vol = 0;
                    } else if (SoundCardEditorMode == 2) { // Adjusting Balance
                        SC_Bal += change;
                        if (SC_Bal > 31) SC_Bal = 31;
                        if (SC_Bal < 1) SC_Bal = 1;
                    }
                    UpdateSoundCardVolume(); // Flush the mapped L/R over UART
                    RPUWPCSubItemChanged = true;
                }
            }
        }            
       
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_DISPLAY) {
        if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) {
            if (RPUWPCSubMenuPage == 2) { // Refresh Rate adjustments
                if (HPDisplayBrightness != 0xFF) {
                    uint8_t r = GetDMDControllerRefreshSpeed();
                    if (r < 4) SetDMDControllerRefreshSpeed(r + 1);
                } else {
                    uint8_t r = GetDisplayDMASpeed();
                    if (r < 3) SetDisplayDMASpeed(r + 1);
                }
            } else if (RPUWPCSubMenuPage == 3) { // Brightness adjustments
                if (HPDisplayBrightness != 0xFF && HPDisplayBrightness <= 95) {
                    SetDMDControllerBrightness(HPDisplayBrightness + 5);
                }
            }
            RPUWPCSubItemChanged = true;
        } else if (cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
            if (RPUWPCSubMenuPage == 2) { // Refresh Rate adjustments
                if (HPDisplayBrightness != 0xFF) {
                    uint8_t r = GetDMDControllerRefreshSpeed();
                    if (r > 0) SetDMDControllerRefreshSpeed(r - 1);
                } else {
                    uint8_t r = GetDisplayDMASpeed();
                    if (r > 0) SetDisplayDMASpeed(r - 1);
                }
            } else if (RPUWPCSubMenuPage == 3) { // Brightness adjustments
                if (HPDisplayBrightness != 0xFF && HPDisplayBrightness >= 25) {
                    SetDMDControllerBrightness(HPDisplayBrightness - 5);
                }
            }
            RPUWPCSubItemChanged = true;
        } else if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
            // Toggle to brightness menu only if it is supported by hardware
            if (HPDisplayBrightness != 0xFF) {
                RPUWPCSubMenuPage += 1;
                if (RPUWPCSubMenuPage>3) RPUWPCSubMenuPage = 1;
                if (!HPDisplayCanStoreRefresh && RPUWPCSubMenuPage==2) RPUWPCSubMenuPage = 3;
            } else {
                RPUWPCSubMenuPage += 1;
                if (RPUWPCSubMenuPage>2) RPUWPCSubMenuPage = 1;
            }
            RPUWPCSubItemChanged = true;
        }
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_INFORMATION) {
    }
}


void RPU_WPC_NavigateMenu(uint8_t cabinetInput) {
    if (RPUWPCMenuPage==0) {
        // on the home page, you only have two options
        // Escape = boot, Enter = go into menus
        if (cabinetInput&RPU_WPC_CABINET_ESCAPE_BUTTON) {
            // escape button
            AnimationDirection = 2;
            RPUWPCMenuPage -= 1;
        } else if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
            // enter button
            AnimationDirection = -2;
            RPUWPCMenuPage = 1;
        }
    } else {
        if (RPUWPCSubMenuPage) {
            // We're in a sub menu
            if (cabinetInput&RPU_WPC_CABINET_ESCAPE_BUTTON) {
                // operator is leaving this option
                if (GIEditorMode) {
                    GIEditorMode = 0;
                    AnimationDirection = 2;
                    RPUWPCSubItemChanged = true;
                } else if (SoundCardEditorMode) {
                    SoundCardEditorMode = 0;
                    AnimationDirection = 2;
                    RPUWPCSubItemChanged = true;
                    if (RPUWPCSubMenuPage == 4) {
                        SoundCardDataValid = false;
                        SoundCardErrorShown = false;
                        SoundCardRequestTicks = RPUWPCCurrentTicks;
                        HPSoundCardRequestCurrentROMName();
                    }
                } else {
                    GameStateNamePickerMode = 0; 
                    SettingsFileOpResult = -1;
                    AnimationDirection = 2;
                    RPUWPCSubMenuPage = 0;        
                }
            } else {
                // make adjustment
                RPU_WPC_HandleAdjustment(cabinetInput);
            }
        } else {
            // We're at a top-level menu
            if (cabinetInput&RPU_WPC_CABINET_ESCAPE_BUTTON) {
                // operator hit escape, so go to home page
                AnimationDirection = 2;
                RPUWPCMenuPage = RPU_WPC_MENU_LANDING_PAGE;
            } else if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
                // operator hit enter, so go into sub menu 
                AnimationDirection = -2;
                RPUWPCSubMenuPage = 1;
                
                // Reset states per menu
                if (RPUWPCMenuPage==RPU_WPC_MENU_ROM) {
                    ROMFlashResult = 0xFF;
                } else if (RPUWPCMenuPage==RPU_WPC_MENU_SOUND_CARD) {
                    GetRelativeVolumeAndBalance(&synthVol, &synthBal, &DACVol, &DACBal, &CVSDVol, &CVSDBal);
                    RPUWPCVolumeChange = 1;
                    SoundCardEditorMode = 0;
                } else if (RPUWPCMenuPage==RPU_WPC_MENU_GI) {
                    GIEditorMode = 0;
                } else if (RPUWPCMenuPage==RPU_WPC_MENU_GAME_STATE) {
                    NewGameStateNum = 0;
                    SettingsFileOpResult = -1;
                }
            } else if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) {
                // operator hit +, so go to next menu
                AnimationDirection = -1;
                RPUWPCMenuPage += 1;
                if (RPUWPCMenuPage == RPU_WPC_MENU_SOUND_CARD && !HPAdvancedWAVSoundCardPresent()) {
                    RPUWPCMenuPage += 1; 
                }
                if (RPUWPCMenuPage>RPU_WPC_MENU_TESTS) RPUWPCMenuPage = RPU_WPC_MENU_GAME_STATE;
            } else if (cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
                // operator hit -, so go to previous menu
                AnimationDirection = 1;
                RPUWPCMenuPage -= 1;
                if (RPUWPCMenuPage == RPU_WPC_MENU_SOUND_CARD && !HPAdvancedWAVSoundCardPresent()) {
                    RPUWPCMenuPage -= 1; 
                }
                if (RPUWPCMenuPage==0) RPUWPCMenuPage = RPU_WPC_MENU_TESTS;
            }
        }
    }
}


void RPU_WPC_DrawSubMenuPage(bool isAlternate) {
    char buf[128];
    RPU_WPC_DisplayClear();
    RPU_WPC_DrawRectangleDotted(0, false, MAX_BRIGHTNESS_COLOR);
    

    if (RPUWPCMenuPage==RPU_WPC_MENU_GAME_STATE) {
        
        if (RPUWPCSubMenuPage == 1) {
            snprintf(buf, 128, "  SETTINGS: LOAD");
            RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
            
            if (!HPSDCardInserted()) {
                snprintf(buf, 128, "  (insert SD card)");
                RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 22, MAX_BRIGHTNESS_COLOR);
            } else {
                NumFilesOnSDCard = HPSDGetGameStateFiles(HPSDGetROMName(), SDCardFileList, 64);
                
                if (!GameStateNamePickerMode) {
                    // 1. Entry Mode (Not picking yet)
                                        
                    if (SettingsFileOpResult == 1) {
                        // Show the file in the middle (solid) if we have any
                        if (NumFilesOnSDCard > 0) {
                            if (NewGameStateNum >= NumFilesOnSDCard) NewGameStateNum = 0;
                            RPU_WPC_DrawTextToScratchXY((const char *)SDCardFileList[NewGameStateNum], 15, 13, MAX_BRIGHTNESS_COLOR);
                        }
                        RPU_WPC_DrawTextToScratchXY("  SUCCESS", 3, 22, MAX_BRIGHTNESS_COLOR);
                    } else if (SettingsFileOpResult == 0) {
                        RPU_WPC_DrawTextToScratchXY("  CARD ERROR", 3, 22, MAX_BRIGHTNESS_COLOR);
                    } else {
                        if (NumFilesOnSDCard) {
                            snprintf(buf, 128, "  (%d available)", NumFilesOnSDCard);
                        } else {
                            snprintf(buf, 128, "  (no files found)");
                        }
                        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 22, MAX_BRIGHTNESS_COLOR);
                    }
                } else {
                    // 2. Picker Mode
                    if (NumFilesOnSDCard) {
                        if (NewGameStateNum >= NumFilesOnSDCard) NewGameStateNum = 0;
                        
                        // Bottom line
                        snprintf(buf, 128, "  (%d of %d)", NewGameStateNum + 1, NumFilesOnSDCard);
                        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 22, MAX_BRIGHTNESS_COLOR);
                        
                        // Middle line (flashing file description)
                        if (!isAlternate) {
                            RPU_WPC_DrawTextToScratchXY((const char *)SDCardFileList[NewGameStateNum], 15, 13, MAX_BRIGHTNESS_COLOR);
                        }
                    }
                }
            }

        } else if (RPUWPCSubMenuPage == 2) {
            snprintf(buf, 128, "  SETTINGS: SAVE");
            RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
            
            if (!GameStateNamePickerMode) {
                // Not picking. Show current selection solid in middle, instructions at bottom
                char descBuf[32];
                PopulateGameStateDescription(NewGameStateNum, descBuf);
                
                if (SettingsFileOpResult == 1) {
                    RPU_WPC_DrawTextToScratchXY((const char *)descBuf, 15, 13, MAX_BRIGHTNESS_COLOR);
                    RPU_WPC_DrawTextToScratchXY("  SUCCESS", 3, 22, MAX_BRIGHTNESS_COLOR);
                } else if (SettingsFileOpResult == 0) {
                    RPU_WPC_DrawTextToScratchXY("  CARD ERROR", 3, 22, MAX_BRIGHTNESS_COLOR);
                } else {
                    RPU_WPC_DrawTextToScratchXY("  (enter to choose)", 3, 22, MAX_BRIGHTNESS_COLOR);
                }
                NumFilesOnSDCard = 11;
            } else {
                // Picking. Flashing middle text.
                char descBuf[32];
                PopulateGameStateDescription(NewGameStateNum, descBuf);
                if (!isAlternate) {
                    RPU_WPC_DrawTextToScratchXY((const char *)descBuf, 15, 13, MAX_BRIGHTNESS_COLOR);
                }
            }
        }
    
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_ROM) {
        snprintf(buf, 128, "         ROM");
        RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
        if (RPUWPCSubMenuPage == 1) {
            snprintf(buf, 128, "%.19s", HPSDGetROMName());
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 6, 12, MAX_BRIGHTNESS_COLOR);

            if (!HPSDCardInserted()) {
                snprintf(buf, 128, "  (insert SD card)");
                RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 22, MAX_BRIGHTNESS_COLOR);
            } else {
                NumFilesOnSDCard = HPSDCardGetFileNames(SDCardFileList, 64, "rom");
                if (NumFilesOnSDCard) {
                    if (NumFilesOnSDCard<10) snprintf(buf, 128, "  (%d ROMs available)", NumFilesOnSDCard);
                    else if (NumFilesOnSDCard<100) snprintf(buf, 128, " (%d ROMs available)", NumFilesOnSDCard);
                    else snprintf(buf, 128, "(%d ROMs available)", NumFilesOnSDCard);
                } else {
                    snprintf(buf, 128, "(no ROMs found)");
                }
                RPU_WPC_DrawTextToScratchXY((const char *)buf, 1, 22, MAX_BRIGHTNESS_COLOR);
            }

        } else if (RPUWPCSubMenuPage == 2) {
            snprintf(buf, 128, "(file %d / %d)", NewROMNum + 1, NumFilesOnSDCard);
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 6, 22, MAX_BRIGHTNESS_COLOR);

            // Only draw the file name text if this isn't the alternate (blanking) pass
            if (!isAlternate) {
                snprintf(buf, 128, "%.19s", SDCardFileList[NewROMNum]);            
                RPU_WPC_DrawTextToScratchXY((const char *)buf, 6, 12, MAX_BRIGHTNESS_COLOR);
            }
        }
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_GI) {
        if (RPUWPCSubMenuPage == 1) {
            snprintf(buf, 128, "  GI: STRING TYPE");
            RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
            snprintf(buf, 128, "  1   2   3   4   5");
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 0, 13, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(6, 23, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(30, 47, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(54, 71, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(78, 95, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(102, 119, 21, MAX_BRIGHTNESS_COLOR);

            char v[5][8];
            for(int i=0; i<5; i++) {
                if (GIEditorMode && isAlternate && i == GISelectedString) {
                    strcpy(v[i], "   ");
                } else {
                    strcpy(v[i], GIBulbTypes[i] ? "LED" : "Inc");
                }
            }
            snprintf(buf, 128, " %s %s %s %s %s", v[0], v[1], v[2], v[3], v[4]);
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 0, 23, MAX_BRIGHTNESS_COLOR);

        } else if (RPUWPCSubMenuPage == 2) {
            RPU_WPC_DrawTextToScratch((const char *)"GI: GAME MAX BRIGHT", MAX_BRIGHTNESS_COLOR);
            snprintf(buf, 128, "  1   2   3   4   5");
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 0, 13, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(6, 23, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(30, 47, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(54, 71, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(78, 95, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(102, 119, 21, MAX_BRIGHTNESS_COLOR);
            ASICSetFlipperStateOverride(true, true);
            
            char v[5][8];
            for(int i=0; i<5; i++) {
                if (GIEditorMode && isAlternate && i == GISelectedString) {
                    strcpy(v[i], "   ");
                } else {
                    snprintf(v[i], 8, "%d/8", GIMaxGameBrightness[i]);
                }
            }
            snprintf(buf, 128, " %s %s %s %s %s", v[0], v[1], v[2], v[3], v[4]);
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 0, 23, MAX_BRIGHTNESS_COLOR);
        } else if (RPUWPCSubMenuPage == 3) {
            RPU_WPC_DrawTextToScratch((const char *)"GI: ATTR MAX BRIGHT", MAX_BRIGHTNESS_COLOR);
            snprintf(buf, 128, "  1   2   3   4   5");
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 0, 13, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(6, 23, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(30, 47, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(54, 71, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(78, 95, 21, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawHorizontalLine(102, 119, 21, MAX_BRIGHTNESS_COLOR);
            ASICSetFlipperStateOverride(true, false);

            char v[5][8];
            for(int i=0; i<5; i++) {
                if (GIEditorMode && isAlternate && i == GISelectedString) {
                    strcpy(v[i], "   ");
                } else {
                    snprintf(v[i], 8, "%d/8", GIMaxAttractBrightness[i]);
                }
            }
            snprintf(buf, 128, " %s %s %s %s %s", v[0], v[1], v[2], v[3], v[4]);
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 0, 23, MAX_BRIGHTNESS_COLOR);
        }
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_SOUND_CARD) {
        if (RPUWPCSubMenuPage == 1) snprintf(buf, 128, "SOUND CARD: SYNTH");
        else if (RPUWPCSubMenuPage == 2) snprintf(buf, 128, "SOUND CARD: PERC");
        else if (RPUWPCSubMenuPage == 3) snprintf(buf, 128, "SOUND CARD: VOICE");
        else if (RPUWPCSubMenuPage == 4) snprintf(buf, 128, "SOUND CARD: ROM");
        
        RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
        
        // Draw an error overlay at the bottom if we timed out, but don't stop drawing!
        if (!SoundCardDataValid && SoundCardErrorShown) {
            RPU_WPC_DrawTextToScratchXY("COMM ERROR", 30, 23, MAX_BRIGHTNESS_COLOR);
        }
        
        if (RPUWPCSubMenuPage <= 3) {
            RPU_WPC_DrawTextToScratchXY("VOL", 5, 12, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawTextToScratchXY("BAL", 5, 21, MAX_BRIGHTNESS_COLOR);
            
            // Fetch values depending on if we are editing or just viewing
            uint8_t drawVol = SC_Vol, drawBal = SC_Bal;
            if (SoundCardEditorMode == 0) {
                if (RPUWPCSubMenuPage == 1) { drawVol = synthVol; drawBal = synthBal; }
                else if (RPUWPCSubMenuPage == 2) { drawVol = DACVol; drawBal = DACBal; }
                else if (RPUWPCSubMenuPage == 3) { drawVol = CVSDVol; drawBal = CVSDBal; }
            }
            
            // Always draw the Volume Bar framework
            RPU_WPC_DrawRectangleXY(30, 13, 78, 5, false, MAX_BRIGHTNESS_COLOR-(SoundCardEditorMode==1 && isAlternate?1:0));
            RPU_WPC_DrawRectangleXY(94, 12, 1, 7, false, MAX_BRIGHTNESS_COLOR);
            // Fill it using drawVol
            RPU_WPC_DrawRectangleXY(31, 14, 15+(drawVol*61)/31, 3, true, MAX_BRIGHTNESS_COLOR - 1);
            
            // Always draw the Balance Line framework
            RPU_WPC_DrawHorizontalLine(30, 108, 23, MAX_BRIGHTNESS_COLOR-(SoundCardEditorMode==2 && isAlternate?1:0));
            RPU_WPC_DrawRectangleXY(69, 20, 1, 7, false, MAX_BRIGHTNESS_COLOR); // Shift center detent to 69            
            // Map 1-31 range exactly over the 78 pixels of movement (30 to 108)
            int safeBal = drawBal;
            if (safeBal < 1) safeBal = 1; // Prevent off-screen drawing if value is uninitialized 0            
            int bx = 30 + ((safeBal - 1) * 78) / 30;             
            RPU_WPC_DrawRectangleXY(bx - 1, 21, 3, 5, true, MAX_BRIGHTNESS_COLOR - 1);
            
        } else if (RPUWPCSubMenuPage == 4) {
            if (!SoundCardDataValid && !SoundCardErrorShown && SoundCardEditorMode != 2) {
                RPU_WPC_DrawTextToScratchXY("Requesting...", 25, 16, MAX_BRIGHTNESS_COLOR);
            }
            if (SoundCardEditorMode == 0) {
                // State 0: Viewing active ROM
                snprintf(buf, 128, "%.19s", HPSoundCardGetActiveROMName());
                RPU_WPC_DrawTextToScratchXY((const char *)buf, 10, 14, MAX_BRIGHTNESS_COLOR);
                RPU_WPC_DrawTextToScratchXY("(Enter to Browse)", 3, 23, MAX_BRIGHTNESS_COLOR);
            } else if (SoundCardEditorMode == 1) {
                // State 1: Browsing ROMs
                char *romName = HPSoundCardCurROMName();
                if (romName) {
                    if (!isAlternate) { // Flashes via the Alternate draw target system
                        snprintf(buf, 128, "%.19s", romName);
                        RPU_WPC_DrawTextToScratchXY((const char *)buf, 10, 14, MAX_BRIGHTNESS_COLOR);
                    }
                } else {
                    RPU_WPC_DrawTextToScratchXY("Loading...", 10, 14, MAX_BRIGHTNESS_COLOR);
                }
                
                uint16_t numRoms = 0;
                if (HPSoundCardCurNumROMs(&numRoms) && numRoms > 0) {
                    snprintf(buf, 128, "(%d of %d)", NewROMNum + 1, numRoms);
                    RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 23, MAX_BRIGHTNESS_COLOR);
                }
            } else if (SoundCardEditorMode == 2) {
                // State 2: Flashing Progress / Result
                uint8_t code;
                if (HPSoundCardCurFlashCode(&code)) {
                    if (code >= 100) {
                        RPU_WPC_DrawTextToScratchXY("FLASH COMPLETE", 10, 12, MAX_BRIGHTNESS_COLOR);
                        char *msg = HPSoundCardCurFlashMessage();
                        if (msg) RPU_WPC_DrawTextToScratchXY(msg, 10, 21, MAX_BRIGHTNESS_COLOR);
                    } else {
                        if (!isAlternate) {
                            RPU_WPC_DrawTextToScratchXY("FLASHING...", 10, 12, MAX_BRIGHTNESS_COLOR);
                        }
                        RPU_WPC_DrawRectangleXY(6, 22, 115, 7, false, MAX_BRIGHTNESS_COLOR);
                        RPU_WPC_DrawRectangleXY(6, 22, (115 * code) / 100, 7, true, MAX_BRIGHTNESS_COLOR);
                    }
                } else {
                    char *msg = HPSoundCardCurFlashMessage();
                    if (msg) {
                        RPU_WPC_DrawTextToScratchXY("FLASH FAILED", 10, 12, MAX_BRIGHTNESS_COLOR);
                        RPU_WPC_DrawTextToScratchXY(msg, 10, 21, MAX_BRIGHTNESS_COLOR);
                    } else if (!isAlternate) {
                        RPU_WPC_DrawTextToScratchXY("FLASHING...", 10, 16, MAX_BRIGHTNESS_COLOR);
                    }
                }
            }        
        }
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_DISPLAY) {
        snprintf(buf, 128, "       DISPLAY");
        RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DrawRectangleXY(117, 3, 4, 4, true, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DrawRectangleXY(121, 3, 4, 4, true, MAX_BRIGHTNESS_COLOR-2);
        RPU_WPC_DrawRectangleXY(117, 7, 4, 4, true, MAX_BRIGHTNESS_COLOR-2);
        RPU_WPC_DrawRectangleXY(121, 7, 4, 4, true, MAX_BRIGHTNESS_COLOR-1);
        RPU_WPC_DrawRectangleXY(116, 2, 10, 10, false, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DrawRectangleXY(3, 3, 4, 4, true, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DrawRectangleXY(7, 3, 4, 4, true, MAX_BRIGHTNESS_COLOR-2);
        RPU_WPC_DrawRectangleXY(3, 7, 4, 4, true, MAX_BRIGHTNESS_COLOR-2);
        RPU_WPC_DrawRectangleXY(7, 7, 4, 4, true, MAX_BRIGHTNESS_COLOR-1);
        RPU_WPC_DrawRectangleXY(2, 2, 10, 10, false, MAX_BRIGHTNESS_COLOR);
        if (HPDisplayBrightness!=0xFF) {
            snprintf(buf, 128, "REFRESH");
            if (HPDisplayCanStoreRefresh) RPU_WPC_DrawTextToScratchXY((const char *)buf, 6, 14, MAX_BRIGHTNESS_COLOR);
                uint8_t curRefresh = GetDMDControllerRefreshSpeed();
            if (curRefresh==0) snprintf(buf, 128, "ORIGINAL");
            else if (curRefresh==1) snprintf(buf, 128, "FAST 1");
            else if (curRefresh==2) snprintf(buf, 128, "FAST 2");
            else if (curRefresh==3) snprintf(buf, 128, "MAX");
            else snprintf(buf, 128, "ULTRA");
            
            if (!(RPUWPCSubMenuPage == 2 && isAlternate)) {
                if (HPDisplayCanStoreRefresh) RPU_WPC_DrawTextToScratchXY((const char *)buf, 70, 14, MAX_BRIGHTNESS_COLOR);
            }
            
            snprintf(buf, 128, "BRIGHTNESS");
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 6, 23, MAX_BRIGHTNESS_COLOR);
            if (!(RPUWPCSubMenuPage == 3 && isAlternate)) {
                snprintf(buf, 128, "%d%%", HPDisplayBrightness);
                RPU_WPC_DrawTextToScratchXY((const char *)buf, 76, 23, MAX_BRIGHTNESS_COLOR);
            }
        } else {
            snprintf(buf, 128, "REFRESH");
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 6, 14, MAX_BRIGHTNESS_COLOR);    
            uint8_t curRefresh = GetDisplayDMASpeed();
            if (curRefresh==0) snprintf(buf, 128, "ORIGINAL");
            else if (curRefresh==1) snprintf(buf, 128, "FAST 1");
            else if (curRefresh==2) snprintf(buf, 128, "FAST 2");
            else snprintf(buf, 128, "MAX");
            
            if (!(RPUWPCSubMenuPage == 2 && isAlternate)) {
                RPU_WPC_DrawTextToScratchXY((const char *)buf, 70, 14, MAX_BRIGHTNESS_COLOR);
            }
        }
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_INFORMATION) {
        snprintf(buf, 128, "     INFORMATION");
        RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
        switch (ACFrequency) {
            case 0: snprintf(buf, 128, "AC=60Hz"); break;
            case 1: snprintf(buf, 128, "AC=50Hz"); break;
            case 3: snprintf(buf, 128, "AC=ERR"); break;
            case 2: snprintf(buf, 128, "AC=err"); break;
        }
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 9, 13, MAX_BRIGHTNESS_COLOR);
        if (SecurityPICGetLearnedID()) {
            snprintf(buf, 128, "U22=%d", SecurityPICGetLearnedID());
            RPU_WPC_DrawTextToScratchXY((const char *)buf, 69, 13, MAX_BRIGHTNESS_COLOR);
        }
        char out[9];
        uint8_t b = ASICRead(WPC_SW_JUMPER_INPUT);
        for (int i = 0; i < 8; i++)
            out[i] = (b & (0x80 >> i)) ? '1' : '0';
        out[8] = '\0';
        snprintf(buf, 128, "Jumpers = %s", out);
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 9, 22, MAX_BRIGHTNESS_COLOR);
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_TESTS) {
        snprintf(buf, 128, "        TESTS");
        RPU_WPC_DrawTextToScratch((const char *)buf, MAX_BRIGHTNESS_COLOR);
        snprintf(buf, 128, "  Running Tests...");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 10, 14, MAX_BRIGHTNESS_COLOR);
    }
}

void RPU_WPC_DrawMenuPage(bool isAlternate) {    
    if (RPUWPCSubMenuPage) {
        RPU_WPC_DrawSubMenuPage(isAlternate);
        return;        
    }
    
    char buf[128];
    RPU_WPC_DisplayClear();
    RPU_WPC_DrawRectangle(0, false, MAX_BRIGHTNESS_COLOR);
    
    if (RPUWPCMenuPage==RPU_WPC_MENU_GAME_STATE) {
        snprintf(buf, 128, "     LOAD/SAVE");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 6, 6, MAX_BRIGHTNESS_COLOR);
        snprintf(buf, 128, "      SETTINGS");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 16, MAX_BRIGHTNESS_COLOR);
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_ROM) {
        snprintf(buf, 128, "         ROM");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 10, MAX_BRIGHTNESS_COLOR);
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_GI) {
        snprintf(buf, 128, "GENERAL");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 45, 6, MAX_BRIGHTNESS_COLOR);
        snprintf(buf, 128, "ILLUMINATION");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 28, 16, MAX_BRIGHTNESS_COLOR);
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_SOUND_CARD) {
        snprintf(buf, 128, "     SOUND CARD");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 10, MAX_BRIGHTNESS_COLOR);
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_DISPLAY) {
        snprintf(buf, 128, "       DISPLAY");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 10, MAX_BRIGHTNESS_COLOR);
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_INFORMATION) {
        snprintf(buf, 128, "     INFORMATION");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 10, MAX_BRIGHTNESS_COLOR);
    } else if (RPUWPCMenuPage==RPU_WPC_MENU_TESTS) {
        snprintf(buf, 128, "        TESTS");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 10, MAX_BRIGHTNESS_COLOR);
    }

    // Determine if we show navigation arrows
    if (isAlternate) {
        RPU_WPC_DisplayDrawArrow(63, 29, 2, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DisplayDrawArrow(126, 15, 1, MAX_BRIGHTNESS_COLOR);
        RPU_WPC_DisplayDrawArrow(1, 15, 3, MAX_BRIGHTNESS_COLOR);
    }
}


bool RPU_WPC_Menu(uint32_t curTicks) {
    RPUWPCCurrentTicks = curTicks;

    // Service the Sound Card UART buffer every 1 millisecond
    static uint32_t lastSoundCardUpdateTicks = 0;
    if ((curTicks - lastSoundCardUpdateTicks) >= (RPU_WPC_TICKS_PER_SECOND / 1000)) {
        lastSoundCardUpdateTicks = curTicks;
        HPSoundCardUpdate();
    }    

    // Check for Sound Card Serial Data availability 
    if (RPUWPCMenuPage == RPU_WPC_MENU_SOUND_CARD && RPUWPCSubMenuPage > 0 && !SoundCardDataValid) {
        bool newlyValid = false;
        int16_t l, r;
        
        // Assign the UART responses to your local variables
        if (RPUWPCSubMenuPage == 1 && HPSoundCardCurSynthVol(&l, &r)) {
            newlyValid = true;
        } else if (RPUWPCSubMenuPage == 2 && HPSoundCardCurDACVol(&l, &r)) {
            newlyValid = true;
        } else if (RPUWPCSubMenuPage == 3 && HPSoundCardCurCVSDVol(&l, &r)) {
            newlyValid = true;
        } else if (RPUWPCSubMenuPage == 4) {
            if (SoundCardEditorMode == 0 && HPSoundCardGetActiveROMName() != NULL) {
                newlyValid = true;
            } else if (SoundCardEditorMode == 1) {
                uint16_t numRoms;
                if (HPSoundCardCurNumROMs(&numRoms) && HPSoundCardCurROMName() != NULL) { // WAIT FOR BOTH
                    newlyValid = true; // We got both the count and the name, stop the error screen
                }
            } else if (SoundCardEditorMode == 2) {
                uint8_t retCode;
                SoundCardRequestTicks = curTicks; // Feed the watchdog! Flashing takes time.
                if (HPSoundCardCurFlashCode(&retCode)) {
                    if (retCode != LastSoundCardFlashCode) {
                        LastSoundCardFlashCode = retCode;
                        RPUWPCSubItemChanged = true; // Redraw new percentage
                    }
                    if (retCode >= 100) {
                        newlyValid = true; // Flash sequence completed
                    }
                } else if (HPSoundCardCurFlashMessage() != NULL) {
                    newlyValid = true; // Flash returned 0xFF ("ERROR")
                }
            }
        }

        if (newlyValid) {
            SoundCardDataValid = true;
            // If the user hasn't touched the buttons yet, sync the active editor vars
            if (SoundCardEditorMode == 0) {
                if (RPUWPCSubMenuPage == 1) { SC_Vol = synthVol; SC_Bal = synthBal; }
                if (RPUWPCSubMenuPage == 2) { SC_Vol = DACVol; SC_Bal = DACBal; }
                if (RPUWPCSubMenuPage == 3) { SC_Vol = CVSDVol; SC_Bal = CVSDBal; }
            }
            RPUWPCSubItemChanged = true; // force redraw
        } else if (!SoundCardErrorShown && (curTicks - SoundCardRequestTicks) > RPU_WPC_TICKS_PER_SECOND) {
            SoundCardErrorShown = true;
            RPUWPCSubItemChanged = true;
        }
    }


    // Set Alternate mode states via standard display API calls
    if (RPUWPCMenuPage == RPU_WPC_MENU_LANDING_PAGE && RPUWPCLastMenuPage != 0) {
        RPU_WPC_DisplaySetAlternateMode(0, false);
    } else if (RPUWPCMenuPage == RPU_WPC_MENU_LANDING_PAGE || RPUWPCSubMenuPage == 0) {
        uint32_t elapsed = curTicks - RPUWPCMenuPageChangeTicks;
        if (elapsed < RPU_WPC_ARROW_WAIT_TICKS) {
            RPU_WPC_DisplaySetAlternateMode(0, false); // Primary only
        } else if (elapsed < (RPU_WPC_ARROW_WAIT_TICKS + RPU_WPC_ARROW_SOLID_TICKS)) {
            RPU_WPC_DisplaySetAlternateMode(0, true); // Solid Alternate (arrows showing)
        } else {
            RPU_WPC_DisplaySetAlternateMode(RPU_WPC_ARROW_FLASH_HALF_CYCLE, false); // Flashing Alternate
        }
    } else if (RPUWPCMenuPage == RPU_WPC_MENU_ROM && RPUWPCSubMenuPage == 2) {
        RPU_WPC_DisplaySetAlternateMode(RPU_WPC_ARROW_FLASH_HALF_CYCLE, false); // Flashing file text
    } else if (RPUWPCMenuPage == RPU_WPC_MENU_GAME_STATE && GameStateNamePickerMode) {
        // Add this block to enable flashing for the settings name picker
        RPU_WPC_DisplaySetAlternateMode(RPU_WPC_ARROW_FLASH_HALF_CYCLE, false);        
    } else if (RPUWPCMenuPage == RPU_WPC_MENU_GI && GIEditorMode) {
        RPU_WPC_DisplaySetAlternateMode(RPU_WPC_ARROW_FLASH_HALF_CYCLE, false); // Flashing Editor value
    } else if (RPUWPCMenuPage == RPU_WPC_MENU_SOUND_CARD && SoundCardEditorMode) {
        RPU_WPC_DisplaySetAlternateMode(RPU_WPC_ARROW_FLASH_HALF_CYCLE, false); // Flashing Volume/Balance element
    } else if (RPUWPCMenuPage == RPU_WPC_MENU_DISPLAY && RPUWPCSubMenuPage) {
        RPU_WPC_DisplaySetAlternateMode(RPU_WPC_ARROW_FLASH_HALF_CYCLE, false); 
        // Periodically check the brightness if we're in the Display menu
        if ((curTicks - RPUWPCMenuPageChangeTicks) > RPU_WPC_DISPLAY_BRIGHTNESS_CHECK_TIME) {
            HPDisplayBrightness = ReadDisplay(WPC_DMD_BRIGHTNESS);
            RPUWPCMenuPageChangeTicks = curTicks;
        }
    
        if (LastBrightness!=HPDisplayBrightness) {
            LastBrightness = HPDisplayBrightness;
            RPUWPCSubItemChanged = true;
        }
    } else {
        RPU_WPC_DisplaySetAlternateMode(0, false);
    }


    // Build layouts conditionally based on what page we are transitioning to
    if (RPUWPCMenuPage == RPU_WPC_MENU_LANDING_PAGE) {
        if (RPUWPCLastMenuPage != 0) {
            bool firstTime = (RPUWPCLastMenuPage != 0);
            
            if (firstTime) {
                GameStateAttributes_Parse();
                TotalRAMAttributes = GameStateAttributes_GetAttributeCount();
                RPUWPCMenuStartTicks = curTicks;
                RPUWPCMenuPageChangeTicks = curTicks;
            }

            // Draw Base Primary Screen (No arrows)
            RPU_WPC_DisplaySetDrawTarget(0);
            RPU_WPC_DisplayClear();
            RPU_WPC_DrawTextToScratchXY((const char *)"      Homepin", 5, 5, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawTextToScratchXY((const char *)"    SYSTEM MENU", 5, 16, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawRectangle(0, false, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawRectangle(1, false, MAX_BRIGHTNESS_COLOR);
            
            // Draw Alternate Screen (With Arrows)
            RPU_WPC_DisplaySetDrawTarget(1);
            RPU_WPC_DisplayClear();
            RPU_WPC_DrawTextToScratchXY((const char *)"      Homepin", 5, 5, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawTextToScratchXY((const char *)"    SYSTEM MENU", 5, 16, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DisplayDrawArrow(63, 29, 2, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawRectangle(0, false, MAX_BRIGHTNESS_COLOR);
            RPU_WPC_DrawRectangle(1, false, MAX_BRIGHTNESS_COLOR);
            
            if (firstTime) {
                if (RPUWPCLastMenuPage == 0xFF) {
                    // Start immediately if directly booting from 0xFF
                } else {
                    RPU_WPC_DisplayStartWipe(2, (RPU_WPC_TICKS_PER_SECOND / 8), curTicks); // Up wipe
                }
                RPUWPCLastMenuPage = 0;
            }
            RPU_WPC_DisplayCommit();
        }
    } else {
        bool pageChanged = (    RPUWPCLastMenuPage != RPUWPCMenuPage || 
                                RPUWPCLastSubMenuPage != RPUWPCSubMenuPage || 
                                RPUWPCSubItemChanged );
        
        if (pageChanged) {
            if (RPUWPCLastMenuPage != RPUWPCMenuPage || RPUWPCLastSubMenuPage != RPUWPCSubMenuPage) {
                if (RPUWPCMenuPage == RPU_WPC_MENU_SOUND_CARD && RPUWPCSubMenuPage >= 1 && RPUWPCSubMenuPage <= 4) {
                    SoundCardDataValid = false;
                    SoundCardErrorShown = false;
                    SoundCardRequestTicks = curTicks;
                    if (RPUWPCSubMenuPage == 1) HPSoundCardRequestSynthVol();
                    else if (RPUWPCSubMenuPage == 2) HPSoundCardRequestDACVol();
                    else if (RPUWPCSubMenuPage == 3) HPSoundCardRequestCVSDVol();
                    else if (RPUWPCSubMenuPage == 4) HPSoundCardRequestCurrentROMName();
                }

                RPUWPCLastMenuPage = RPUWPCMenuPage;
                RPUWPCLastSubMenuPage = RPUWPCSubMenuPage;
                RPUWPCMenuPageChangeTicks = curTicks;
            }
            
            // Draw Target 0
            RPU_WPC_DisplaySetDrawTarget(0); 
            RPU_WPC_DrawMenuPage(false);
            
            // Draw Target 1 
            RPU_WPC_DisplaySetDrawTarget(1); 
            RPU_WPC_DrawMenuPage(true);
            
            if (RPUWPCSubItemChanged) {
                // Wipe isn't required when we alter values in-place
                RPUWPCSubItemChanged = false;
            } else {
                int wipeDuration = (AnimationDirection == 2 || AnimationDirection == -2) ? (RPU_WPC_TICKS_PER_SECOND / 8) : (RPU_WPC_TICKS_PER_SECOND / 4);
                RPU_WPC_DisplayStartWipe(AnimationDirection, wipeDuration, curTicks);
            }
            RPU_WPC_DisplayCommit();
        }
    }

    // Check input
    uint8_t cabinetInput = ASICReadUnfilteredCabinetSwitches();

    // ENTRY HOLD: Absorb the initial button combo used to open the menu.
    if (RPUWPCMenuAwaitingRelease) {
        uint8_t menuButtons = RPU_WPC_CABINET_ESCAPE_BUTTON | 
                              RPU_WPC_CABINET_MINUS_BUTTON | 
                              RPU_WPC_CABINET_PLUS_BUTTON | 
                              RPU_WPC_CABINET_ENTER_BUTTON;
                              
        if ((cabinetInput & menuButtons) == 0) {
            RPUWPCMenuAwaitingRelease = false;
            RPUWPCLastCabinetInput = cabinetInput;
            RPUWPCLastCabinetInputTicks = curTicks;
        }
        return true; 
    }

    // EXTRICATION HOLD: Absorb the Escape button hold when exiting the menu.
    if (RPUWPCMenuPage == 0xFF) {
        if (cabinetInput & RPU_WPC_CABINET_ESCAPE_BUTTON) {
            RPUWPCLastCabinetInputTicks = curTicks;
            return true; 
        } else {
            if ((curTicks - RPUWPCLastCabinetInputTicks) > CABINET_BUTTON_DEBOUNCE_TICKS) {
                RPUWPCMenuPage = 0;
                RPUWPCLastMenuPage = 0xFF;
                RPUWPCSubMenuPage = 0;
                RPUWPCLastSubMenuPage = 0xFF;
                ASICResetCabinetFilter();
                MPURestoreDisplayState();
                ASICSetFlipperStateOverride(false, false);
                return false; // Safely return control to the ROM
            }
            return true; 
        }
    }

    if (cabinetInput!=RPUWPCLastCabinetInput) {
        RPUWPCLastCabinetInput = cabinetInput;
        // Need to debounce this button
        if ((curTicks-CABINET_BUTTON_DEBOUNCE_TICKS)>RPUWPCLastCabinetInputTicks) {
            RPUWPCLastCabinetInputTicks = curTicks;
            RPU_WPC_NavigateMenu(cabinetInput);
        }
    }

    return true;
}

bool RPU_WPC_ROMRequiresReset() {
    bool romWasChanged = RPUWPC_ROMChanged;
    RPUWPC_ROMChanged = false;
    return romWasChanged;
}

bool RPU_WPC_RAMRequiresReset() {
    bool ramWasChanged = RPUWPC_RAMChanged;
    RPUWPC_RAMChanged = false;
    return ramWasChanged;
}