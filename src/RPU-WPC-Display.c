#include <stdbool.h>
#include <stdint.h>
#include "RPU-WPC-Display.h"
#include "mpu89.h"

extern const unsigned char HPLogoFrame[512];
extern const unsigned char RPUWPC_FONT_5x7[][5];
static uint8_t RPUWPC_ScratchBuffer[512];
uint8_t RPUWPC_ScratchBufferCopy[512];
uint8_t RPUWPC_FrontBufferNum = 0;
uint8_t RPUWPC_BackBufferNum = 1;

void RPU_WPC_DisplayClearScratchBuffer() {
    for (int count=0; count<512; count++) RPUWPC_ScratchBuffer[count] = 0x00;
}

#include <stdint.h>
#include <string.h>

extern uint8_t RPUWPC_ScratchBufferCopy[512];
extern uint8_t RPUWPC_ScratchBuffer[512];

/**
 * Shifts a specific row (16 bytes/128 pixels) from the Copy buffer to the main buffer.
 * LSb is left-most pixel.
 * shiftPixels > 0: Shift Right (towards MSb)
 * shiftPixels < 0: Shift Left (towards LSb)
 */
void RPU_WPC_ShiftScratchRow(uint8_t rowNum, int shiftPixels) {
    if (shiftPixels == 0 || rowNum >= 32) {
        return;
    }

    uint8_t *src = &RPUWPC_ScratchBufferCopy[rowNum * 16];
    uint8_t *dst = &RPUWPC_ScratchBuffer[rowNum * 16];
    
    // Initialize destination row to 0
    memset(dst, 0, 16);

    int absShift = (shiftPixels < 0) ? -shiftPixels : shiftPixels;
    int byteOffset = absShift / 8;
    int bitOffset = absShift % 8;

    // Entire row shifted off-screen
    if (byteOffset >= 16) {
        return;
    }

    if (shiftPixels > 0) {
        /* SHIFT RIGHT: Moving pixels towards the end of the row (Right side)
           In LSb-first, this is bit-shifting towards the MSb (0 -> 7) */
        for (int i = 0; i < 16 - byteOffset; i++) {
            uint8_t val = src[i];
            int targetIdx = i + byteOffset;

            // Current byte shift
            dst[targetIdx] |= (val << bitOffset);

            // Carry to next byte
            if (bitOffset > 0 && (targetIdx + 1) < 16) {
                dst[targetIdx + 1] |= (val >> (8 - bitOffset));
            }
        }
    } else {
        /* SHIFT LEFT: Moving pixels towards the start of the row (Left side)
           In LSb-first, this is bit-shifting towards the LSb (7 -> 0) */
        for (int i = byteOffset; i < 16; i++) {
            uint8_t val = src[i];
            int targetIdx = i - byteOffset;

            // Current byte shift
            dst[targetIdx] |= (val >> bitOffset);

            // Carry to previous byte
            if (bitOffset > 0 && (targetIdx - 1) >= 0) {
                dst[targetIdx - 1] |= (val << (8 - bitOffset));
            }
        }
    }
}

void RPU_WPC_DisplayUpdateCopyScratch() {
    for (int count=0; count<512; count++) {
        RPUWPC_ScratchBufferCopy[count] = RPUWPC_ScratchBuffer[count];        
    }
}




/**
 * LUT containing 32 starting offsets (0-255).
 * Represents the delay before each row begins its shift.
 * Index 0 = Top Row, Index 31 = Bottom Row.
 */
/*
static const uint8_t WIPE_SINE_LUT[32] = {
    0,   5,   12,  22,  34,  48,  64,  82, 
    100, 118, 136, 154, 172, 188, 203, 216, 
    227, 236, 243, 248, 251, 253, 254, 255,
    255, 254, 253, 251, 248, 243, 236, 227 // Example sine-like curve
};
*/

/**
 * Iteratively wipes the display.
 * @param speed Total ticks for full screen wipe (negative=left, positive=right)
 * @param elapsedTicks Ticks passed since the wipe began
 * @return true if wipe is active, false when complete
 */
