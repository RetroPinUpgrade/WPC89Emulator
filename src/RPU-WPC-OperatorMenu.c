#include <stdio.h>
#include "mpu89.h"
#include "RPU-WPC-OperatorMenu.h"
#include "RPU-WPC-Display.h"
#include "GameStateAttributes.h"
#include "gd32f4xx.h"

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

static uint16_t TotalRAMAttributes = 0;
static uint16_t CurrentRAMAttribute = 0;
static uint16_t LastRAMAttribute = 0xFFFF;

uint32_t RPUWPCCurrentTicks = 0;
bool RPUWPCAnimationRunning = false;
bool RPUWPCSubItemChanged = false;

// Menu pages
// 0 - landing, enter or exit
// 1 - Load new ROM
// 2 - Load / Save settings to card
// 3 - Change relative volumes (music, sound FX, callouts, ducking)
// 4 - Set Time & Date
// 5 - Achievments mapping

#define RPU_WPC_MENU_LANDING_PAGE           0
#define RPU_WPC_LOAD_ROM_MENU               1
#define RPU_WPC_LOAD_SETTINGS_MENU          2
#define RPU_WPC_VOLUME_MENU                 3
#define RPU_WPC_DATE_TIME_MENU              4
#define RPU_WPC_ACHIEVEMENTS_MENU           5

#define RPU_WPC_CABINET_ESCAPE_BUTTON       0x10
#define RPU_WPC_CABINET_MINUS_BUTTON        0x20
#define RPU_WPC_CABINET_PLUS_BUTTON         0x40
#define RPU_WPC_CABINET_ENTER_BUTTON        0x80

#define CABINET_BUTTON_DEBOUNCE_TICKS   200000U


void RPU_WPC_SetupPorts() {
    // Turn on blanking
    SetBlanking(true);
    // Turn off lamp rows & columns
    SetLampRow(0);
    SetLampCol(0);

    // Turn off all solenoids and flashers
    SetSol1(0x00);
    SetSol2(0x00);
    SetSol3(0x00);
    SetSol4(0x00);

    // Turn off flipper relay and GI
}

bool RPU_WPC_CheckForMenuRequest() {
    // If the operator presses both + and - during
    // boot, we'll assume they want to enter the
    // special boot menu
    byte cabInput = MPUHardwareRead(WPC_SWITCH_CABINET_INPUT);
    if ((cabInput & 0x60)==0x60) {
        return true;
    }
    return false;
}


void ChangeRelativeVolume(int volToChange, int changeAmount) {
    if (volToChange==0 || volToChange>3) return;
    uint8_t incomingVol = 0xFF;
    incomingVol = (RTC_BKP9>>(8*volToChange)) & 0x000000FF;

    uint8_t outgoingVol = (uint8_t)((int)incomingVol + changeAmount);
    if (outgoingVol<5) outgoingVol = 5;
    if (outgoingVol>100) outgoingVol = 100;

    RTC_BKP9 &= ~(0xFF<<(8*volToChange));
    RTC_BKP9 |= (outgoingVol<<(8*volToChange));
}

