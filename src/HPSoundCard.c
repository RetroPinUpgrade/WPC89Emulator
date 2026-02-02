#include "HPSoundCard.h"
#include "gd32f4xx.h"

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

uint16_t HPSCVoiceTable[MAX_NUM_VOICES];
uint8_t HPSCrxMessage[MAX_MESSAGE_LEN];
char HPSCVerion[VERSION_STRING_LEN];
uint16_t HPSCNumTracks;
uint8_t HPSCNumVoices;
uint8_t HPSCrxCount;
uint8_t HPSCrxLen;
bool HPSCrxMsgReady;
bool HPSCVerionRcvd;
bool HPSCSysinfoRcvd;

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