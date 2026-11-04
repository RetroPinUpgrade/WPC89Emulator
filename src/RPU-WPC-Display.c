#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "RPU-WPC-Display.h"
#include "mpu89.h"

// Core Display Buffers mapped to 4 logical layers
static uint8_t RPUWPC_PrimaryBuffer[4][512];
static uint8_t RPUWPC_AlternateBuffer[4][512];
static uint8_t RPUWPC_RenderBuffer[4][512];
static uint8_t RPUWPC_WipeSourceBuffer[4][512];

// Active drawing pointer
static uint8_t (*ActiveDrawBuffer)[512] = RPUWPC_PrimaryBuffer;

// State Flags
static bool DisplayDirty = true;
static bool PendingPageFlip = false;

// Hardware RAM pages mapping to our 4 logical layers
static uint8_t RPUWPC_FrontPage[4] = {8, 9, 10, 11};
static uint8_t RPUWPC_BackPage[4]  = {12, 13, 14, 15};

// Active Frame loops 0 through 5 to represent the 6 output frames
static uint8_t RPUWPC_ActiveFrame = 0;

// Animation State
static bool WipeRunning = false;
static uint32_t WipeStartTicks = 0;
static uint32_t WipeDuration = 0;
static int WipeDirection = 0;

// Alternate Screen State
static uint32_t AlternateFrequency = 0;
static bool AlternateSolid = false;

extern const unsigned char HPLogoFrame[512];
extern const unsigned char RPUWPC_FONT_5x7[95][5];

void RPU_WPC_WriteDelay() {
    DelayQuarterCycle(); DelayQuarterCycle(); DelayQuarterCycle(); DelayQuarterCycle();
    DelayQuarterCycle(); DelayQuarterCycle(); DelayQuarterCycle(); DelayQuarterCycle();
}

bool RPU_WPC_DisplayInit() {
    RPUWPC_FrontPage[0] = 8;  RPUWPC_FrontPage[1] = 9;  
    RPUWPC_FrontPage[2] = 10; RPUWPC_FrontPage[3] = 11;
    RPUWPC_BackPage[0]  = 12; RPUWPC_BackPage[1]  = 13; 
    RPUWPC_BackPage[2]  = 14; RPUWPC_BackPage[3]  = 15;
    RPUWPC_ActiveFrame = 0;
    PendingPageFlip = false;
    
    memset(RPUWPC_PrimaryBuffer, 0, 2048);
    memset(RPUWPC_AlternateBuffer, 0, 2048);
    memset(RPUWPC_RenderBuffer, 0, 2048);

    WriteDisplay(WPC_DMD_LOW_PAGE, RPUWPC_FrontPage[0]);
    RPU_WPC_WriteDelay();
    WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_BackPage[0]);
    RPU_WPC_WriteDelay();
    WriteDisplay(WPC_DMD_ACTIVE_PAGE, RPUWPC_FrontPage[0]);
    RPU_WPC_WriteDelay();
    
    for (int frame = 0; frame < 4; frame++) {
        WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_BackPage[frame]);
        RPU_WPC_WriteDelay();
        for (int count = 0; count < 512; count++) {
            WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START + count, 0x00);
            RPU_WPC_WriteDelay();
        }
        WriteDisplay(WPC_DMD_LOW_PAGE, RPUWPC_FrontPage[frame]);
        RPU_WPC_WriteDelay();
        for (int count = 0; count < 512; count++) {
            WriteDisplay(DISPLAY_RAM_LOWER_PAGE_START + count, 0x00);
            RPU_WPC_WriteDelay();
        }
    }
    
    WriteDisplay(WPC_DMD_LOW_PAGE, RPUWPC_FrontPage[RPUWPC_ActiveFrame]);
    RPU_WPC_WriteDelay();
    WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_BackPage[0]);
    RPU_WPC_WriteDelay();
    
    // Set FIRQ to trigger on scanline 31 for toggling buffers during vertical refresh
    WriteDisplay(WPC_DMD_SCANLINE, 31);
    RPU_WPC_WriteDelay();
    
    return true;
}

void RPU_WPC_DisplaySetDrawTarget(uint8_t targetIdx) {
    ActiveDrawBuffer = (targetIdx == 1) ? RPUWPC_AlternateBuffer : RPUWPC_PrimaryBuffer;
}

void RPU_WPC_DisplayClear() {
    for (int i = 0; i < 4; i++) {
        memset(ActiveDrawBuffer[i], 0x00, 512);
    }
}

void RPU_WPC_DisplayCommit() {
    DisplayDirty = true;
}

