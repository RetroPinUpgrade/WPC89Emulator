#include "HPSoundCard.h"
#include "gd32f4xx.h"


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

#define CMD_GET_VERSION             1
#define CMD_GET_SYS_INFO            2
#define CMD_TRACK_CONTROL           3
#define CMD_STOP_ALL                4
#define CMD_MASTER_VOLUME           5
#define CMD_TRACK_VOLUME            8
#define CMD_AMP_POWER               9
#define CMD_TRACK_FADE              10
#define CMD_RESUME_ALL_SYNC         11
#define CMD_SAMPLERATE_OFFSET       12      
#define CMD_TRACK_CONTROL_EX        13
#define CMD_SET_REPORTING           14
#define CMD_SET_TRIGGER_BANK        15

#define TRK_PLAY_SOLO               0
#define TRK_PLAY_POLY               1
#define TRK_PAUSE                   2
#define TRK_RESUME                  3
#define TRK_STOP                    4
#define TRK_LOOP_ON                 5
#define TRK_LOOP_OFF                6
#define TRK_LOAD                    7

#define RSP_VERSION_STRING          129
#define RSP_SYSTEM_INFO             130
#define RSP_STATUS                  131
#define RSP_TRACK_REPORT            132

#define SOM1                        0xf0
#define SOM2                        0xaa
#define EOM                         0x55      

#define HPSC_BACKGROUND_MUSIC_NOT_PLAYING   0xFFFF
uint16_t HPSCVoiceTable[MAX_NUM_VOICES];
uint8_t HPSCrxMessage[MAX_MESSAGE_LEN];
char HPSCVerion[VERSION_STRING_LEN];
uint16_t HPSCNumTracks;
uint8_t HPSCNumVoices;
uint8_t HPSCrxCount;
uint8_t HPSCrxLen;

uint8_t HPSCMusicVolume = 31;
uint8_t HPSCSFXVolume = 31;
uint8_t HPSCCalloutsVolume = 31;

bool HPSCrxMsgReady;
bool HPSCVerionRcvd;
bool HPSCSysinfoRcvd;
bool HPSCDataReady = false;
uint8_t HPSCNextDataByte = 0xFF;
uint8_t HPSCNumberOfBytesInMessage = 0;
uint8_t HPSCIncomingMessage[10];
int HPSCBackgroundMusic = HPSC_BACKGROUND_MUSIC_NOT_PLAYING;
int HPSCBackgroundMusicGain = 0;

uint32_t HPSCStartTicksOfMessage = 0;

#define HPSC_PRIMARY_VOLUME_MASK        0x000000FF
#define HPSC_MUSIC_VOLUME_MASK          0x0000FF00
#define HPSC_SFX_VOLUME_MASK            0x00FF0000
#define HPSC_CALLOUTS_VOLUME_MASK       0xFF000000

#define WPC_SOUND_VERSION_REQUEST         95
#define HPSC_VERSION_NUMMBER              209
#define WPC_SOUND_VOLUME_COMMMAND         121
#define WPC_SOUND_CALLOUT_INCOMING        122
#define WPC_SOUND_STOP_ALL_TRACKS         125
#define WPC_SOUND_STOP_BACKGROUND         126
#define WPC_SOUND_STOP_SOUND              127
#define WPC_SOUND_STOP_BACKGROUND_3       86
#define WPC_SOUND_MUSIC_VOLUME_START      96    // This will be followed by XX - 64
#define WPC_SOUND_MUSIC_VOLUME_END        111
#define WPC_SOUND_DUCKING_RELEASE         65

#define WPC_SOUND_COMMAND_TIMEOUT         5000

#define HPSC_SOUND_TYPE_UNKNOWN             0
#define HPSC_SOUND_TYPE_BACKGROUND_MUSIC    1
#define HPSC_SOUND_TYPE_CALLOUT             2
#define HPSC_SOUND_TYPE_MUSIC               3
#define HPSC_SOUND_TYPE_SOUND_FX            4

bool HPSCVersionRequestSeen = false;

