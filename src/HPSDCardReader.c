#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "gd32f4xx.h"
#include "HPSDCardReader.h"
#include "ff.h"


#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "gd32f4xx.h"


#include "asic.h"

// Extern declaration to reach the 4KB RAM buffer in mpu89.c
extern uint8_t* MPUGetNVRAMStart(void);
extern uint16_t MPUGetNVRAMSize(void);

// FatFs hardcoded dependency for file timestamps
// Returning a fixed dummy time: Jan 1, 2024, 00:00:00
DWORD get_fattime(void) {
    // bit31:25 Year offset from 1980 (2024 - 1980 = 44)
    // bit24:21 Month (1)
    // bit20:16 Day (1)
    // bit15:11 Hour (0)
    // bit10:5  Minute (0)
    // bit4:0   Second / 2 (0)
    return ((DWORD)(2024 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}

// User-defined ROM Metadata (40 bytes)
typedef struct {
    uint32_t sanity_value;
    uint32_t fileSize;
    char rom_id[32];
} RomMetadata_t;

// Master layout for the 128KB metadata sector (Sector 11)
typedef struct {
    // 1. ROM Metadata (padded to a predictable 256-byte boundary)
    RomMetadata_t romMeta;
    uint8_t _pad1[216]; 

    // 2. RAM Map JSON (Header + 64k reserved space)
    uint32_t jsonSanity;
    char jsonFilename[32];
    uint32_t jsonSize;
    uint8_t _pad2[216];
    uint8_t jsonData[65536];

    // 3. INI Settings (Header + 16KB reserved space)
    uint32_t iniSanity;
    char iniFilename[32];
    uint32_t iniSize;
    uint8_t _pad3[216];
    uint8_t iniData[16384];

    // 4. Remaining empty space to complete the 128KB sector
    uint8_t _reserved[48384];
} Sector11Metadata_t;

// Global pointer for easy read access anywhere in your code
const Sector11Metadata_t* const metadataPtr = (const Sector11Metadata_t*)0x080E0000;
const uint8_t* const HPSDRomData = (const uint8_t*)0x08100000;




// Calculates the checksum of the first and last 1K of the ROM data
uint32_t CalculateRomChecksum(uint32_t fileSize) {
    uint32_t checkSum = 0;
    
    // Safety check to ensure we have at least 2KB of data
    if (fileSize < 2048) return 0;

    for (uint32_t count = 0; count < 1024; count++) {
        checkSum += HPSDRomData[count] + HPSDRomData[fileSize - 1 - count];
    }
    return checkSum;
}

// Erases Sector 11 and writes the entire 128KB RAM buffer to flash
uint8_t WriteSector11(Sector11Metadata_t* ramCopy) {
    uint32_t flashAddress = 0x080E0000;
    uint32_t* dataPtr = (uint32_t*)ramCopy;
    uint32_t wordsToWrite = sizeof(Sector11Metadata_t) / 4;

    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);

    // Erase Sector 11 (128KB)
    fmc_sector_erase(CTL_SECTOR_NUMBER_11);
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);

    // Write the buffer word-by-word
    for (uint32_t i = 0; i < wordsToWrite; i++) {
        fmc_word_program(flashAddress, dataPtr[i]);
        fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
        flashAddress += 4;
    }

    fmc_lock();
    return 1; // Success
}