void RPU_WPC_DisplayStartWipe(int direction, uint32_t durationTicks, uint32_t currentTicks) {
    for (int i = 0; i < 4; i++) {
        memcpy(RPUWPC_WipeSourceBuffer[i], RPUWPC_RenderBuffer[i], 512);
    }
    
    WipeRunning = true;
    WipeDirection = direction;
    WipeDuration = durationTicks;
    WipeStartTicks = currentTicks;
}

void RPU_WPC_DisplaySetAlternateMode(uint32_t frequencyTicks, bool forceAlternate) {
    AlternateFrequency = frequencyTicks;
    AlternateSolid = forceAlternate;
}

static void ComputeWipe(uint32_t elapsedTicks) {
    int direction = WipeDirection; 
    uint32_t absSpeed = WipeDuration;
    if (absSpeed == 0) absSpeed = 1;
    
    for (int frame = 0; frame < 4; frame++) {
        if (direction == 1 || direction == -1) {
            for (uint8_t row = 0; row < 32; row++) {
                uint32_t rowStartTick = (absSpeed / (32 * 4)) * row;
                for (uint8_t col = 0; col < 16; col++) {
                    uint8_t mixedByte = RPUWPC_WipeSourceBuffer[frame][row * 16 + col];
                    
                    if (elapsedTicks >= rowStartTick) {
                        uint32_t moveDuration = absSpeed - rowStartTick;
                        if (moveDuration == 0) moveDuration = 1;
                        int pixelsToShift = (int)((128 * (uint64_t)(elapsedTicks - rowStartTick)) / moveDuration);
                        
                        if (pixelsToShift > 128) {
                            mixedByte = RPUWPC_PrimaryBuffer[frame][row * 16 + col];
                        } else if (direction == 1) {
                            int byteOffset = pixelsToShift / 8;
                            int bitOffset = pixelsToShift % 8;
                            if (byteOffset > col) {
                                mixedByte = RPUWPC_PrimaryBuffer[frame][row * 16 + col];
                            } else if (byteOffset == col) {
                                mixedByte = RPUWPC_PrimaryBuffer[frame][row * 16 + col] & (0xFF >> (8 - bitOffset));
                                mixedByte |= RPUWPC_WipeSourceBuffer[frame][row * 16] << bitOffset;
                            } else {
                                mixedByte = RPUWPC_WipeSourceBuffer[frame][row * 16 + (col - 1) - byteOffset] >> (8 - bitOffset);
                                mixedByte |= RPUWPC_WipeSourceBuffer[frame][row * 16 + col - byteOffset] << bitOffset;
                            }
                        } else {
                            int byteOffset = pixelsToShift / 8;
                            int bitOffset = pixelsToShift % 8;
                            if (col > (15 - byteOffset)) {
                                mixedByte = RPUWPC_PrimaryBuffer[frame][row * 16 + col];
                            } else if (col == (15 - byteOffset)) {
                                mixedByte = RPUWPC_PrimaryBuffer[frame][row * 16 + col] & (0xFF << (8 - bitOffset));
                                mixedByte |= RPUWPC_WipeSourceBuffer[frame][row * 16 + 15] >> bitOffset;
                            } else {
                                mixedByte = RPUWPC_WipeSourceBuffer[frame][row * 16 + col + byteOffset] >> bitOffset;
                                mixedByte |= RPUWPC_WipeSourceBuffer[frame][row * 16 + col + byteOffset + 1] << (8 - bitOffset);
                            }
                        }
                    }
                    RPUWPC_RenderBuffer[frame][row * 16 + col] = mixedByte;
                }
            }
        } else if (direction == 2 || direction == -2) {
            int dirSign = (direction == -2) ? -1 : 1; 
            for (uint8_t row = 0; row < 32; row++) {
                uint8_t *rowPtr;
                int rowOffset = (elapsedTicks * 32) / absSpeed;
                if (dirSign == -1) {
                    if (row > (31 - rowOffset)) rowPtr = &RPUWPC_PrimaryBuffer[frame][row * 16];
                    else rowPtr = &RPUWPC_WipeSourceBuffer[frame][(row + rowOffset) * 16];
                } else {
                    if (row < rowOffset) rowPtr = &RPUWPC_PrimaryBuffer[frame][row * 16];
                    else rowPtr = &RPUWPC_WipeSourceBuffer[frame][(row - rowOffset) * 16];
                }
                for (int col = 0; col < 16; col++) {
                    RPUWPC_RenderBuffer[frame][row * 16 + col] = rowPtr[col];
                }
            }
        }
    }
}