#define HPSC_NUMBER_OF_BACKGROUND_MUSIC_TRACKS  18
#define HPSC_NUMBER_OF_ONE_SHOT_MUSIC_TRACKS    2
#define HPSC_NUMBER_OF_CALLOUT_TRACKS           3
#define HPSC_NUMBER_OF_SOUND_EFFECTS_TRACKS     3
int HPSCBackgroundMusicTracks[HPSC_NUMBER_OF_BACKGROUND_MUSIC_TRACKS] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 54};
int HPSCOneShotMusicTracks[HPSC_NUMBER_OF_ONE_SHOT_MUSIC_TRACKS] = {3, 5};
int HPSCCalloutTracks[HPSC_NUMBER_OF_CALLOUT_TRACKS] = {26, 31, 72};
int HPSCSoundEffectsTracks[HPSC_NUMBER_OF_SOUND_EFFECTS_TRACKS] = {86, 142, 143};

// Unmapped
// 86 ?
// 142 credits (single?)
// 143 credits (center slot / multiple?, also max?)

void HPSoundCardTrackControl(int trk, int code);
void HPSoundCardTrackControlLock(int trk, int code, bool lock);
void HPSoundCardFlush(void);

__attribute__((always_inline)) static inline uint8_t SerialReadByte(void) {
    while (RESET == usart_flag_get(USART1, USART_FLAG_RBNE));
    return (uint8_t)usart_data_receive(USART1);
}

__attribute__((always_inline)) static inline bool SerialAvailable(void) {
    return (SET == usart_flag_get(USART1, USART_FLAG_RBNE));
}

__attribute__((always_inline)) static inline void SerialWriteByte(uint8_t data) {
    while (RESET == usart_flag_get(USART1, USART_FLAG_TBE));
    usart_data_transmit(USART1, (uint16_t)data);
}

__attribute__((always_inline)) static inline void SerialWriteBuf(uint8_t *buf, uint8_t numbytes) {
    for (uint8_t count=0; count<numbytes; count++) {
        SerialWriteByte((uint8_t)*buf);
        buf += 1;
    }
}



/**
 * Maps linear volume (0-255) to signed 16-bit decibels (dB).
 * 0       -> -70 dB
 * 230     -> 0 dB (Unity)
 * 255     -> +15 dB
 * * Note: Values will repeat because the input range (256) 
 * is larger than the output range (85).
 */
static const int16_t HPSCVolumeToGain[32] = {
  -70, -68, -65, -63, -61, -59, -56, -54, 
  -52, -50, -47, -45, -43, -41, -38, -36, 
  -34, -32, -29, -27, -25, -23, -20, -18, 
  -16, -14, -11, -9, -7, -5, -2, 0
};

int HPSCGetGainFromLevel(byte level) {
  if (level>31) level = 31;
  return HPSCVolumeToGain[level];
}

int HPSCGetBackgroundGainFromLevel(byte level) {
  level = (111-level)*2 + 1;
  if (level>31) level = 31;
  return HPSCVolumeToGain[level];
}


struct HPSCReturnMessages {
  uint32_t respondAfterticks;
  uint8_t response;
};
struct HPSCReturnMessages HPSReturnMessageQueue[10];

void HPSCClearReturnMessagesQueue() {
  for (int count=0; count<10; count++) {
    HPSReturnMessageQueue[count].respondAfterticks = 0;
    HPSReturnMessageQueue[count].response = 0;
  }
}

bool HPSCPushMessageToQueue(uint32_t ticks, uint8_t message) {
  for (int count=0; count<10; count++) {
    if (HPSReturnMessageQueue[count].respondAfterticks==0) {
      HPSReturnMessageQueue[count].respondAfterticks = ticks;
      HPSReturnMessageQueue[count].response = message;
      return true;
    }
  }
  return false;
}

int HPSCGetMessageFromQueue(uint32_t ticks) {
  for (int count=0; count<10; count++) {
    if (HPSReturnMessageQueue[count].respondAfterticks && ticks>HPSReturnMessageQueue[count].respondAfterticks) {
      HPSReturnMessageQueue[count].respondAfterticks = 0;
      return HPSReturnMessageQueue[count].response;
    }
  }
  return 256;
}