// animation directions
// -1 / 1 = left / right
// -2 / 2 = up / down 
static int32_t AnimationDirection = 1;
void RPU_WPC_HandleAdjustment(uint8_t cabinetInput) {
    // this could be navigation through sub menus or changing a value
    if (RPUWPCMenuPage==RPU_WPC_LOAD_ROM_MENU) {
        // the only input that makes sense is 
        // the enter (escape is handled before we get here)
        if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
            RPUWPCSettingChangeStartTicks = RPUWPCCurrentTicks;
        }
    } else if (RPUWPCMenuPage==RPU_WPC_ACHIEVEMENTS_MENU) {
        if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) {
            CurrentRAMAttribute += 1;
            if (CurrentRAMAttribute>=TotalRAMAttributes) CurrentRAMAttribute = 0;
            RPUWPCSubItemChanged = true;
            AnimationDirection = -1;
        } else if (cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
            if (CurrentRAMAttribute) { 
                CurrentRAMAttribute -= 1;
            } else {
                if (TotalRAMAttributes) CurrentRAMAttribute = TotalRAMAttributes - 1;
                else CurrentRAMAttribute = 0;
            }
            RPUWPCSubItemChanged = true;
            AnimationDirection = 1;
        }
    } else if (RPUWPCMenuPage==RPU_WPC_VOLUME_MENU) {
        if (cabinetInput&RPU_WPC_CABINET_ENTER_BUTTON) {
            RPUWPCVolumeChange += 1;
            if (RPUWPCVolumeChange>3) RPUWPCVolumeChange = 1;
            RPUWPCSubItemChanged = true;
        } else if (cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
            ChangeRelativeVolume(RPUWPCVolumeChange, -5);
            RPUWPCSubItemChanged = true;
        } else if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) {
            ChangeRelativeVolume(RPUWPCVolumeChange, 5);
            RPUWPCSubItemChanged = true;
        }
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
            if (RPUWPCMenuPage<4) RPUWPCMenuPage += 1;
            else RPUWPCMenuPage = 0;
        }
    } else {
        if (RPUWPCSubMenuPage) {
            // We're in a sub menu
            if (cabinetInput&RPU_WPC_CABINET_ESCAPE_BUTTON) {
                // operator is leaving this option
                AnimationDirection = 2;
                RPUWPCSubMenuPage = 0;        
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
                if (RPUWPCMenuPage==RPU_WPC_ACHIEVEMENTS_MENU) {
                    CurrentRAMAttribute = 0;
                    LastRAMAttribute = 0xFFFF;
                } else if (RPUWPCMenuPage==RPU_WPC_VOLUME_MENU) {
                    RPUWPCVolumeChange = 1;
                }
            } else if (cabinetInput&RPU_WPC_CABINET_PLUS_BUTTON) {
                // operator hit +, so go to next menu
                AnimationDirection = -1;
                RPUWPCMenuPage += 1;
                if (RPUWPCMenuPage>RPU_WPC_ACHIEVEMENTS_MENU) RPUWPCMenuPage = RPU_WPC_LOAD_ROM_MENU;
            } else if (cabinetInput&RPU_WPC_CABINET_MINUS_BUTTON) {
                // operator hit -, so go to previous menu
                AnimationDirection = 1;
                RPUWPCMenuPage -= 1;
                if (RPUWPCMenuPage==0) RPUWPCMenuPage = RPU_WPC_ACHIEVEMENTS_MENU;
            }
        }
    }

}

#define RPU_WPC_TICKS_PER_SECOND    2000000U
uint32_t AnimationLastFliptime = 0;

bool UpdateAnimation(uint32_t elapsedTicks) {
    if (AnimationDirection==1 || AnimationDirection==-1) {
        RPUWPCAnimationRunning = RPU_WPC_DisplayWipe((RPU_WPC_TICKS_PER_SECOND/4) * AnimationDirection, elapsedTicks);
        RPU_WPC_DisplayFlipBackToFront();
    } else if (AnimationDirection==-2 || AnimationDirection==2) {
        RPUWPCAnimationRunning = RPU_WPC_DisplayWipeUD((RPU_WPC_TICKS_PER_SECOND/8) * AnimationDirection, elapsedTicks);
        RPU_WPC_DisplayFlipBackToFront();
    } else if (AnimationDirection==3) {
        if ((elapsedTicks-(RPU_WPC_TICKS_PER_SECOND/4))>AnimationLastFliptime) {
            // flip back to front every 500ms
            AnimationLastFliptime = elapsedTicks;
            RPU_WPC_DisplayFlipBackToFront();
        }
    }
    return RPUWPCAnimationRunning;
}

void DrawRectangle(int offset) {
    if (offset>15) return;
    for (int count=offset; count<(512-offset*2); count++) {
        RPU_WPC_DisplayDrawPixel(count+offset, offset, 1);
        RPU_WPC_DisplayDrawPixel(count+offset, 31-offset, 1);
        if (count<(32-offset*2)) {
            RPU_WPC_DisplayDrawPixel(offset, count+offset, 1);
            RPU_WPC_DisplayDrawPixel(127-offset, count+offset, 1);
        }
    }
}