bool RPU_WPC_DisplayWipe(int speed, uint32_t elapsedTicks) {
    if (speed == 0) return false;

    uint32_t absSpeed = (uint32_t)(speed < 0 ? -speed : speed);
    int direction = (speed < 0) ? -1 : 1;

    if (elapsedTicks==0) {
        // populate the backbuffer with full scratch copy
        for (int count=0; count<512; count++) {
            WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+count, RPUWPC_ScratchBufferCopy[count]);
        }    
        return true;
    }

    // Check if the global timer has exceeded the requested duration
    if (elapsedTicks >= absSpeed) {
        // populate the backbuffer with full scratch copy
        for (int count=0; count<512; count++) {
            WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+count, RPUWPC_ScratchBuffer[count]);
        }    
        return false;
    }

    for (uint8_t row = 0; row < 32; row++) {
        uint32_t rowStartTick = (absSpeed/(32*4))*row;
        for (uint8_t col=0; col<16; col++) {
            // Calculate the new byte for WriteDisplay
            uint8_t mixedByte = RPUWPC_ScratchBufferCopy[row*16 + col];
            if (elapsedTicks>=rowStartTick) {
                // this row has started moving, so we have to shift/mix
                // Calculate progress: how far the row should have moved since its start
                uint32_t moveDuration = absSpeed - rowStartTick;
                uint32_t timeElapsedSinceStart = elapsedTicks - rowStartTick;
                if (moveDuration == 0) moveDuration = 1; // Safety
    
                // Linear interpolation to 128 pixels
                int pixelsToShift = (int)((128 * (uint64_t)timeElapsedSinceStart) / moveDuration);
                                
                if (pixelsToShift > 128) {
                    mixedByte = RPUWPC_ScratchBuffer[row*16 + col];
                } else if (direction==1) {
                    int byteOffset = pixelsToShift / 8;
                    int bitOffset = pixelsToShift % 8;

                    if ((byteOffset)>col) {
                        mixedByte = RPUWPC_ScratchBuffer[row*16+col];
                    } else if (byteOffset==col) {
                        // This byte is a mix of the two buffers
                        mixedByte = RPUWPC_ScratchBuffer[row*16+col] & (0xFF>>(8-(bitOffset)));
                        mixedByte |= RPUWPC_ScratchBufferCopy[row*16] << (bitOffset);
                    } else {
                        mixedByte = RPUWPC_ScratchBufferCopy[row*16 + (col-1) - (byteOffset)]>>(8-(bitOffset));
                        mixedByte |= RPUWPC_ScratchBufferCopy[row*16 + col - (byteOffset)]<<(bitOffset);
                    }
                } else {
                    int byteOffset = pixelsToShift / 8;
                    int bitOffset = pixelsToShift % 8;
                
                    if (col > (15 - byteOffset)) {
                        // Trailing edge: This part of the screen is now showing the background
                        mixedByte = RPUWPC_ScratchBuffer[row * 16 + col];
                    } else if (col == (15 - byteOffset)) {
                        // Seam: The rightmost byte of the scrolling buffer meets the background
                        mixedByte = RPUWPC_ScratchBuffer[row * 16 + col] & (0xFF << (8 - bitOffset));
                        mixedByte |= RPUWPC_ScratchBufferCopy[row * 16 + 15] >> bitOffset;
                    } else {
                        // General case: Pulling from the copy buffer and shifting left
                        mixedByte = RPUWPC_ScratchBufferCopy[row * 16 + col + byteOffset] >> bitOffset;
                        mixedByte |= RPUWPC_ScratchBufferCopy[row * 16 + col + byteOffset + 1] << (8 - bitOffset);
                    }
                }
            }
            WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+(row*16 + col), mixedByte);
        }
    }

    return true;
}