void RPU_WPC_DisplayUpdateImmediate(void) {
    // Write the current active draw buffer directly to the front (visible) pages
    for (int frame = 0; frame < 4; frame++) {
        WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_FrontPage[frame]);
        RPU_WPC_WriteDelay();
        
        for (int count = 0; count < 512; count++) {
            WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START + count, ActiveDrawBuffer[frame][count]);
            RPU_WPC_WriteDelay();
        }
    }
    
    // Return multiplexer to a stable state for legacy routines
    WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_BackPage[0]);
    RPU_WPC_WriteDelay();
    
    // Clear the dirty flag since we just manually flushed to hardware
    DisplayDirty = false; 
}


void RPU_WPC_DisplayUpdate(bool firqStatus, uint32_t currentTicks) {
    if (!firqStatus) return;

    // 1. VBLANK CRITICAL SECTION (Scanline 31)
    if (PendingPageFlip) {
        for (int i = 0; i < 4; i++) {
            uint8_t temp = RPUWPC_FrontPage[i];
            RPUWPC_FrontPage[i] = RPUWPC_BackPage[i];
            RPUWPC_BackPage[i] = temp;
        }
        
        WriteDisplay(WPC_DMD_LOW_PAGE, RPUWPC_FrontPage[0]);
        RPU_WPC_WriteDelay();
        
        PendingPageFlip = false;
    }

    // Immediately apply the active frame to the hardware
    RPUWPC_ActiveFrame++;
    if (RPUWPC_ActiveFrame >= 6) {
        RPUWPC_ActiveFrame = 0;
    }
    
    // Sequence mappings to pulse-width modulate 5 levels into 4 memory buffers across 6 hardware frames 
    static const uint8_t FrameToBuffer[6] = {0, 1, 2, 3, 2, 1}; 
    
    WriteDisplay(WPC_DMD_ACTIVE_PAGE, RPUWPC_FrontPage[FrameToBuffer[RPUWPC_ActiveFrame]]);
    RPU_WPC_WriteDelay();
    WriteDisplay(WPC_DMD_SCANLINE, 31); // Re-arm the FIRQ trigger
    RPU_WPC_WriteDelay();

    // 2. BACKGROUND RENDERING
    bool needHardwareUpdate = false;
    uint8_t (*targetRender)[512] = RPUWPC_PrimaryBuffer;

    if (WipeRunning) {
        uint32_t elapsed = currentTicks - WipeStartTicks;
        if (elapsed >= WipeDuration) {
            WipeRunning = false;
            targetRender = RPUWPC_PrimaryBuffer;
            for (int i = 0; i < 4; i++) {
                memcpy(RPUWPC_RenderBuffer[i], RPUWPC_PrimaryBuffer[i], 512);
            }
            needHardwareUpdate = true;
        } else {
            ComputeWipe(elapsed);
            targetRender = RPUWPC_RenderBuffer;
            needHardwareUpdate = true;
        }
    } else if (AlternateSolid) {
        targetRender = RPUWPC_AlternateBuffer;
        if (DisplayDirty) {
            for (int i = 0; i < 4; i++) {
                memcpy(RPUWPC_RenderBuffer[i], RPUWPC_AlternateBuffer[i], 512);
            }
            needHardwareUpdate = true;
        }
    } else if (AlternateFrequency > 0) {
        bool showAlt = ((currentTicks / AlternateFrequency) % 2) == 0;
        targetRender = showAlt ? RPUWPC_AlternateBuffer : RPUWPC_PrimaryBuffer;
        
        static bool lastShowAlt = false;
        if (showAlt != lastShowAlt || DisplayDirty) {
            lastShowAlt = showAlt;
            for (int i = 0; i < 4; i++) {
                memcpy(RPUWPC_RenderBuffer[i], targetRender[i], 512);
            }
            needHardwareUpdate = true;
        }
    } else {
        targetRender = RPUWPC_PrimaryBuffer;
        if (DisplayDirty) {
            for (int i = 0; i < 4; i++) {
                memcpy(RPUWPC_RenderBuffer[i], RPUWPC_PrimaryBuffer[i], 512);
            }
            needHardwareUpdate = true;
        }
    }

    if (needHardwareUpdate) {
        // Bulk write to the BACK pages
        for (int frame = 0; frame < 4; frame++) {
            WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_BackPage[frame]);
            RPU_WPC_WriteDelay();
            for (int count = 0; count < 512; count++) {
                WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START + count, targetRender[frame][count]);
                RPU_WPC_WriteDelay();
            }
        }
        
        // Return the HIGH_PAGE multiplexer to a stable default 
        WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_BackPage[0]);
        RPU_WPC_WriteDelay();
        
        // Queue the flip for the NEXT vertical blanking interval.
        PendingPageFlip = true;
        DisplayDirty = false;
    }
}

