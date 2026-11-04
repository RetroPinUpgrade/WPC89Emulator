#ifndef HOMEPIN_SOUND_CARD_H
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t byte;

#define MAX_MESSAGE_LEN             150
#define MAX_NUM_VOICES              14
#define VERSION_STRING_LEN          21

// Init
bool HPSoundCardInitConnection();
void HPSoundCardUpdate(void);
void HPSoundCardFlush(void);

// System
void HPSoundCardHandleCommand(byte command, uint32_t curTicks);
bool HPSoundCardCheckForOutboundByte(unsigned long curTicks);
byte HPSoundCardGetOutboundByte();
void HPSoundCardSetReporting(bool enable);
void HPSoundCardRequestVersion();

bool HPBasicWAVSoundCardPresent();
char *HPSoundCardVersion();

bool HPAdvancedWAVSoundCardPresent();
void HPSoundCardRequestSynthVol();
void HPSoundCardRequestDACVol();
void HPSoundCardRequestCVSDVol();
bool HPSoundCardCurSynthVol(int16_t *curLeft, int16_t *curRight);
bool HPSoundCardCurDACVol(int16_t *curLeft, int16_t *curRight);
bool HPSoundCardCurCVSDVol(int16_t *curLeft, int16_t *curRight);
void HPSoundCardSetSynthVol(int16_t leftVol, int16_t rightVol);
void HPSoundCardSetDACVol(int16_t leftVol, int16_t rightVol);
void HPSoundCardSetCVSDVol(int16_t leftVol, int16_t rightVol);
void HPSoundCardRequestNumROMs();
bool HPSoundCardCurNumROMs(uint16_t *numRoms);
void HPSoundCardRequestROMName(uint16_t romNum);
char *HPSoundCardCurROMName();
void HPSoundCardFlashROM(uint16_t romNum);
bool HPSoundCardCurFlashCode(uint8_t *retCode);
char *HPSoundCardCurFlashMessage();
void HPSoundCardRequestCurrentROMName(void);
char *HPSoundCardGetActiveROMName(void);

#define HOMEPIN_SOUND_CARD_H
#endif