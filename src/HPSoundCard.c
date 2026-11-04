#include "HPSoundCard.h"
#include "gd32f4xx.h"
#include <string.h>

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

#define CMD_GET_VERSION             0x01
#define CMD_GET_SYS_INFO            0x02
#define CMD_TRACK_CONTROL           0x03
#define CMD_STOP_ALL                0x04
#define CMD_MAIN_VOLUME             0x05 
#define CMD_AMP_POWER               0x09
#define CMD_TRACK_VOLUME            0x08
#define CMD_TRACK_FADE              0x0A
#define CMD_RESUME_ALL_SYNC         0x0B
#define CMD_SAMPLERATE_OFFSET       0x0C
#define CMD_SET_REPORTING           0x0D
#define CMD_SET_TRIGGER_BANK        0x0E
#define CMD_TRACK_CONTROL_EX        0x0F
#define CMD_SET_SYNTH_VOLUME        0x20 
#define CMD_SET_DAC_VOLUME          0x21 
#define CMD_SET_CVSD_VOLUME         0x22
#define CMD_GET_SYNTH_VOLUME        0x23 
#define CMD_GET_DAC_VOLUME          0x24 
#define CMD_GET_CVSD_VOLUME         0x25
#define CMD_GET_NUM_WAV_FILES       0x30
#define CMD_GET_NUM_WAV_FOLDERS     0x31
#define CMD_GET_NUM_ROMS            0x32
#define CMD_GET_ROM_NAME            0x38
#define CMD_FLASH_ROM               0x39
#define CMD_GET_CURRENT_ROM_NAME    0x3A

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
#define RSP_SYNTH_VOL               140
#define RSP_DAC_VOL                 141
#define RSP_CVSD_VOL                142
#define RSP_NUM_ROMS                145
#define RSP_FILE_NAME               146
#define RSP_ROM_FLASH_RESULT        147
#define RSP_CURRENT_ROM_NAME        148


#define SOM1                        0xf0
#define SOM2                        0xaa
#define EOM                         0x55      

#define RX_BUFFER_SIZE 255
#define TX_BUFFER_SIZE 255
// Global variables for the ring buffer
uint8_t RxBuffer[RX_BUFFER_SIZE];
uint32_t ReadIndex = 0;
// Global buffer dedicated to DMA transmission
uint8_t TxBuffer[TX_BUFFER_SIZE];

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

int16_t HPSCSynthVolL = 0x8000;
int16_t HPSCSynthVolR = 0x8000;
int16_t HPSCDACVolL = 0x8000;
int16_t HPSCDACVolR = 0x8000;
int16_t HPSCCVSDVolL = 0x8000;
int16_t HPSCCVSDVolR = 0x8000;
uint16_t HPSCNumROMs = 0xFFFF;
char HPSCFileName[128];
char HPSCCurrentROMName[128];
uint8_t HPSCFlashResultCode = 0xFF;
char HPSCFlashResultText[11];


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


__attribute__((always_inline)) static inline uint32_t SerialAvailable(void) {
  // Calculate the current write index based on remaining DMA transfers
  uint32_t writeIndex = RX_BUFFER_SIZE - dma_transfer_number_get(DMA0, DMA_CH5);
  
  if (writeIndex >= ReadIndex) {
      return writeIndex - ReadIndex;
  } else {
      // The DMA write pointer has wrapped around the end of the buffer
      return (RX_BUFFER_SIZE - ReadIndex) + writeIndex;
  }
}

__attribute__((always_inline)) static inline uint8_t SerialReadByte(void) {
  uint8_t data = 0;
  
  // Only read if unread bytes exist
  if (SerialAvailable() > 0) {
      data = RxBuffer[ReadIndex];
      ReadIndex++;
      
      // Wrap the read index if it hits the end of the array
      if (ReadIndex >= RX_BUFFER_SIZE) {
          ReadIndex = 0;
      }
  }
  return data;
}