bool HPSoundCardInitConnection() {
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART1);

    /* Configure TX as AF, Push-Pull */
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_2);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

    /* Configure RX as AF, Push-Pull */
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_3);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_3);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);

    usart_deinit(USART1);
    usart_baudrate_set(USART1, 57600U);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    /* Disable Hardware Flow Control */
    usart_hardware_flow_cts_config(USART1, USART_CTS_DISABLE);
    usart_hardware_flow_rts_config(USART1, USART_RTS_DISABLE);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);

    /* Enable USART1 */
    usart_enable(USART1);

    uint8_t txbuf[5];

    HPSCVerionRcvd = false;
    HPSCSysinfoRcvd = false;
    HPSoundCardFlush();

    // Request HPSCVerion string
    txbuf[0] = SOM1;
    txbuf[1] = SOM2;
    txbuf[2] = 0x05;
    txbuf[3] = CMD_GET_VERSION;
    txbuf[4] = EOM;
    SerialWriteBuf(txbuf, 5);

    // Request system info
    txbuf[0] = SOM1;
    txbuf[1] = SOM2;
    txbuf[2] = 0x05;
    txbuf[3] = CMD_GET_SYS_INFO;
    txbuf[4] = EOM;
    SerialWriteBuf(txbuf, 5);  

    HPSoundCardStopAllTracks();
    byte volumeIndex = RTC_BKP9 & HPSC_PRIMARY_VOLUME_MASK;
    if (volumeIndex>31) volumeIndex = 20;        

    HPSCMusicVolume = (RTC_BKP9 & HPSC_MUSIC_VOLUME_MASK)>>8;
    HPSCSFXVolume = (RTC_BKP9 & HPSC_SFX_VOLUME_MASK)>>16;
    HPSCCalloutsVolume = (RTC_BKP9 & HPSC_CALLOUTS_VOLUME_MASK)>>24;
    if (HPSCMusicVolume>100) HPSCMusicVolume = 100;
    if (HPSCSFXVolume>100) HPSCSFXVolume = 100;
    if (HPSCCalloutsVolume>100) HPSCCalloutsVolume = 100;
    if (HPSCMusicVolume<5) HPSCMusicVolume = 5;
    if (HPSCSFXVolume<5) HPSCSFXVolume = 5;
    if (HPSCCalloutsVolume<5) HPSCCalloutsVolume = 5;
    
    // put sanitized values back in RTC_BKP9
    RTC_BKP9 = volumeIndex + (HPSCMusicVolume<<8) + (HPSCSFXVolume<<16) + (HPSCCalloutsVolume<<24);

    HPSCClearReturnMessagesQueue();
    HPSCPushMessageToQueue(20753100, 1);
    HPSoundCardMasterGain(HPSCGetGainFromLevel(volumeIndex));
    HPSoundCardTrackPlayPoly(999); // startup bong
    return true;
}


void HPSoundCardFlush(void) {
  int i;
  HPSCrxCount = 0;
  HPSCrxLen = 0;
  HPSCrxMsgReady = false;
  for (i = 0; i < MAX_NUM_VOICES; i++) {
    HPSCVoiceTable[i] = 0xffff;
  }
  while(SerialAvailable()) SerialReadByte();
}


void HPSoundCardStopAllBackgroundTracks() {
  for (int count=0; count<HPSC_NUMBER_OF_BACKGROUND_MUSIC_TRACKS; count++) {
    HPSoundCardTrackStop(HPSCBackgroundMusicTracks[count]);
  }
  HPSCBackgroundMusic = HPSC_BACKGROUND_MUSIC_NOT_PLAYING;
}


void HPSoundCardStopBackgroundTrack() {
  HPSoundCardTrackStop(HPSCBackgroundMusic);  
  HPSCBackgroundMusic = HPSC_BACKGROUND_MUSIC_NOT_PLAYING;
}


void HPSoundCardPlayBackgroundTrack(int trackIndex) {
  if (HPSCBackgroundMusic!=HPSC_BACKGROUND_MUSIC_NOT_PLAYING) {
    // Stop current music before playing this one
    HPSoundCardStopBackgroundTrack();
  }
  HPSCBackgroundMusic = trackIndex;
  HPSoundCardTrackPlayPolyLock(HPSCBackgroundMusic, true);
  HPSoundCardTrackLoop(HPSCBackgroundMusic, true);
}

void HPSoundCardAdjustBackgroundTrackGain(int gain) {
  HPSCBackgroundMusicGain = gain;
  if (HPSCBackgroundMusic==HPSC_BACKGROUND_MUSIC_NOT_PLAYING) return;
  HPSoundCardTrackGain(HPSCBackgroundMusic, HPSCBackgroundMusicGain);
}