bool RPU_WPC_DisplayWipeUD(int speed, uint32_t elapsedTicks) {
    if (speed == 0) return false;

    uint32_t absSpeed = (uint32_t)(speed < 0 ? -speed : speed);
    int direction = (speed < 0) ? -1 : 1;

    if (elapsedTicks==0) {
        // populate the backbuffer with full scratch copy
        for (int count=0; count<512; count++) {
            WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+count, RPUWPC_ScratchBufferCopy[count]);
        }    
        return true;
    }

    // Check if the global timer has exceeded the requested duration
    if (elapsedTicks >= absSpeed) {
        // populate the backbuffer with full scratch copy
        for (int count=0; count<512; count++) {
            WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+count, RPUWPC_ScratchBuffer[count]);
        }    
        return false;
    }

    for (uint8_t row = 0; row < 32; row++) {
        uint8_t *rowPtr;
        int rowOffset = (elapsedTicks*32) / absSpeed;
        if (direction==-1) {
            if (row>(31-rowOffset)) rowPtr = &RPUWPC_ScratchBuffer[row*16];
            else rowPtr = &RPUWPC_ScratchBufferCopy[(row+rowOffset)*16];
        } else {
            if (row<rowOffset) rowPtr = &RPUWPC_ScratchBuffer[row*16];
            else rowPtr = &RPUWPC_ScratchBufferCopy[(row-rowOffset)*16];
        }
        for (int col=0; col<16; col++) {
            WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+col + row*16, rowPtr[col]);
        }
    }
    return true;
}


void RPU_WPC_DisplayUpdateBackBuffer() {
    // push the scratch buffer to the back buffer
    for (int count=0; count<512; count++) {
        WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+count, RPUWPC_ScratchBuffer[count]);
    }    
}

void RPU_WPC_DisplayFlipBackToFront() {
    uint8_t tempBuffer = RPUWPC_FrontBufferNum;
    RPUWPC_FrontBufferNum = RPUWPC_BackBufferNum;
    RPUWPC_BackBufferNum = tempBuffer;

    WriteDisplay(WPC_DMD_LOW_PAGE, RPUWPC_FrontBufferNum);
    WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_BackBufferNum);
    WriteDisplay(WPC_DMD_ACTIVE_PAGE, RPUWPC_FrontBufferNum);
}


bool RPU_WPC_DisplayInit(){
    RPUWPC_FrontBufferNum = 14;
    RPUWPC_BackBufferNum = 15;
    WriteDisplay(WPC_DMD_LOW_PAGE, RPUWPC_FrontBufferNum);
    WriteDisplay(WPC_DMD_HIGH_PAGE, RPUWPC_BackBufferNum);
    WriteDisplay(WPC_DMD_ACTIVE_PAGE, RPUWPC_FrontBufferNum);
    for (int count=0; count<512; count++) {
        WriteDisplay(DISPLAY_RAM_LOWER_PAGE_START+count, 0x00);
        WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+count, 0x00);
    }

    WriteDisplay(WPC_DMD_SCANLINE, 0x40);
    return true;
}

bool RPU_WPC_DisplayShowLogoScreen(int pageNum) {
    for (int count=0; count<512; count++) {
        if (pageNum==0) WriteDisplay(DISPLAY_RAM_LOWER_PAGE_START+count, HPLogoFrame[count]);
        else if (pageNum==1) WriteDisplay(DISPLAY_RAM_UPPER_PAGE_START+count, HPLogoFrame[count]);
        for (int nopCount=0; nopCount<100; nopCount++) {
            __NOP();
        }
    }
    return true;
}

/**
 * @brief Draws a single pixel in the 128x32 buffer.
 * @note LSB-first packing: First pixel of the byte is the left-most.
 */
static inline void DrawPixelToScratch(uint8_t *buffer, int x, int y) {
    if (x < 0 || x >= 128 || y < 0 || y >= 32) return;
    // (y * 16) gets us to the row (128 pixels / 8 bits = 16 bytes per row)
    // (x >> 3) gets us to the correct byte in that row
    // (x & 7) gets us the bit index (0-7), where 0 is the LSB
    buffer[(y * 16) + (x >> 3)] |= (1 << (x & 7));
}

void RPU_WPC_DisplayDrawPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x < 0 || x >= 128 || y < 0 || y >= 32) return;
    // (y * 16) gets us to the row (128 pixels / 8 bits = 16 bytes per row)
    // (x >> 3) gets us to the correct byte in that row
    // (x & 7) gets us the bit index (0-7), where 0 is the LSB
    if (color) RPUWPC_ScratchBuffer[(y * 16) + (x >> 3)] |= (1 << (x & 7));
    else RPUWPC_ScratchBuffer[(y * 16) + (x >> 3)] &= ~(1 << (x & 7));
}


