#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define PIC_SERIAL_SIZE 16
#define WPC_PIC_RESET 0x00
#define WPC_PIC_COUNTER 0x0D
#define WPC_PIC_UNLOCK 0x20
#define INITIAL_BYTE 0xFF
#define DEFAULT_SCRAMBLER_VALUE 0xA5

// Internal state variables
static uint8_t unlockCode[3];
static int unlockCodeCounter = 0;
static uint8_t picUnlockBuffer[3];
static uint8_t picSerialNumber[PIC_SERIAL_SIZE];
static uint8_t originalPicSerialNumber[PIC_SERIAL_SIZE];
static uint8_t serialNumberScrambler = DEFAULT_SCRAMBLER_VALUE;
static uint8_t lastByteWrite = INITIAL_BYTE;
static uint8_t writesUntilUnlockNeeded = 0x20;
static bool securityPicInitialized = false;

static bool debugTrapActive = false;

static int LearnedGameID = 0;
void SecurityPICSetLearnedID(int id) {
    LearnedGameID = id;
}

int SecurityPICGetLearnedID() {
    return LearnedGameID;
}

// Initialize the PIC array and unlock code based on the 3-digit game ID
void SecurityPICInit(int machineNumber) {

    //machineNumber = 526;
    char gameSerialNumber[32];
    // Recreate the concatenated string: machineNumber + " 123456 12345 123"
    snprintf(gameSerialNumber, sizeof(gameSerialNumber), "%03d 123456 12345 123", machineNumber);

    memset(picSerialNumber, 0, PIC_SERIAL_SIZE);
    serialNumberScrambler = DEFAULT_SCRAMBLER_VALUE;

    picSerialNumber[10] = 0x12;
    picSerialNumber[2]  = 0x34;

    uint32_t tmp = 100 * (gameSerialNumber[1] - '0') +
                   10  * (gameSerialNumber[8] - '0') +
                         (gameSerialNumber[5] - '0');
    tmp += 5 * picSerialNumber[10];
    tmp = (tmp * 0x1BCD) + 0x1F3F0;
    picSerialNumber[1]  = (tmp >> 16) & 0xFF;
    picSerialNumber[11] = (tmp >> 8)  & 0xFF;
    picSerialNumber[9]  = tmp & 0xFF;

    tmp = 10000 * (gameSerialNumber[2] - '0') +
          1000  * (gameSerialNumber[18] - '0') +
          100   * (gameSerialNumber[0] - '0') +
          10    * (gameSerialNumber[9] - '0') +
                  (gameSerialNumber[7] - '0');
    tmp += 2 * picSerialNumber[10] + picSerialNumber[2];
    tmp = (tmp * 0x107F) + 0x71E259;
    picSerialNumber[7]  = (tmp >> 24) & 0xFF;
    picSerialNumber[12] = (tmp >> 16) & 0xFF;
    picSerialNumber[0]  = (tmp >> 8)  & 0xFF;
    picSerialNumber[8]  = tmp & 0xFF;

    tmp = 1000 * (gameSerialNumber[19] - '0') +
          100  * (gameSerialNumber[4] - '0') +
          10   * (gameSerialNumber[6] - '0') +
                 (gameSerialNumber[17] - '0');
    tmp += picSerialNumber[2];
    tmp = (tmp * 0x245) + 0x3D74;
    picSerialNumber[3]  = (tmp >> 16) & 0xFF;
    picSerialNumber[14] = (tmp >> 8)  & 0xFF;
    picSerialNumber[6]  = tmp & 0xFF;

    tmp = 10000 * ('9' - gameSerialNumber[15]) +
          1000  * ('9' - gameSerialNumber[14]) +
          100   * ('9' - gameSerialNumber[13]) +
          10    * ('9' - gameSerialNumber[12]) +
                  ('9' - gameSerialNumber[11]);
    picSerialNumber[15] = (tmp >> 8) & 0xFF;
    picSerialNumber[4]  = tmp & 0xFF;

    // Save the original generated sequence
    memcpy(originalPicSerialNumber, picSerialNumber, PIC_SERIAL_SIZE);

    tmp = 100 * (gameSerialNumber[0] - '0') +
          10  * (gameSerialNumber[1] - '0') +
                (gameSerialNumber[2] - '0');

    // Note: JS treats string numeric operations loosely. This strictly ports the 
    // numerical character values intended by the multiplier logic. 
    tmp = ((tmp >> 8) & 0xFF) * (0x100 * gameSerialNumber[17] + gameSerialNumber[19]) +
          (tmp & 0xFF) * (0x100 * gameSerialNumber[18] + gameSerialNumber[17]);

    unlockCode[0] = (tmp >> 16) & 0xFF;
    unlockCode[1] = (tmp >> 8)  & 0xFF;
    unlockCode[2] = (tmp)       & 0xFF;
}


void SecurityPICReset() {
    if (!securityPicInitialized) {
        return;
    }

    lastByteWrite = INITIAL_BYTE;
    writesUntilUnlockNeeded = 0x20;
    memcpy(picSerialNumber, originalPicSerialNumber, PIC_SERIAL_SIZE);
    
    // Explicitly apply the WPC_PIC_RESET hardware logic so 
    // bytes 5 and 13 are properly seeded even if the CPU's 0x00 is missed.
    serialNumberScrambler = DEFAULT_SCRAMBLER_VALUE;
    picSerialNumber[5] = picSerialNumber[0] ^ picSerialNumber[15];
    picSerialNumber[13] = picSerialNumber[2] ^ picSerialNumber[12];
}