bool HPSoundCardIsBackgroundTrack(int trackIndex) {
  for (int count=0; count<HPSC_NUMBER_OF_BACKGROUND_MUSIC_TRACKS; count++) {
    if (trackIndex==HPSCBackgroundMusicTracks[count]) {
      return true;
    }
  }
  return false;
}


void HPSoundCardCheckMessageTimeout(uint32_t curTicks) {
  if (HPSCNumberOfBytesInMessage && curTicks>(HPSCStartTicksOfMessage+WPC_SOUND_COMMAND_TIMEOUT)) {
    HPSCNumberOfBytesInMessage = 0;
    HPSCStartTicksOfMessage = 0;
  }
}


void HPSCStartCommand(uint8_t commandID, uint32_t curTicks) {
  HPSCIncomingMessage[0] = commandID;
  HPSCNumberOfBytesInMessage = 1;
  HPSCStartTicksOfMessage = curTicks;
}


void HPSoundCardHandleCommand(byte command, uint32_t curTicks) {

  HPSoundCardCheckMessageTimeout(curTicks);

  if (HPSCNumberOfBytesInMessage==0) {
    // single byte commands or first byte in multi-byte message
    if (command==WPC_SOUND_VERSION_REQUEST) {
      HPSCPushMessageToQueue(curTicks, HPSC_VERSION_NUMMBER);
    } else if (command==WPC_SOUND_STOP_BACKGROUND || command==WPC_SOUND_STOP_BACKGROUND_3) {
      HPSoundCardStopAllBackgroundTracks();
    } else if (command==WPC_SOUND_VOLUME_COMMMAND) {
      // start of a multi-byte command, so we just record it and move on
      HPSCStartCommand(WPC_SOUND_VOLUME_COMMMAND, curTicks);
    } else if (command==WPC_SOUND_STOP_ALL_TRACKS) {
      HPSCStartCommand(WPC_SOUND_STOP_ALL_TRACKS, curTicks);
    } else if (command==WPC_SOUND_CALLOUT_INCOMING) {
      HPSCStartCommand(WPC_SOUND_CALLOUT_INCOMING, curTicks);
    } else if (command>=WPC_SOUND_MUSIC_VOLUME_START && command<=WPC_SOUND_MUSIC_VOLUME_END) {
      HPSCStartCommand(command, curTicks);
    } else if (command==WPC_SOUND_DUCKING_RELEASE) {
      HPSoundCardTrackFade(HPSCBackgroundMusic, 0, 2000, false);
      HPSCPushMessageToQueue(curTicks+15978237, 10);
      HPSCPushMessageToQueue(curTicks+21815467, 10);
    } else if (command==WPC_SOUND_STOP_SOUND) {
      HPSoundCardStopAllTracks();
    } else { 
      // This command is atomic (one and done)
      uint8_t soundType = HPSC_SOUND_TYPE_UNKNOWN;
      if (soundType==HPSC_SOUND_TYPE_UNKNOWN && HPSoundCardIsBackgroundTrack(command)) {
        HPSoundCardPlayBackgroundTrack(command);
        soundType = HPSC_SOUND_TYPE_BACKGROUND_MUSIC;
      }
      for (int count=0; soundType==HPSC_SOUND_TYPE_UNKNOWN && count<HPSC_NUMBER_OF_ONE_SHOT_MUSIC_TRACKS; count++) {
        if (command==HPSCOneShotMusicTracks[count]) {
          soundType = HPSC_SOUND_TYPE_MUSIC;
          HPSoundCardTrackPlayPolyLock(command, true);
        }
      }
      if (soundType==HPSC_SOUND_TYPE_UNKNOWN) {
        // We're going to assume that this track is a SFX track
        soundType = HPSC_SOUND_TYPE_SOUND_FX;
        HPSoundCardTrackPlayPoly(command);
      }
    }
  } else {
    // This is non-zero if we're building up a message
    if (HPSCIncomingMessage[0]==WPC_SOUND_VOLUME_COMMMAND) {
      // This is a volume call, which means that 
      // byte 1 is volume and byte 2 is 255-volume
      if (HPSCNumberOfBytesInMessage==1) {
        HPSCIncomingMessage[1] = command;
        HPSCNumberOfBytesInMessage += 1;
      } else {
        HPSCNumberOfBytesInMessage = 0;
        uint8_t newVol = HPSCIncomingMessage[1];
        if (newVol>31) newVol = 31;
        RTC_BKP9 = (RTC_BKP9&(~HPSC_PRIMARY_VOLUME_MASK)) | newVol;
        HPSoundCardMasterGain(HPSCGetGainFromLevel(HPSCIncomingMessage[1]));
      }
    } else if (HPSCIncomingMessage[0]==WPC_SOUND_STOP_ALL_TRACKS) {
      if (command==127) {
        HPSoundCardStopAllTracks();
        HPSCNumberOfBytesInMessage = 0;
        HPSCBackgroundMusic = HPSC_BACKGROUND_MUSIC_NOT_PLAYING;
      }
    } else if (HPSCIncomingMessage[0]==WPC_SOUND_CALLOUT_INCOMING) {
      HPSCNumberOfBytesInMessage = 0;
      HPSoundCardTrackPlayPoly(command + 500);
    } else if (HPSCIncomingMessage[0]>=WPC_SOUND_MUSIC_VOLUME_START && HPSCIncomingMessage[0]<=WPC_SOUND_MUSIC_VOLUME_END) {
      // Make sure the checksum works out
      if ((HPSCIncomingMessage[0]-command)==0x40) {
        // The command check value (0x40) is good, so we can apply volume
        // Set the volume of the background music
        if (HPSCIncomingMessage[0]>=(WPC_SOUND_MUSIC_VOLUME_END)) {
          HPSoundCardStopBackgroundTrack();
        } else {
          HPSoundCardAdjustBackgroundTrackGain(HPSCGetBackgroundGainFromLevel(HPSCIncomingMessage[0]));
        }  
      }
      HPSCNumberOfBytesInMessage = 0;
    } else {
      HPSCNumberOfBytesInMessage = 0;
    }    
  }

}