__attribute__((always_inline)) static inline void SerialWriteBuf(uint8_t *buf, uint8_t numbytes) {
  if (numbytes == 0 || numbytes > TX_BUFFER_SIZE) return;

    // Wait for any previous DMA transmission to finish safely
    while (dma_transfer_number_get(DMA0, DMA_CH6) > 0) {
        // Hardware is busy sending the previous block
    }

    // Disable channel to configure the new transfer
    dma_channel_disable(DMA0, DMA_CH6);
    
    // Clear the full transfer finish flag
    dma_flag_clear(DMA0, DMA_CH6, DMA_FLAG_FTF);

    // Copy data to the dedicated DMA memory buffer
    for (uint8_t count = 0; count < numbytes; count++) {
        TxBuffer[count] = buf[count];
    }

    // Set the number of bytes and enable the DMA channel to start transmission
    dma_transfer_number_config(DMA0, DMA_CH6, numbytes);
    dma_channel_enable(DMA0, DMA_CH6);
}

__attribute__((always_inline)) static inline void SerialWriteByte(uint8_t data) {
  SerialWriteBuf(&data, 1);
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


void InitUsartDmaReceive() {
  // Enable DMA0 clock since USART1 is on APB1
  rcu_periph_clock_enable(RCU_DMA0);

  // Deinitialize DMA0 Channel5
  dma_deinit(DMA0, DMA_CH5);

  dma_single_data_parameter_struct dmaInitStruct;
  
  // Configure the DMA transfer parameters
  dmaInitStruct.periph_addr = (uint32_t)&USART_DATA(USART1);
  dmaInitStruct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
  dmaInitStruct.memory0_addr = (uint32_t)RxBuffer;
  dmaInitStruct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
  dmaInitStruct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
  dmaInitStruct.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
  dmaInitStruct.direction = DMA_PERIPH_TO_MEMORY;
  dmaInitStruct.number = RX_BUFFER_SIZE;
  dmaInitStruct.priority = DMA_PRIORITY_HIGH;
  
  // Apply configuration and map the USART1_RX subperipheral
  dma_single_data_mode_init(DMA0, DMA_CH5, &dmaInitStruct);
  dma_channel_subperipheral_select(DMA0, DMA_CH5, DMA_SUBPERI4);
  
  // Enable the DMA channel
  dma_channel_enable(DMA0, DMA_CH5);

  // Tell USART1 to issue DMA requests when bytes arrive using the correct macro
  usart_dma_receive_config(USART1, USART_DENR_ENABLE);
}



void InitUsartDmaTransmit() {
    // Enable DMA0 clock since USART1 is on APB1
    rcu_periph_clock_enable(RCU_DMA0);

    // Deinitialize DMA0 Channel6 (mapped to USART1 TX)
    dma_deinit(DMA0, DMA_CH6);

    dma_single_data_parameter_struct dmaInitStruct;
    
    // Configure the DMA transfer parameters for TX
    dmaInitStruct.periph_addr = (uint32_t)&USART_DATA(USART1);
    dmaInitStruct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dmaInitStruct.memory0_addr = (uint32_t)TxBuffer;
    dmaInitStruct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dmaInitStruct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dmaInitStruct.circular_mode = DMA_CIRCULAR_MODE_DISABLE; // Normal mode for TX
    dmaInitStruct.direction = DMA_MEMORY_TO_PERIPH;
    dmaInitStruct.number = 0; 
    dmaInitStruct.priority = DMA_PRIORITY_HIGH;
    
    // Apply configuration and map the USART1_TX subperipheral
    dma_single_data_mode_init(DMA0, DMA_CH6, &dmaInitStruct);
    dma_channel_subperipheral_select(DMA0, DMA_CH6, DMA_SUBPERI4);
    
    // Tell USART1 to issue DMA requests for transmission
    usart_dma_transmit_config(USART1, USART_DENT_ENABLE);
}



bool HPSoundCardInitConnection() {
  rcu_periph_clock_enable(RCU_GPIOA);
  rcu_periph_clock_enable(RCU_USART1);

  // Configure TX as AF, Push-Pull
  gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_2);
  gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

  // Configure RX as AF, Push-Pull
  gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_3);
  gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_3);
  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);

  usart_deinit(USART1);
  usart_baudrate_set(USART1, 57600U);
  usart_word_length_set(USART1, USART_WL_8BIT);
  usart_stop_bit_set(USART1, USART_STB_1BIT);
  usart_parity_config(USART1, USART_PM_NONE);
  
  // Disable Hardware Flow Control
  usart_hardware_flow_cts_config(USART1, USART_CTS_DISABLE);
  usart_hardware_flow_rts_config(USART1, USART_RTS_DISABLE);
  usart_receive_config(USART1, USART_RECEIVE_ENABLE);
  usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);

  // Initialize the DMA for USART1 RX/TX before enabling the USART
  InitUsartDmaReceive();
  InitUsartDmaTransmit();

  // Clear any lingering overrun errors or junk data before starting
  usart_flag_clear(USART1, USART_FLAG_ORERR);
  usart_data_receive(USART1); 

  // Enable USART1
  usart_enable(USART1);
  
  HPSCVerionRcvd = false;
  HPSCVerion[0] = '\0';
  HPSCSysinfoRcvd = false;

  byte volumeIndex = RTC_BKP9 & HPSC_PRIMARY_VOLUME_MASK;
  if (volumeIndex==0) volumeIndex = 10;
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

  // Flush and request version at the very end so DMA is ready to catch the reply
  HPSoundCardFlush();
  HPSoundCardStopAllTracks();
  HPSoundCardSetReporting(true);
  HPSoundCardMasterGain(HPSCGetGainFromLevel(volumeIndex));
  HPSoundCardTrackPlayPoly(999); // startup bong
  HPSoundCardRequestVersion();

  return true;
}