bool TestUnlockCode(int testMachineNumber, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    char gameSerialNumber[32];
    snprintf(gameSerialNumber, sizeof(gameSerialNumber), "%03d 123456 12345 123", testMachineNumber);

    uint32_t tmp = 100 * (gameSerialNumber[0] - '0') +
                   10  * (gameSerialNumber[1] - '0') +
                         (gameSerialNumber[2] - '0');

    // FIX: Removed the - '0' to match SecurityPICInit
    tmp = ((tmp >> 8) & 0xFF) * (0x100 * gameSerialNumber[17] + gameSerialNumber[19]) +
          (tmp & 0xFF) * (0x100 * gameSerialNumber[18] + gameSerialNumber[17]);

    uint8_t expected0 = (tmp >> 16) & 0xFF;
    uint8_t expected1 = (tmp >> 8)  & 0xFF;
    uint8_t expected2 = (tmp)       & 0xFF;

    return (expected0 == byte0 && expected1 == byte1 && expected2 == byte2);
}


// Inside your SecurityPIC_Write function, update the unlock sequence handler:
bool SecurityPICWrite(uint8_t data, void (*setColFunction)(uint8_t)) {

    // Lazy init with dummy ID on the first write
    if (!securityPicInitialized) {
        SecurityPICInit(LearnedGameID);
        securityPicInitialized = true;
        SecurityPICReset();
    }

    lastByteWrite = data;
    if (lastByteWrite >= 0x16 && lastByteWrite <= 0x1F) {
        uint8_t colIndex = lastByteWrite - 0x16; // 0x16 becomes 0, 0x17 becomes 1, etc.
        uint8_t colMask = (1 << colIndex);       // 0->0x01, 1->0x02, 2->0x04, etc.
        setColFunction(colMask);
    } else if (debugTrapActive && data!=4 && data!=9 && data!=13 && data!=32) {
//        __asm volatile ("BKPT #0");
    }

    if (unlockCodeCounter > 0) {
        // Store incoming bytes into a temporary buffer
        // Note: unlockCodeCounter goes 3, 2, 1
        picUnlockBuffer[3 - unlockCodeCounter] = data;
        unlockCodeCounter--;


        if (unlockCodeCounter == 0) {
            // TRUE FAST PATH: Compare directly to the pre-calculated array. 
            // Zero math, zero string formatting, executes in 3 clock cycles.
            if (picUnlockBuffer[0] == unlockCode[0] &&
                picUnlockBuffer[1] == unlockCode[1] &&
                picUnlockBuffer[2] == unlockCode[2]) {
                
                // It matches, keep playing instantly without stalling the MCU
                return false; 
            }

            // SLOW PATH: We only get here if the board was moved to a new game.
            // Brute-force the new ID.
            for (int i = 0; i < 1000; i++) {
                if (TestUnlockCode(i, picUnlockBuffer[0], picUnlockBuffer[1], picUnlockBuffer[2])) {
                    SecurityPICSetLearnedID(i);
                    SecurityPICInit(i);
                    return true; // Signal reboot
                }
            }
            return false; // Unknown sequence
        }
        return false;
    }
    
    if (data == WPC_PIC_RESET) {
        serialNumberScrambler = DEFAULT_SCRAMBLER_VALUE;
        picSerialNumber[5] = picSerialNumber[0] ^ picSerialNumber[15];
        picSerialNumber[13] = picSerialNumber[2] ^ picSerialNumber[12];
        writesUntilUnlockNeeded = 0x20;
        return false;
    }

    if (data == WPC_PIC_UNLOCK) {
        unlockCodeCounter = 3;
        return false;
    }

    if (data == WPC_PIC_COUNTER) {
        writesUntilUnlockNeeded = (writesUntilUnlockNeeded - 1) & 0x1F;
        return false;
    }

    return false;
}

// Read Hook for WPC_PICREAD 
// getRowFunction is a pointer to your switch matrix read function
uint8_t SecurityPICRead(uint8_t (*getRowFunction)(void)) {
    // Lazy init with dummy ID on the first read
    if (!securityPicInitialized) {
        SecurityPICInit(LearnedGameID);
        securityPicInitialized = true;
    }

    if (lastByteWrite == WPC_PIC_COUNTER) {
        return writesUntilUnlockNeeded;
    }

    if (lastByteWrite >= 0x16 && lastByteWrite <= 0x1F) {
        if (getRowFunction != NULL) {
            return getRowFunction();
        }
        return 0;
    }

    if ((lastByteWrite & 0xF0) == 0x70) {
        uint8_t ret = picSerialNumber[lastByteWrite & 0x0F];
        
        // Advance the rolling encryption data dynamically 
        serialNumberScrambler = ((serialNumberScrambler >> 4) | (lastByteWrite << 4)) & 0xFF;
        picSerialNumber[5] = ((picSerialNumber[5] ^ serialNumberScrambler) + picSerialNumber[13]) & 0xFF;
        picSerialNumber[13] = ((picSerialNumber[13] + serialNumberScrambler) ^ picSerialNumber[5]) & 0xFF;
        
        return ret;
    }

    return 0;
}

uint8_t SecurityPIC_GetScrambler() {
    return serialNumberScrambler;
}