int HPSCStartupBytesSent = 0;
byte HPSCNextOutboundByte = 0x00;

bool HPSoundCardCheckForOutboundByte(unsigned long curTicks) {

  int queueEntry = HPSCGetMessageFromQueue(curTicks);
  if (queueEntry<256) {
    HPSCNextOutboundByte = (uint8_t)queueEntry;
    return true;
  }
  return false;
}

byte HPSoundCardGetOutboundByte() {
  HPSCStartupBytesSent += 1;
  return HPSCNextOutboundByte;
}


void HPSoundCardUpdate(void) {

  int i;
  uint8_t dat;
  uint8_t voice;
  uint16_t track;

  HPSCrxMsgReady = false;
  while (SerialAvailable() > 0) {
    dat = SerialReadByte();
    if ((HPSCrxCount == 0) && (dat == SOM1)) {
      HPSCrxCount++;
    }
    else if (HPSCrxCount == 1) {
      if (dat == SOM2)
        HPSCrxCount++;
      else {
        HPSCrxCount = 0;
        //Serial.print("Bad msg 1\n");
      }
    }
    else if (HPSCrxCount == 2) {
      if (dat <= MAX_MESSAGE_LEN) {
        HPSCrxCount++;
        HPSCrxLen = dat - 1;
      }
      else {
        HPSCrxCount = 0;
        //Serial.print("Bad msg 2\n");
      }
    }
    else if ((HPSCrxCount > 2) && (HPSCrxCount < HPSCrxLen)) {
      HPSCrxMessage[HPSCrxCount - 3] = dat;
      HPSCrxCount++;
    }
    else if (HPSCrxCount == HPSCrxLen) {
      if (dat == EOM)
        HPSCrxMsgReady = true;
      else {
        HPSCrxCount = 0;
        //Serial.print("Bad msg 3\n");
      }
    }
    else {
      HPSCrxCount = 0;
      //Serial.print("Bad msg 4\n");
    }

    if (HPSCrxMsgReady) {
      switch (HPSCrxMessage[0]) {

        case RSP_TRACK_REPORT:
          track = HPSCrxMessage[2];
          track = (track * 256) + HPSCrxMessage[1] + 1;
          voice = HPSCrxMessage[3];
          if (voice < MAX_NUM_VOICES) {
            if (HPSCrxMessage[4] == 0) {
              if (track == HPSCVoiceTable[voice])
                HPSCVoiceTable[voice] = 0xffff;
            }
            else
              HPSCVoiceTable[voice] = track;
          }
          // ==========================
          //Serial.print("Track ");
          //Serial.print(track);
          //if (HPSCrxMessage[4] == 0)
          //  Serial.print(" off\n");
          //else
          //  Serial.print(" on\n");
          // ==========================
        break;

        case RSP_VERSION_STRING:
          for (i = 0; i < (VERSION_STRING_LEN - 1); i++)
            HPSCVerion[i] = HPSCrxMessage[i + 1];
          HPSCVerion[VERSION_STRING_LEN - 1] = 0;
          HPSCVerionRcvd = true;
          // ==========================
//          Serial.write("WAV Version: ");
//          Serial.write(HPSCVerion);
//          Serial.write("\n");
          // ==========================
        break;

        case RSP_SYSTEM_INFO:
          HPSCNumVoices = HPSCrxMessage[1];
          HPSCNumTracks = HPSCrxMessage[3];
          HPSCNumTracks = (HPSCNumTracks * 256) + HPSCrxMessage[2];
          HPSCSysinfoRcvd = true;
          // ==========================
          ///\Serial.print("Sys info received\n");
          // ==========================
        break;

      }
      HPSCrxCount = 0;
      HPSCrxLen = 0;
      HPSCrxMsgReady = false;

    } // if (HPSCrxMsgReady)

  } // while (SerialAvailable() > 0)
  
}


