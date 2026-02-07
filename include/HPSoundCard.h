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
void HPSoundCardHandleCommand(byte command);
bool HPSoundCardCheckForOutboundByte(unsigned long curTicks);
byte HPSoundCardGetOutboundByte();
void HPSoundCardSetReporting(bool enable);
void HPSoundCardSetAmpPwr(bool enable);
bool HPSoundCardGetVersion(char *pDst, int len);
int  HPSoundCardGetNumTracks(void);
bool HPSoundCardIsTrackPlaying(int trk);
int  HPSoundCardGetPlayingTrack(int voiceNum);
void HPSoundCardMasterGain(int gain);
void HPSoundCardStopAllTracks(void);
void HPSoundCardResumeAllInSync(void);

// Tracks
void HPSoundCardTrackPlaySolo(int trk);
void HPSoundCardTrackPlaySoloLock(int trk, bool lock);
void HPSoundCardTrackPlayPoly(int trk);
void HPSoundCardTrackPlayPolyLock(int trk, bool lock);
void HPSoundCardTrackLoad(int trk);
void HPSoundCardTrackLoadLock(int trk, bool lock);

void HPSoundCardTrackStop(int trk);
void HPSoundCardTrackPause(int trk);
void HPSoundCardTrackResume(int trk);
void HPSoundCardTrackLoop(int trk, bool enable);
void HPSoundCardTrackGain(int trk, int gain);
void HPSoundCardTrackFade(int trk, int gain, int time, bool stopFlag);

// Settings
void HPSoundCardSamplerateOffset(int offset);
void HPSoundCardSetTriggerBank(int bank);


#define HOMEPIN_SOUND_CARD_H
#endif