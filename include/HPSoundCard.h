#ifndef HOMEPIN_SOUND_CARD_H
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t byte;

#define MAX_MESSAGE_LEN             32
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

bool HPSoundCardPresent();
char *HPSoundCardVersion();


#define HOMEPIN_SOUND_CARD_H
#endif