// Menu pages
// 0 - landing, enter or exit
// 1 - Load new ROM
// 2 - Load / Save settings to card
// 3 - Change relative volumes (music, sound FX, callouts, ducking)
// 4 - Set Time & Date
// 5 - Achievments mapping

void RPU_WPC_DrawSubMenuPage() {
    char buf[128];
    RPU_WPC_DisplayClearScratchBuffer();
    RPUWPCSubItemChanged = false;
    
    if (RPUWPCMenuPage==RPU_WPC_LOAD_ROM_MENU) {
        // There's only one sub menu, and it's 
        // for loading the ROM
        snprintf(buf, 128, "    LOAD NEW ROM");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "(not yet\n implemented)");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 20, 14);
    } else if (RPUWPCMenuPage==RPU_WPC_LOAD_SETTINGS_MENU) {
        snprintf(buf, 128, "    LOAD SETTINGS");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "(not yet\n implemented)");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 20, 14);
    } else if (RPUWPCMenuPage==RPU_WPC_VOLUME_MENU) {
        snprintf(buf, 128, "    AUDIO MIXING");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "VOL    MUS  SFX  VOI");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 2, 14);
        uint8_t vol = RTC_BKP9 & 0x000000FF;
        uint8_t mus = (RTC_BKP9>>8) & 0x000000FF;
        uint8_t sfx = (RTC_BKP9>>16) & 0x000000FF;
        uint8_t voi = (RTC_BKP9>>24) & 0x000000FF;
        snprintf(buf, 128, "%3d   %3d%% %3d%% %3d%%", vol, mus, sfx, voi);
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 2, 23);
        RPU_WPC_DisplayUpdateBackBuffer(); // put the scratch in back buffer
        RPU_WPC_DisplayFlipBackToFront(); // flip back to front

        RPU_WPC_DisplayClearScratchBuffer();
        snprintf(buf, 128, "    AUDIO MIXING");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "VOL    MUS  SFX  VOI");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 2, 14);
        if (RPUWPCVolumeChange==0)      snprintf(buf, 128, "      %3d%% %3d%% %3d%%", mus, sfx, voi);
        else if (RPUWPCVolumeChange==1) snprintf(buf, 128, "%3d        %3d%% %3d%%", vol, sfx, voi);
        else if (RPUWPCVolumeChange==2) snprintf(buf, 128, "%3d   %3d%%      %3d%%", vol, mus, voi);
        else                            snprintf(buf, 128, "%3d   %3d%% %3d%%    ", vol, mus, sfx);
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 2, 23);
        RPU_WPC_DisplayUpdateBackBuffer(); // put the scratch in back buffer
        AnimationDirection = 3; // flip back and forth
    } else if (RPUWPCMenuPage==RPU_WPC_DATE_TIME_MENU) {
    } else if (RPUWPCMenuPage==RPU_WPC_ACHIEVEMENTS_MENU) {
        snprintf(buf, 128, "ACHIEVEMENTS MAPPING");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        wpc_ram_attribute_t ramAttribute;
        if (GameStateAttributes_GetAttribute(CurrentRAMAttribute, &ramAttribute)==0) {
            uint8_t *ramPtr = MPUGetRAMAtIndex(ramAttribute.start);
            if (ramPtr) {
                GameStateAttributes_FormatAttributeForDisplay(&ramAttribute, ramPtr, buf);
                RPU_WPC_DrawTextToScratchXY((const char *)buf, 0, 14);
            }
        }
    }
}