// Updates just the ROM metadata portion in flash using a temporary RAM buffer
void HPSDUpdateRomMetadata(const char* newRomId, uint32_t newFileSize) {
    
    // 1. Allocate the 128KB buffer on the heap
    Sector11Metadata_t* tempBuffer = (Sector11Metadata_t*)malloc(sizeof(Sector11Metadata_t));
    if (!tempBuffer) return; // Out of memory, abort
    
    // 2. Copy the existing 128KB flash sector to our RAM buffer
    memcpy(tempBuffer, metadataPtr, sizeof(Sector11Metadata_t));

    // 3. Update just the ROM metadata fields in the RAM copy
    tempBuffer->romMeta.fileSize = newFileSize;
    tempBuffer->romMeta.sanity_value = CalculateRomChecksum(newFileSize);
    
    // Copy the filename, ensuring null termination
    strncpy(tempBuffer->romMeta.rom_id, newRomId, 31);
    tempBuffer->romMeta.rom_id[31] = '\0';

    // 4. Write the modified 128KB buffer back to Sector 11
    WriteSector11(tempBuffer);

    // 5. Free the temporary RAM to prevent memory leaks
    free(tempBuffer);
}



uint8_t HPSDCheckROMIntegrity() {
    uint32_t checkSum = 0;
    uint32_t fileSize = metadataPtr->romMeta.fileSize;
    
    // Safety check
    if (fileSize < 2048) return 0; 

    for (uint32_t count=0; count<1024; count++) {
        checkSum += HPSDRomData[count] + HPSDRomData[fileSize - 1 - count];
    }
    if (checkSum==metadataPtr->romMeta.sanity_value) return 1;
    else return 0;    
}

const char *HPSDGetROMName() {
    return metadataPtr->romMeta.rom_id;
}


// Helper function to get pointer to start of ROM
uint8_t* HPSDGetROMPointer(void) {
    return (uint8_t *)HPSDRomData;
}

uint32_t HPSDGetROMSize(void) {
    return (uint32_t)metadataPtr->romMeta.fileSize;
}


/* 1. Declare the file system object globally */
FATFS fs;

void HPSDCardPortInit() {
    /* 1. Enable GPIO and SDIO Clocks */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_SDIO);

    /* 2. Configure Card Detect (PA15) */
    // Set as Input with an internal pull-up resistor
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_15);

    /* 3. Configure SDIO Data Lines D0-D3 (PC8, PC9, PC10, PC11) */
    // Assign Alternate Function 12 (SDIO)
    gpio_af_set(GPIOC, GPIO_AF_12, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11);
    // Set as Alternate Function, Pull-up enabled
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11);
    // Set output type to Push-Pull, High Speed
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11);

    /* 4. Configure SDIO Clock Line CK (PC12) */
    // Assign Alternate Function 12
    gpio_af_set(GPIOC, GPIO_AF_12, GPIO_PIN_12);
    // Set as Alternate Function, No Pull-up (standard for clock)
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_12);
    // Set output type to Push-Pull, High Speed
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);

    /* 5. Configure SDIO Command Line CMD (PD2) */
    // Assign Alternate Function 12
    gpio_af_set(GPIOD, GPIO_AF_12, GPIO_PIN_2);
    // Set as Alternate Function, Pull-up enabled
    gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
    // Set output type to Push-Pull, High Speed
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

    f_mount(&fs, "", 1);
}

/* Returns true if the SD card is detected on PA15 */
bool HPSDCardInserted(void) {
    /* Assuming the CD switch pulls the pin LOW (RESET) when inserted */
    if (gpio_input_bit_get(GPIOA, GPIO_PIN_15) == RESET) {
        return true;
    }
    return false;
}




// Helper to perform case-insensitive extension comparison
static bool MatchExtension(const char* filename, const char* targetExtension) {
    const char* ext = strrchr(filename, '.');
    
    if (ext == NULL || targetExtension == NULL) {
        return false;
    }
    
    // Skip the dot in the filename
    ext++; 

    // Compare character by character, ignoring case
    while (*ext && *targetExtension) {
        if (tolower((unsigned char)*ext) != tolower((unsigned char)*targetExtension)) {
            return false;
        }
        ext++;
        targetExtension++;
    }
    
    // Return true if both strings ended at the exact same length
    return (*ext == '\0' && *targetExtension == '\0');
}

