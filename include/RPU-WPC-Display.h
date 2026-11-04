#ifndef RPU_WPC_DISPLAY_H
#define RPU_WPC_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

bool RPU_WPC_DisplayInit(void);

// Buffer targeting: 0 = Primary Screen, 1 = Alternate Screen (with arrows)
void RPU_WPC_DisplaySetDrawTarget(uint8_t targetIdx);
void RPU_WPC_DisplayClear(void);
void RPU_WPC_DisplayCommit(void);

// Drawing primitives
#define MAX_BRIGHTNESS_COLOR    4
void RPU_WPC_DisplayDrawPixel(uint8_t x, uint8_t y, uint8_t color);
void RPU_WPC_DrawHorizontalLine(int x1, int x2, int y, uint8_t color);
void RPU_WPC_DrawRectangle(int offset, bool filled, uint8_t color);
void RPU_WPC_DrawRectangleDotted(int offset, bool filled, uint8_t color);
void RPU_WPC_DrawRectangleXY(int x, int y, int width, int height, bool filled, uint8_t color);
void RPU_WPC_DrawTextToScratch(const char *message, uint8_t color);
void RPU_WPC_DrawTextToScratchXY(const char *message, int xCorner, int yCorner, uint8_t color);
void RPU_WPC_DisplayDrawArrow(int x, int y, int direction, uint8_t color);

bool RPU_WPC_DisplayShowLogoScreen(int pageNum);
void RPU_WPC_DisplayShowLogo(int pageNum, uint32_t tickCount);
void RPU_WPC_DisplayTextWithLogo(const char *message, uint8_t color);

// Animation triggers
// direction: 1 = Right, -1 = Left, 2 = Down, -2 = Up
void RPU_WPC_DisplayStartWipe(int direction, uint32_t durationTicks, uint32_t currentTicks);

// frequencyTicks: 0 stops flashing. forceAlternate: true overrides and forces solid alternate
void RPU_WPC_DisplaySetAlternateMode(uint32_t frequencyTicks, bool forceAlternate);

// Main rendering function (Called ONLY during FIRQ)
void RPU_WPC_DisplayUpdate(bool firqStatus, uint32_t currentTicks);

// For callers who need to write the buffer without calling update
// (for solid color only)
void RPU_WPC_DisplayUpdateImmediate(void);

#endif