bool HPSoundCardIsTrackPlaying(int trk) {
  int i;
  bool fResult = false;
  HPSoundCardUpdate();
  for (i = 0; i < MAX_NUM_VOICES; i++) {
    if (HPSCVoiceTable[i] == ((uint16_t)trk))
      fResult = true;
  }
  return fResult;
}


int HPSoundCardGetPlayingTrack(int voiceNum) {
  if (voiceNum>=MAX_NUM_VOICES || voiceNum<0) return 0xFFFF;
  return (HPSCVoiceTable[voiceNum]);
}




void HPSoundCardMasterGain(int gain) {
  uint8_t txbuf[7];
  unsigned short vol;
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x07;
  txbuf[3] = CMD_MASTER_VOLUME;
  vol = (unsigned short)gain;
  txbuf[4] = (uint8_t)vol;
  txbuf[5] = (uint8_t)(vol >> 8);
  txbuf[6] = EOM;
  SerialWriteBuf(txbuf, 7);
}


void HPSoundCardSetAmpPwr(bool enable) {
  uint8_t txbuf[6];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x06;
  txbuf[3] = CMD_AMP_POWER;
  txbuf[4] = enable;
  txbuf[5] = EOM;
  SerialWriteBuf(txbuf, 6);
}


void HPSoundCardSetReporting(bool enable) {
  uint8_t txbuf[6];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x06;
  txbuf[3] = CMD_SET_REPORTING;
  txbuf[4] = enable;
  txbuf[5] = EOM;
  SerialWriteBuf(txbuf, 6);
}


bool HPSoundCardGetVersion(char *pDst, int len) {
  int i;
  HPSoundCardUpdate();
  if (!HPSCVerionRcvd) {
    return false;
  }
  for (i = 0; i < (VERSION_STRING_LEN - 1); i++) {
    if (i >= (len - 1))
      break;
    pDst[i] = HPSCVerion[i];
  }
  pDst[++i] = 0;
  return true;
}


int HPSoundCardGetNumTracks(void) {
  HPSoundCardUpdate();
  return HPSCNumTracks;
}


void HPSoundCardTrackPlaySolo(int trk) {
  HPSoundCardTrackControl(trk, TRK_PLAY_SOLO);
}


void HPSoundCardTrackPlaySoloLock(int trk, bool lock) {
  HPSoundCardTrackControlLock(trk, TRK_PLAY_SOLO, lock);
}


void HPSoundCardTrackPlayPoly(int trk) {
  HPSoundCardTrackControl(trk, TRK_PLAY_POLY);
}


void HPSoundCardTrackPlayPolyLock(int trk, bool lock) {
  HPSoundCardTrackControlLock(trk, TRK_PLAY_POLY, lock);
}