static inline void DrawPixelToBuffer(uint8_t buffer[4][512], int x, int y, uint8_t color) {
    if (x < 0 || x >= 128 || y < 0 || y >= 32) return;
    int idx = (y * 16) + (x >> 3);
    uint8_t bit = 1 << (x & 7);
    
    // Clean slate across all layers
    buffer[0][idx] &= ~bit;
    buffer[1][idx] &= ~bit;
    buffer[2][idx] &= ~bit;
    buffer[3][idx] &= ~bit;
    
    // Map values (0-4) onto the overlapping buffers 
    if (color == 1) {
        buffer[0][idx] |= bit;
    } else if (color == 2) {
        buffer[0][idx] |= bit;
        buffer[3][idx] |= bit;
    } else if (color == 3) {
        buffer[0][idx] |= bit;
        buffer[2][idx] |= bit;
    } else if (color == 4) {
        buffer[0][idx] |= bit;
        buffer[1][idx] |= bit;
        buffer[2][idx] |= bit;
        buffer[3][idx] |= bit;
    }
}

void RPU_WPC_DisplayDrawPixel(uint8_t x, uint8_t y, uint8_t color) {
    DrawPixelToBuffer(ActiveDrawBuffer, x, y, color);
}

void RPU_WPC_DrawHorizontalLine(int x1, int x2, int y, uint8_t color) {
    if (x1 > 127 || x2 > 127 || y > 31 || x1 < 0 || x2 < 0 || y < 0) return;
    for (int count = x1; count < x2; count++) {
        RPU_WPC_DisplayDrawPixel(count, y, color);
    }
}

void RPU_WPC_DrawRectangle(int offset, bool filled, uint8_t color) {
    if (offset > 15) return;
    if (!filled) {   
        for (int count = offset; count < (512 - offset * 2); count++) {
            RPU_WPC_DisplayDrawPixel(count, offset, color);
            RPU_WPC_DisplayDrawPixel(count, 31 - offset, color);
            if (count < (32 - offset * 2)) {
                RPU_WPC_DisplayDrawPixel(offset, count, color);
                RPU_WPC_DisplayDrawPixel(127 - offset, count, color);
            }
        }
    } else {
        for (int count = offset; count < (512 - offset * 2); count++) {
            for (int row = offset; row < (32 - offset); row++) {
                RPU_WPC_DisplayDrawPixel(count, row, color);
            }
        }
    }
}

void RPU_WPC_DrawRectangleDotted(int offset, bool filled, uint8_t color) {
    if (offset > 15) return;
    if (!filled) {   
        for (int count = offset; count < (512 - offset * 2); count++) {
            if (((count + offset) % 2) == 0) RPU_WPC_DisplayDrawPixel(count, offset, color);
            if (((count + 31 - offset) % 2) == 0) RPU_WPC_DisplayDrawPixel(count, 31 - offset, color);
            if (count < (32 - offset * 2)) {
                if (((count + offset) % 2) == 0) RPU_WPC_DisplayDrawPixel(offset, count, color);
                if (((count + 127 - offset) % 2) == 0) RPU_WPC_DisplayDrawPixel(127 - offset, count, color);
            }
        }
    } else {
        for (int count = offset; count < (512 - offset * 2); count++) {
            for (int row = offset; row < (32 - offset); row++) {
                RPU_WPC_DisplayDrawPixel(count, row, color);
            }
        }
    }
}

void RPU_WPC_DrawRectangleXY(int x, int y, int width, int height, bool filled, uint8_t color) {
    for (int col = x; col < (x + width); col++) {
        for (int row = y; row < (y + height); row++) {
            if (filled) {
                RPU_WPC_DisplayDrawPixel(col, row, color);
            } else {
                if (row == y || row == (y + height - 1) || col == x || col == (x + width - 1)) {
                    RPU_WPC_DisplayDrawPixel(col, row, color);
                }
            }
        }
    }
}

