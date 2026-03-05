#ifndef HP_SD_CARD_READER_H
#include <stdbool.h>

void HPSDCardPortInit();
bool HPSDCardInserted(void);
uint16_t HPSDCardGetFileNames(char fileList[][13], uint16_t maxFiles, const char* targetExtension);


typedef void (*RomProgressCallback)(uint32_t currentBytes, uint32_t totalBytes, uint8_t isErasing);
uint32_t HPSDFlashRomFromSD(const char* filename, RomProgressCallback progressCb);

//uint32_t HPSDFlashRomFromSD(const char* filename); // Returns 0 (fail), or size of ROM

void HPSDUpdateRomMetadata(const char* newRomId, uint32_t newFileSize);
uint8_t HPSDCheckROMIntegrity();
const char *HPSDGetROMName();
uint8_t* HPSDGetROMPointer(void);
uint32_t HPSDGetROMSize(void);

#define HP_SD_CARD_READER_H
#endif