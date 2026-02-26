#ifndef RPU_WPC_DISPLAY_H

bool RPU_WPC_DisplayInit();
bool RPU_WPC_DisplayShowLogoScreen();
void RPU_WPC_DrawTextToScratch(const char *message);
void RPU_WPC_DrawTextToScratchXY(const char *message, int xCorner, int yCorner);
void RPU_WPC_DisplayTextWithLogo(const char *message);
void RPU_WPC_DisplayShowLogo(int pageNum, uint32_t tickCount);
void RPU_WPC_DisplayDrawPixel(uint8_t x, uint8_t y, uint8_t color);
void RPU_WPC_DisplayClearScratchBuffer();
void RPU_WPC_DisplayUpdateBackBuffer();
void RPU_WPC_DisplayFlipBackToFront();
void RPU_WPC_DisplayUpdateCopyScratch();
void RPU_WPC_ShiftScratchRow(uint8_t rowNum, int shiftPixels);
bool RPU_WPC_DisplayWipe(int speed, uint32_t elapsedTicks);
bool RPU_WPC_DisplayWipeUD(int speed, uint32_t elapsedTicks);


#define RPU_WPC_DISPLAY_H
#endif