bool HPBasicWAVSoundCardPresent() {
  bool basicSoundCard = false;  
  if (HPSCVerionRcvd) {
      // A standard Robertsonics board version string begins with "WAV Trigger"
      // By checking the first 11 characters, we ignore the specific version number (e.g., " v1.34")
      if (strncmp(HPSCVerion, "WAV Trigger", 11) == 0) {
          basicSoundCard = true;
      }
  }
  return basicSoundCard;
}


bool HPAdvancedWAVSoundCardPresent() {
  bool advSoundCard = false;  
  if (HPSCVerionRcvd) {
      if (strncmp(HPSCVerion, "WPCS Audio", 10) == 0) {
        advSoundCard = true;
      }
  }
  return advSoundCard;
}


char *HPSoundCardVersion() {
  return HPSCVerion;
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
          break;

        case RSP_VERSION_STRING:
          for (i = 0; i < (VERSION_STRING_LEN - 1); i++)
            HPSCVerion[i] = HPSCrxMessage[i + 1];
          HPSCVerion[VERSION_STRING_LEN - 1] = 0;
          HPSCVerionRcvd = true;
          break;

        case RSP_SYSTEM_INFO:
          HPSCNumVoices = HPSCrxMessage[1];
          HPSCNumTracks = HPSCrxMessage[3];
          HPSCNumTracks = (HPSCNumTracks * 256) + HPSCrxMessage[2];
          HPSCSysinfoRcvd = true;
          break;

        case RSP_SYNTH_VOL:
          HPSCSynthVolL = HPSCrxMessage[1] | (HPSCrxMessage[2])<<8;
          HPSCSynthVolR = HPSCrxMessage[3] | (HPSCrxMessage[4])<<8;
          break;

        case RSP_DAC_VOL:
          HPSCDACVolL = HPSCrxMessage[1] | (HPSCrxMessage[2])<<8;
          HPSCDACVolR = HPSCrxMessage[3] | (HPSCrxMessage[4])<<8;
          break;

        case RSP_CVSD_VOL:
          HPSCCVSDVolL = HPSCrxMessage[1] | (HPSCrxMessage[2])<<8;
          HPSCCVSDVolR = HPSCrxMessage[3] | (HPSCrxMessage[4])<<8;
          break;

        case RSP_NUM_ROMS:
          HPSCNumROMs = HPSCrxMessage[1] | (HPSCrxMessage[2])<<8;
          break;

        case RSP_FILE_NAME:
          for (uint8_t count=0; count<128; count++) {
            HPSCFileName[count] = HPSCrxMessage[1+count];
          }
          break;

        case RSP_ROM_FLASH_RESULT:
          HPSCFlashResultCode = HPSCrxMessage[1];
          for (uint8_t count=0; count<10; count++) {
            HPSCFlashResultText[count] = HPSCrxMessage[2+count];
          }
          HPSCFlashResultText[10] = '\0';
          break;

        case RSP_CURRENT_ROM_NAME:
          for (uint8_t count=0; count<128; count++) {
            HPSCCurrentROMName[count] = HPSCrxMessage[1+count];
          }
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
  txbuf[3] = CMD_MAIN_VOLUME;
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


void HPSoundCardRequestVersion() {
  uint8_t txbuf[5];
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



void HPSoundCardRequestSynthVol() {
  HPSCSynthVolL = 0x8000;
  HPSCSynthVolR = 0x8000;
  uint8_t txbuf[5];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x05;
  txbuf[3] = CMD_GET_SYNTH_VOLUME;
  txbuf[4] = EOM;
  SerialWriteBuf(txbuf, 5);
}

void HPSoundCardRequestDACVol() {
  HPSCDACVolL = 0x8000;
  HPSCDACVolR = 0x8000;
  uint8_t txbuf[5];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x05;
  txbuf[3] = CMD_GET_DAC_VOLUME;
  txbuf[4] = EOM;
  SerialWriteBuf(txbuf, 5);
}

void HPSoundCardRequestCVSDVol() {
  HPSCCVSDVolL = 0x8000;
  HPSCCVSDVolR = 0x8000;
  uint8_t txbuf[5];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x05;
  txbuf[3] = CMD_GET_CVSD_VOLUME;
  txbuf[4] = EOM;
  SerialWriteBuf(txbuf, 5);
}


void HPSoundCardSetSynthVol(int16_t leftVol, int16_t rightVol) {
  HPSCSynthVolL = leftVol;
  HPSCSynthVolR = rightVol;

  uint8_t txbuf[9];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x09;
  txbuf[3] = CMD_SET_SYNTH_VOLUME;
  txbuf[4] = HPSCSynthVolL & 0xFF;
  txbuf[5] = HPSCSynthVolL >> 8;
  txbuf[6] = HPSCSynthVolR & 0xFF;
  txbuf[7] = HPSCSynthVolR >> 8;
  txbuf[8] = EOM;
  SerialWriteBuf(txbuf, 9);
}

void HPSoundCardSetDACVol(int16_t leftVol, int16_t rightVol) {
  HPSCDACVolL = leftVol;
  HPSCDACVolR = rightVol;

  uint8_t txbuf[9];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x09;
  txbuf[3] = CMD_SET_DAC_VOLUME;
  txbuf[4] = HPSCDACVolL & 0xFF;
  txbuf[5] = HPSCDACVolL >> 8;
  txbuf[6] = HPSCDACVolR & 0xFF;
  txbuf[7] = HPSCDACVolR >> 8;
  txbuf[8] = EOM;
  SerialWriteBuf(txbuf, 9);  
}

void HPSoundCardSetCVSDVol(int16_t leftVol, int16_t rightVol) {
  HPSCCVSDVolL = leftVol;
  HPSCCVSDVolR = rightVol;
  
  uint8_t txbuf[9];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x09;
  txbuf[3] = CMD_SET_CVSD_VOLUME;
  txbuf[4] = HPSCCVSDVolL & 0xFF;
  txbuf[5] = HPSCCVSDVolL >> 8;
  txbuf[6] = HPSCCVSDVolR & 0xFF;
  txbuf[7] = HPSCCVSDVolR >> 8;
  txbuf[8] = EOM;
  SerialWriteBuf(txbuf, 9);
}

bool HPSoundCardCurSynthVol(int16_t *curLeft, int16_t *curRight) {
  if (HPSCSynthVolL==(int16_t)0x8000 || HPSCSynthVolR==(int16_t)0x8000) return false;
  *curLeft = HPSCSynthVolL;
  *curRight = HPSCSynthVolR;
  return true;
}

bool HPSoundCardCurDACVol(int16_t *curLeft, int16_t *curRight) {
  if (HPSCDACVolL==(int16_t)0x8000 || HPSCDACVolR==(int16_t)0x8000) return false;
  *curLeft = HPSCDACVolL;
  *curRight = HPSCDACVolR;
  return true;
}

bool HPSoundCardCurCVSDVol(int16_t *curLeft, int16_t *curRight) {
  if (HPSCCVSDVolL==(int16_t)0x8000 || HPSCCVSDVolR==(int16_t)0x8000) return false;
  *curLeft = HPSCCVSDVolL;
  *curRight = HPSCCVSDVolR;
  return true;
}


void HPSoundCardRequestNumROMs() {
  HPSCNumROMs = 0xFFFF;
  uint8_t txbuf[5];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x05;
  txbuf[3] = CMD_GET_NUM_ROMS;
  txbuf[4] = EOM;
  SerialWriteBuf(txbuf, 5);
}

bool HPSoundCardCurNumROMs(uint16_t *numRoms) {
  if (HPSCNumROMs==0xFFFF) return false;
  *numRoms = HPSCNumROMs;
  return true;
}



void HPSoundCardRequestROMName(uint16_t romNum) {
  HPSCFileName[0] = 0x00;

  uint8_t txbuf[7];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x07;
  txbuf[3] = CMD_GET_ROM_NAME;
  txbuf[4] = romNum & 0xFF;
  txbuf[5] = romNum >> 8;
  txbuf[6] = EOM;
  SerialWriteBuf(txbuf, 7);
}


char *HPSoundCardCurROMName() {
  if (HPSCFileName[0]==0x00) return NULL;
  return HPSCFileName;
}


void HPSoundCardFlashROM(uint16_t romNum) {
  HPSCFlashResultCode = 0xFF;
  HPSCFlashResultText[0] = 0x00;

  uint8_t txbuf[7];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x07;
  txbuf[3] = CMD_FLASH_ROM;
  txbuf[4] = romNum & 0xFF;
  txbuf[5] = romNum >> 8;
  txbuf[6] = EOM;
  SerialWriteBuf(txbuf, 7);  
}

bool HPSoundCardCurFlashCode(uint8_t *retCode) {
  if (HPSCFlashResultCode==0xFF) return false;
  *retCode = HPSCFlashResultCode;
  return true;
}

char *HPSoundCardCurFlashMessage() {
  if (HPSCFlashResultText[0]==0x00) return NULL;
  return HPSCFlashResultText;
}


void HPSoundCardRequestCurrentROMName(void) {
  HPSCCurrentROMName[0] = 0x00; // Invalidate the old string

  uint8_t txbuf[5];
  txbuf[0] = SOM1;
  txbuf[1] = SOM2;
  txbuf[2] = 0x05;
  txbuf[3] = CMD_GET_CURRENT_ROM_NAME;
  txbuf[4] = EOM;
  SerialWriteBuf(txbuf, 5);
}

char *HPSoundCardGetActiveROMName(void) {
  if (HPSCCurrentROMName[0] == 0x00) return NULL; // Indicates waiting for response
  return HPSCCurrentROMName;
}