void RPU_WPC_DrawMenuPage() {    
    if (RPUWPCSubMenuPage) {
        RPU_WPC_DrawSubMenuPage();
        return;        
    }
    
    char buf[128];
    RPU_WPC_DisplayClearScratchBuffer();
    DrawRectangle(0);
    if (RPUWPCMenuPage==RPU_WPC_LOAD_ROM_MENU) {
        snprintf(buf, 128, "    LOAD NEW ROM");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "Press Enter to load:\n(no card found)");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 3, 14);
    } else if (RPUWPCMenuPage==RPU_WPC_LOAD_SETTINGS_MENU) {
        snprintf(buf, 128, "LOAD / SAVE SETTINGS");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "Press Enter for\nsettings");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 20, 14);
    } else if (RPUWPCMenuPage==RPU_WPC_VOLUME_MENU) {
        snprintf(buf, 128, "       VOLUME");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "Press Enter for\nsound settings");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 24, 14);
    } else if (RPUWPCMenuPage==RPU_WPC_DATE_TIME_MENU) {
        snprintf(buf, 128, "    TIME / DATE");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "Press Enter for\ninternal clock");
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 20, 14);
    } else if (RPUWPCMenuPage==RPU_WPC_ACHIEVEMENTS_MENU) {
        snprintf(buf, 128, "ACHIEVEMENTS MAPPING");
        RPU_WPC_DrawTextToScratch((const char *)buf);
        snprintf(buf, 128, "Press Enter for\nmapping"); 
        RPU_WPC_DrawTextToScratchXY((const char *)buf, 20, 14);
    }

}



bool RPU_WPC_Menu(uint32_t curTicks) {
    //if (curTicks>(10U * 2000000)) return false;
    RPUWPCCurrentTicks = curTicks;

    if (RPUWPCMenuPage==RPU_WPC_MENU_LANDING_PAGE) {
        if (RPUWPCLastMenuPage!=0) {
            // This is the first time we're entering the menu
            // Parse the attributes
            GameStateAttributes_Parse();
            TotalRAMAttributes = GameStateAttributes_GetAttributeCount();

            RPU_WPC_DisplayClearScratchBuffer();
            RPU_WPC_DrawTextToScratch((const char *)"    HOMEPIN MENU\nPress Escape to boot or Enter to adjust");
            DrawRectangle(0);
            DrawRectangle(1);
            if (RPUWPCLastMenuPage==0xFF) {
                RPU_WPC_DisplayUpdateBackBuffer();        
                RPU_WPC_DisplayFlipBackToFront();
            } else {
                RPUWPCMenuPageChangeTicks = curTicks;
                RPUWPCAnimationRunning = true; // We will reveal the new scratch
            }
            RPUWPCMenuStartTicks = curTicks;
            RPUWPCLastMenuPage = 0;
        }
    } else {
        if (RPUWPCLastMenuPage!=RPUWPCMenuPage || RPUWPCLastSubMenuPage!=RPUWPCSubMenuPage || RPUWPCSubItemChanged) {
            RPUWPCLastMenuPage = RPUWPCMenuPage;
            RPUWPCLastSubMenuPage = RPUWPCSubMenuPage;
            RPUWPCMenuPageChangeTicks = curTicks;
            RPU_WPC_DisplayUpdateCopyScratch(); 
            // This is the display of this page
            RPU_WPC_DrawMenuPage();
            RPUWPCAnimationRunning = true; // We will reveal the new scratch
        }
    }
    if (RPUWPCAnimationRunning) UpdateAnimation(curTicks - RPUWPCMenuPageChangeTicks);

    // Check input
    uint8_t cabinetInput = MPUHardwareRead(WPC_SWITCH_CABINET_INPUT);
    if (cabinetInput!=RPUWPCLastCabinetInput) {
        RPUWPCLastCabinetInput = cabinetInput;
        // Need to debounce this button
        if ((curTicks-CABINET_BUTTON_DEBOUNCE_TICKS)>RPUWPCLastCabinetInputTicks) {
            RPUWPCLastCabinetInputTicks = curTicks;
            RPU_WPC_NavigateMenu(cabinetInput);
            if (RPUWPCMenuPage==0xFF) {
                RPUWPCMenuPage = 0;
                RPUWPCLastMenuPage = 0xFF;
                RPUWPCSubMenuPage = 0;
                RPUWPCLastSubMenuPage = 0xFF;
                return false;
            }
        }
    } else {
        // We can handle repeat keys here
    }

    return true;
}