uint16_t HPSDCardGetFileNames(char fileList[][13], uint16_t maxFiles, const char* targetExtension) {
    DIR dir;
    FILINFO fno;
    FRESULT res;
    uint16_t count = 0;

    // Open the root directory
    res = f_opendir(&dir, "/");
    if (res == FR_OK) {
        for (;;) {
            // Read a directory item
            res = f_readdir(&dir, &fno);
            
            // Break on error or end of directory
            if (res != FR_OK || fno.fname[0] == '\0') {
                break;
            }

            // Filter out hidden files and macOS metadata files (starts with '.', '_', or contains '~')
            if ((fno.fattrib & AM_HID) || fno.fname[0] == '.' || fno.fname[0] == '_' || strchr(fno.fname, '~') != NULL) {
                continue;
            }

            // Check if it is a file (not a directory)
            if (!(fno.fattrib & AM_DIR)) {
                
                // Accept the file if targetExtension is empty/NULL, or if the extension matches
                if (targetExtension == NULL || targetExtension[0] == '\0' || MatchExtension(fno.fname, targetExtension)) {
                    if (count < maxFiles) {
                        // Copy the filename into the provided array
                        strncpy(fileList[count], fno.fname, 12);
                        fileList[count][12] = '\0';
                        count++;
                    }
                }
            }
        }
        f_closedir(&dir);
    }
    
    return count;
}



#define ROM_START_ADDR 0x08100000
#define MAX_ROM_SIZE (1024 * 1024) // 1MB maximum for Bank 1


uint32_t HPSDFlashRomFromSD(const char* filename, RomProgressCallback progressCb) {
    FIL file;
    FRESULT res;
    UINT bytesRead;
    uint32_t flashAddress = ROM_START_ADDR;
    uint32_t buffer[128]; 

    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) return 0;

    uint32_t fileSize = f_size(&file);
    if (fileSize > MAX_ROM_SIZE) {
        f_close(&file);
        return 0; 
    }

    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);

    uint32_t sectors[] = {
        CTL_SECTOR_NUMBER_12, CTL_SECTOR_NUMBER_13, CTL_SECTOR_NUMBER_14, 
        CTL_SECTOR_NUMBER_15, CTL_SECTOR_NUMBER_16, CTL_SECTOR_NUMBER_17, 
        CTL_SECTOR_NUMBER_18, CTL_SECTOR_NUMBER_19, CTL_SECTOR_NUMBER_20,
        CTL_SECTOR_NUMBER_21, CTL_SECTOR_NUMBER_22, CTL_SECTOR_NUMBER_23
    };
    
    uint32_t sectorSizes[] = {
        16384, 16384, 16384, 16384,    
        65536,                         
        131072, 131072, 131072,        
        131072, 131072, 131072, 131072 
    };

    uint32_t capacityErased = 0;
    for (int i = 0; i < 12; i++) {
        if (capacityErased >= fileSize) break;
        
        // Trigger callback for the erase phase
        if (progressCb) progressCb(capacityErased, fileSize, 1);
        
        fmc_sector_erase(sectors[i]);
        fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
        capacityErased += sectorSizes[i];
    }

    uint32_t totalWritten = 0;

    do {
        res = f_read(&file, buffer, sizeof(buffer), &bytesRead);
        if (res != FR_OK || bytesRead == 0) break;

        uint32_t wordsToWrite = (bytesRead + 3) / 4; 

        if (bytesRead % 4 != 0) {
            uint8_t* bytePointer = (uint8_t*)buffer;
            for (uint32_t i = bytesRead; i < wordsToWrite * 4; i++) {
                bytePointer[i] = 0xFF;
            }
        }

        for (uint32_t i = 0; i < wordsToWrite; i++) {
            fmc_word_program(flashAddress, buffer[i]);
            fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
            flashAddress += 4;
        }
        
        totalWritten += bytesRead;
        
        // Trigger callback for the write phase
        if (progressCb) progressCb(totalWritten, fileSize, 0);
        
    } while (bytesRead == sizeof(buffer) && flashAddress < (ROM_START_ADDR + MAX_ROM_SIZE));

    fmc_lock();
    f_close(&file);

    return fileSize;
}