void HPSoundCardTrackLoad(int trk) {
  HPSoundCardTrackControl(trk, TRK_LOAD);
}


void HPSoundCardTrackLoadLock(int trk, bool lock) {
  HPSoundCardTrackControlLock(trk, TRK_LOAD, lock);
}


void HPSoundCardTrackStop(int trk) {
  HPSoundCardTrackControl(trk, TRK_STOP);
}


void HPSoundCardTrackPause(int trk) {
  HPSoundCardTrackControl(trk, TRK_PAUSE);
}


void HPSoundCardTrackResume(int trk) {
  HPSoundCardTrackControl(trk, TRK_RESUME);
}


void HPSoundCardTrackLoop(int trk, bool enable) {
  if (enable)
    HPSoundCardTrackControl(trk, TRK_LOOP_ON);
  else
    HPSoundCardTrackControl(trk, TRK_LOOP_OFF);
}


void HPSoundCardTrackControl(int trk, int code) {
  uint8_t txbuf[8];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x08;
  txbuf[3] = CMD_TRACK_CONTROL;
  txbuf[4] = (uint8_t)code;
  txbuf[5] = (uint8_t)trk;
  txbuf[6] = (uint8_t)(trk >> 8);
  txbuf[7] = EOM;
  SerialWriteBuf(txbuf, 8);
}


void HPSoundCardTrackControlLock(int trk, int code, bool lock) {
  uint8_t txbuf[9];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x09;
  txbuf[3] = CMD_TRACK_CONTROL_EX;
  txbuf[4] = (uint8_t)code;
  txbuf[5] = (uint8_t)trk;
  txbuf[6] = (uint8_t)(trk >> 8);
  txbuf[7] = lock;
  txbuf[8] = EOM;
  SerialWriteBuf(txbuf, 9);
}


void HPSoundCardStopAllTracks(void) {
  uint8_t txbuf[5];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x05;
  txbuf[3] = CMD_STOP_ALL;
  txbuf[4] = EOM;
  SerialWriteBuf(txbuf, 5);
}


void HPSoundCardResumeAllInSync(void) {
  uint8_t txbuf[5];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x05;
  txbuf[3] = CMD_RESUME_ALL_SYNC;
  txbuf[4] = EOM;
  SerialWriteBuf(txbuf, 5);
}


void HPSoundCardTrackGain(int trk, int gain) {
  uint8_t txbuf[9];
  unsigned short vol;
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x09;
  txbuf[3] = CMD_TRACK_VOLUME;
  txbuf[4] = (uint8_t)trk;
  txbuf[5] = (uint8_t)(trk >> 8);
  vol = (unsigned short)gain;
  txbuf[6] = (uint8_t)vol;
  txbuf[7] = (uint8_t)(vol >> 8);
  txbuf[8] = EOM;
  SerialWriteBuf(txbuf, 9);
}


void HPSoundCardTrackFade(int trk, int gain, int time, bool stopFlag) {
  uint8_t txbuf[12];
  unsigned short vol;
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x0c;
  txbuf[3] = CMD_TRACK_FADE;
  txbuf[4] = (uint8_t)trk;
  txbuf[5] = (uint8_t)(trk >> 8);
  vol = (unsigned short)gain;
  txbuf[6] = (uint8_t)vol;
  txbuf[7] = (uint8_t)(vol >> 8);
  txbuf[8] = (uint8_t)time;
  txbuf[9] = (uint8_t)(time >> 8);
  txbuf[10] = stopFlag;
  txbuf[11] = EOM;
  SerialWriteBuf(txbuf, 12);
}


void HPSoundCardSamplerateOffset(int offset) {
  uint8_t txbuf[7];
  unsigned short off;
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x07;
  txbuf[3] = CMD_SAMPLERATE_OFFSET;
  off = (unsigned short)offset;
  txbuf[4] = (uint8_t)off;
  txbuf[5] = (uint8_t)(off >> 8);
  txbuf[6] = EOM;
  SerialWriteBuf(txbuf, 7);
}


void HPSoundCardSetTriggerBank(int bank) {
  uint8_t txbuf[6];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x06;
  txbuf[3] = CMD_SET_TRIGGER_BANK;
  txbuf[4] = (uint8_t)bank;
  txbuf[5] = EOM;
  SerialWriteBuf(txbuf, 6);
}