static void DrawTextWrappedToBuffer(uint8_t buffer[4][512], int x1, int y1, int x2, int y2, int x, int y, const char *msg, uint8_t color) {
    int curX = x;
    int curY = y;

    while (*msg != '\0') {
        char c = *msg++;
        if (c == '\r') { curX = x1; continue; }
        if (c == '\n') { curX = x1; curY += 8; continue; }
        if (curX + 5 > x2) { curX = x1; curY += 8; }
        if (curY + 7 > y2) break;

        if (c >= 0x20 && c <= 0x7E) {
            uint8_t fontIdx = c - 0x20;
            for (int col = 0; col < 5; col++) {
                uint8_t columnData = RPUWPC_FONT_5x7[fontIdx][col];
                for (int row = 0; row < 7; row++) {
                    if (columnData & (1 << row)) {
                        int px = curX + col;
                        int py = curY + row;
                        if (px >= x1 && px <= x2 && py >= y1 && py <= y2) {
                            DrawPixelToBuffer(buffer, px, py, color);
                        }
                    }
                }
            }
            curX += 6;
        }
    }
}

void RPU_WPC_DrawTextToScratch(const char *message, uint8_t color) {
    DrawTextWrappedToBuffer(ActiveDrawBuffer, 3, 0, 127, 31, 3, 3, message, color);
}

void RPU_WPC_DrawTextToScratchXY(const char *message, int xCorner, int yCorner, uint8_t color) {
    DrawTextWrappedToBuffer(ActiveDrawBuffer, xCorner, yCorner, 127, 31, xCorner, yCorner, message, color);
}

void RPU_WPC_DisplayDrawArrow(int x, int y, int direction, uint8_t color) {
    int dx = 0, dy = 0, perpX = 0, perpY = 0;
    switch (direction) {
        case 0: dy = 1;  perpX = 1; break;
        case 1: dx = -1; perpY = 1; break;
        case 2: dy = -1; perpX = 1; break;
        case 3: dx = 1;  perpY = 1; break;
        default: return;
    }
    for (int step = 0; step < 3; step++) {
        int width = (step * 2) + 1; 
        int curX = x + (dx * step);
        int curY = y + (dy * step);
        int halfWidth = width / 2;
        for (int w = -halfWidth; w <= halfWidth; w++) {
            RPU_WPC_DisplayDrawPixel(curX + (perpX * w), curY + (perpY * w), color);
        }
    }
    for (int step = 3; step < 6; step++) {
        RPU_WPC_DisplayDrawPixel(x + (dx * step), y + (dy * step), color);
    }
}

void RPU_WPC_DisplayTextWithLogo(const char *message, uint8_t color) {
    RPU_WPC_DisplaySetDrawTarget(0); 
    
    for (int frame = 0; frame < 4; frame++) {
        memcpy(RPUWPC_PrimaryBuffer[frame], HPLogoFrame, 512);
    }
    
    DrawTextWrappedToBuffer(RPUWPC_PrimaryBuffer, 40, 3, 126, 29, 40, 3, message, color);
    
    RPU_WPC_DisplaySetAlternateMode(0, false);
    RPU_WPC_DisplayUpdateImmediate();
}

bool RPU_WPC_DisplayShowLogoScreen(int pageNum) {
    RPU_WPC_DisplaySetDrawTarget(0);
    
    for (int frame = 0; frame < 4; frame++) {
        memcpy(RPUWPC_PrimaryBuffer[frame], HPLogoFrame, 512);
    }
    
    RPU_WPC_DisplaySetAlternateMode(0, false);
    RPU_WPC_DisplayUpdateImmediate();
    
    return true;
}

uint32_t RPUWPCDisplayLogoBaseTickCount = 0;
void RPU_WPC_DisplayShowLogo(int pageNum, uint32_t tickCount) {
    if (RPUWPCDisplayLogoBaseTickCount == 0 || (tickCount - 20000000) > RPUWPCDisplayLogoBaseTickCount) {
        RPUWPCDisplayLogoBaseTickCount = tickCount;
        RPU_WPC_DisplayShowLogoScreen(pageNum);
    }
}

const unsigned char HPLogoFrame[512] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x80, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xC0, 0x07, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xF0, 0x1F, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xF8, 0x3F, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xFC, 0xFF, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x82, 0x87, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0xC2, 0x87, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0xF2, 0x87, 0xFF, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0xFA, 0x87, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x82, 0x87, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x82, 0xC7, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x82, 0xC7, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x87, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x07, 0xF8, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x07, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x07, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x07, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x07, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x07, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x07, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xE7, 0x1F, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xE7, 0xFF, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xE6, 0xFF, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xE6, 0xFF, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0xE6, 0xFF, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 
    0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Font data: 5 bytes per character. 
// Format: Column-major, 1 byte = 1 vertical column (LSB = top pixel)
// Full 5x7 Font Array (ASCII 0x20 - 0x7E)
const unsigned char RPUWPC_FONT_5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 0x20 (Space)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x24, 0x24, 0x24, 0x24, 0x24}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \ // Backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x08, 0x04, 0x08, 0x10, 0x08}  // ~
};