/**
 * @brief Prints a wrapped and clipped string into a 512-byte display buffer.
 * * @param buffer  Pointer to 512-byte display buffer
 * @param x1, y1  Upper-left of the bounding (clipping) box
 * @param x2, y2  Lower-right of the bounding (clipping) box
 * @param x, y    Start position of the first character
 * @param msg     Null-terminated string to print
 */
void DrawTextWrapped(uint8_t *buffer, int x1, int y1, int x2, int y2, int x, int y, const char *msg) {
    int curX = x;
    int curY = y;

    while (*msg != '\0') {
        char c = *msg++;

        // 1. Handle Carriage Return (\r)
        if (c == '\r') {
            curX = x1;
            continue;
        }

        // 2. Handle Newline (\n)
        if (c == '\n') {
            curX = x1;
            curY += 8;
            continue;
        }

        // 3. Handle Automatic Wrapping
        if (curX + 5 > x2) {
            curX = x1;
            curY += 8;
        }

        // 4. Global Y-Clipping
        if (curY + 7 > y2) break;

        // 5. Bounds Check: Ensure character is in the 0x20-0x7E range
        if (c >= 0x20 && c <= 0x7E) {
            uint8_t fontIdx = c - 0x20;
            
            for (int col = 0; col < 5; col++) {
                uint8_t columnData = RPUWPC_FONT_5x7[fontIdx][col];
                for (int row = 0; row < 7; row++) {
                    if (columnData & (1 << row)) {
                        int px = curX + col;
                        int py = curY + row;
                        // Local Bounding Box Clipping
                        if (px >= x1 && px <= x2 && py >= y1 && py <= y2) {
                            DrawPixelToScratch(buffer, px, py);
                        }
                    }
                }
            }
            curX += 6; // Space for character + 1px gutter
        }
    }
}

/**
 * @brief Clears a rectangular area in the 512-byte display buffer.
 * @param buffer Pointer to the 512-byte buffer.
 * @param x1, y1 Upper-left coordinates of the area to clear.
 * @param x2, y2 Lower-right coordinates of the area to clear.
 */
void ClearBufferArea(uint8_t *buffer, int x1, int y1, int x2, int y2) {
    // Bounds checking to prevent buffer overflow
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= 128) x2 = 127;
    if (y2 >= 32) y2 = 31;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            // (y * 16) finds the row start
            // (x >> 3) finds the byte
            // ~(1 << (x & 7)) creates a mask with a 0 at the target bit
            buffer[(y * 16) + (x >> 3)] &= ~(1 << (x & 7));
        }
    }
}


void RPU_WPC_DrawTextToScratch(const char *message) {
    DrawTextWrapped(RPUWPC_ScratchBuffer, 3, 0, 127, 31, 3, 3, message);
}

void RPU_WPC_DrawTextToScratchXY(const char *message, int xCorner, int yCorner) {
    DrawTextWrapped(RPUWPC_ScratchBuffer, xCorner, yCorner, 127, 31, xCorner, yCorner, message);
}


void RPU_WPC_DisplayTextWithLogo(const char *message) {
    for (int count=0; count<512; count++) {
        RPUWPC_ScratchBuffer[count] = HPLogoFrame[count];
    }
    DrawTextWrapped(RPUWPC_ScratchBuffer, 40, 3, 126, 29, 40, 3, message);
    for (int count=0; count<512; count++) {
        WriteDisplay(DISPLAY_RAM_LOWER_PAGE_START+count, RPUWPC_ScratchBuffer[count]);
    }
}


uint32_t RPUWPCDisplayLogoBaseTickCount = 0;
void RPU_WPC_DisplayShowLogo(int pageNum, uint32_t tickCount) {
    if (RPUWPCDisplayLogoBaseTickCount==0 || (tickCount-20000000)>RPUWPCDisplayLogoBaseTickCount) {
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
// Format: Column-major, 1 byte = 1 vertical column (LSB = top pixel)
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
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \ (Backslash)
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
    {0x08, 0x04, 0x08, 0x10, 0x08}  // ~ (Tilde)
};