bool HPSDSaveGameState(const char* description) {
    const char *romName = HPSDGetROMName();
    if (romName == NULL || romName[0] == '\0') {
        return false;
    }

    if (description == NULL) {
        description = "SAVE";
    }

    size_t baseLen = strlen(romName);
    if (baseLen >= 4) {
        const char* ext = &romName[baseLen - 4];
        if (ext[0] == '.' && (ext[1] == 'R' || ext[1] == 'r') && 
            (ext[2] == 'O' || ext[2] == 'o') && (ext[3] == 'M' || ext[3] == 'm')) {
            baseLen -= 4;
        }
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "%.*s-%.10s.RAM", (int)baseLen, romName, description);

    char path[128];
    FILINFO fno;

    if (f_stat(filename, &fno) == FR_OK) {
        strncpy(path, filename, sizeof(path));
        path[sizeof(path) - 1] = '\0';
    } else {
        f_mkdir("/GameStateFiles");
        snprintf(path, sizeof(path), "/GameStateFiles/%s", filename);
    }

    // 1. Made static to pull the 512-byte sector buffer off the stack
    static FIL file; 
    FRESULT res = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) return false;

    UINT bytesWritten;
    res = f_write(&file, MPUGetNVRAMStart(), MPUGetNVRAMSize(), &bytesWritten);

    // 2. Force the data through the driver
    f_sync(&file);

    // 3. Dummy delay to prevent the SDIO driver from dropping the final FAT updates.
    // (Adjust the loop size based on your MCU clock speed to yield ~50ms)
    for(volatile uint32_t delay = 0; delay < 2000000; delay++) {
        __asm("nop");
    }

    FRESULT closeRes = f_close(&file);
    return (res == FR_OK && closeRes == FR_OK && bytesWritten == MPUGetNVRAMSize());
}


bool HPSDLoadGameState(const char* filename) {
    if (filename == NULL || filename[0] == '\0') {
        return false;
    }

    FIL file;
    FRESULT res;
    char path[128];

    // Try to open the file in the GameStateFiles sub-folder first
    snprintf(path, sizeof(path), "/GameStateFiles/%s", filename);
    res = f_open(&file, path, FA_READ);

    // If it fails, fallback to the root directory
    if (res != FR_OK) {
        res = f_open(&file, filename, FA_READ);
    }

    // If it still failed to open in either location, abort
    if (res != FR_OK) return false;

    UINT bytesRead;
    // Overwrite the live NVRAM with the saved payload
    res = f_read(&file, MPUGetNVRAMStart(), MPUGetNVRAMSize(), &bytesRead);
    f_close(&file);

    return (res == FR_OK && bytesRead == MPUGetNVRAMSize());
}


uint16_t HPSDGetGameStateFiles(const char* romName, char fileList[][13], uint16_t maxFiles) {
    DIR dir;
    FILINFO fno;
    FRESULT res;
    uint16_t count = 0;

    if (romName == NULL || romName[0] == '\0') return 0;
    
    // Determine the base length of the ROM name, stripping ".ROM" if present
    size_t baseLen = strlen(romName);
    if (baseLen >= 4) {
        const char* ext = &romName[baseLen - 4];
        if (ext[0] == '.' && 
            (ext[1] == 'R' || ext[1] == 'r') && 
            (ext[2] == 'O' || ext[2] == 'o') && 
            (ext[3] == 'M' || ext[3] == 'm')) {
            baseLen -= 4;
        }
    }

    // Define the directories to search
    const char* searchPaths[] = {"/GameStateFiles", "/"};
    
    for (int p = 0; p < 2; p++) {
        if (count >= maxFiles) break;

        res = f_opendir(&dir, searchPaths[p]);
        if (res == FR_OK) {
            for (;;) {
                // Read a directory item
                res = f_readdir(&dir, &fno);
                
                // Break on error or end of directory
                if (res != FR_OK || fno.fname[0] == '\0') {
                    break;
                }

                // Filter out directories, hidden files, and macOS metadata files
                if ((fno.fattrib & AM_DIR) || (fno.fattrib & AM_HID) || fno.fname[0] == '.' || fno.fname[0] == '_' || strchr(fno.fname, '~') != NULL) {
                    continue;
                }
                
                // Ensure the file has a .RAM extension
                if (MatchExtension(fno.fname, "ram")) {
                    
                    // Ensure the filename starts with the exact target ROM base name
                    if (strncmp(fno.fname, romName, baseLen) == 0) {
                        
                        // Enforce the ROMNAME-XXXX structure by checking for the hyphen
                        if (fno.fname[baseLen] == '-') {
                            
                            // Extract just the description (strip ROMNAME- and .RAM)
                            const char* descStart = &fno.fname[baseLen + 1];
                            const char* extStart = strrchr(descStart, '.');
                            size_t descLen = (extStart != NULL) ? (size_t)(extStart - descStart) : strlen(descStart);
                            
                            // Bound length to fit within fileList[][13] (12 chars + null terminator)
                            if (descLen > 12) {
                                descLen = 12;
                            }
                            
                            char tempDesc[13];
                            strncpy(tempDesc, descStart, descLen);
                            tempDesc[descLen] = '\0';
                            
                            // Check for duplicates to ensure a file in both folders is only listed once
                            bool isDuplicate = false;
                            for (uint16_t i = 0; i < count; i++) {
                                if (strcmp(fileList[i], tempDesc) == 0) {
                                    isDuplicate = true;
                                    break;
                                }
                            }
                            
                            if (!isDuplicate && count < maxFiles) {
                                // Copy the parsed description into the provided array
                                strncpy(fileList[count], tempDesc, 13);
                                count++;
                            }
                        }
                    }
                }
            }
            f_closedir(&dir);
        }
    }
    
    return count;
}

bool HPSDGetDescription(const char *filename, char *description, int maxDescriptionLen) {
    if (filename == NULL || description == NULL || maxDescriptionLen <= 0) {
        return false;
    }

    const char *romName = HPSDGetROMName();
    if (romName == NULL || romName[0] == '\0') {
        return false;
    }

    // Determine the base length of the ROM name, stripping ".ROM" if present
    size_t baseLen = strlen(romName);
    if (baseLen >= 4) {
        const char* ext = &romName[baseLen - 4];
        if (ext[0] == '.' && 
            (ext[1] == 'R' || ext[1] == 'r') && 
            (ext[2] == 'O' || ext[2] == 'o') && 
            (ext[3] == 'M' || ext[3] == 'm')) {
            baseLen -= 4;
        }
    }

    // Check if the filename begins with the exact target ROM base name
    if (strncmp(filename, romName, baseLen) != 0) {
        return false;
    }

    // Ensure the next character is the expected hyphen 
    if (filename[baseLen] != '-') {
        return false;
    }

    // The description starts immediately after the hyphen
    const char *descStart = &filename[baseLen + 1];

    // Find the start of the file extension (e.g., .RAM) to exclude it
    const char *extStart = strrchr(descStart, '.');
    size_t descLen;

    if (extStart != NULL) {
        descLen = extStart - descStart;
    } else {
        // Fallback if the file somehow lacks an extension
        descLen = strlen(descStart);
    }

    // Bound the length to prevent buffer overflows
    if ((int)descLen >= maxDescriptionLen) {
        descLen = maxDescriptionLen - 1;
    }

    // Copy the description into the buffer and null-terminate
    strncpy(description, descStart, descLen);
    description[descLen] = '\0';

    return true;
}