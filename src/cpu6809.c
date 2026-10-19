#include "stdio.h"
#include "cpu6809.h"
#include "mpu89.h"


bool WPCSCPUWriteToLog = false;


static uint16_t WPCSCPUTickCount;
static uint8_t WPCSCPUirqPending = false;
static uint8_t WPCSCPUfirqPending = false;
static uint8_t WPCSCPUmissedIRQ = 0;
static uint8_t WPCSCPUmissedFIRQ = 0;

static uint8_t WPCSCPUirqCount = 0;
static uint8_t WPCSCPUfirqCount = 0;
static uint8_t WPCSCPUnmiCount = 0;

static uint8_t WPCSCPUregA = 0;
static uint8_t WPCSCPUregB = 0;
static uint16_t WPCSCPUregX = 0;
static uint16_t WPCSCPUregY = 0;
static uint16_t WPCSCPUregU = 0;
static uint16_t WPCSCPUregS = 0;
static uint8_t WPCSCPUregCC = 0;
static uint16_t WPCSCPUregPC = 0;
static uint8_t WPCSCPUregDP = 0;

static uint8_t WPCSCPU_F_CARRY = 1;
static uint8_t WPCSCPU_F_OVERFLOW = 2;
static uint8_t WPCSCPU_F_ZERO = 4;
static uint8_t WPCSCPU_F_NEGATIVE = 8;
static uint8_t WPCSCPU_F_IRQMASK = 16;
static uint8_t WPCSCPU_F_HALFCARRY = 32;
static uint8_t WPCSCPU_F_FIRQMASK = 64;
static uint8_t WPCSCPU_F_ENTIRE = 128;

static uint16_t WPCSCPUvecRESET = 0xFFFE;
static uint16_t WPCSCPUvecFIRQ = 0xFFF6;
static uint16_t WPCSCPUvecIRQ = 0xFFF8;
static uint16_t WPCSCPUvecNMI = 0xFFFC;
static uint16_t WPCSCPUvecSWI = 0xFFFA;
static uint16_t WPCSCPUvecSWI2 = 0xFFF4;
static uint16_t WPCSCPUvecSWI3 = 0xFFF2;

static uint8_t WPCSCPUCycles[] = {
  6, 0, 0, 6, 6, 0, 6, 6, 6, 6, 6, 0, 6, 6, 3, 6, /* 00-0F */
  1, 1, 2, 2, 0, 0, 5, 9, 0, 2, 3, 0, 3, 2, 8, 7, /* 10-1F */
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 20-2F */
  4, 4, 4, 4, 5, 5, 5, 5, 0, 5, 3, 6, 21,11,0, 19,/* 30-3F */
  2, 0, 0, 2, 2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2, /* 40-4F */
  2, 0, 0, 2, 2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2, /* 50-5F */
  6, 0, 0, 6, 6, 0, 6, 6, 6, 6, 6, 0, 6, 6, 3, 6, /* 60-6F */
  7, 0, 0, 7, 7, 0, 7, 7, 7, 7, 7, 0, 7, 7, 4, 7, /* 70-7F */
  2, 2, 2, 4, 2, 2, 2, 0, 2, 2, 2, 2, 4, 7, 3, 0, /* 80-8F */
  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 6, 7, 5, 5, /* 90-9F */
  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 6, 7, 5, 5, /* A0-AF */
  5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 5, 7, 8, 6, 6, /* B0-BF */
  2, 2, 2, 4, 2, 2, 2, 0, 2, 2, 2, 2, 3, 0, 3, 0, /* C0-CF */
  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, /* D0-DF */
  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, /* E0-EF */
  5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6  /* F0-FF */
};

static uint8_t WPCSCPUCycles2[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00-0F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10-1F */
  0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, /* 20-2F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, /* 30-3F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 40-4F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 50-5F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 60-6F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 70-7F */
  0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 4, 0, /* 80-8F */
  0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 6, 6, /* 90-9F */
  0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 6, 6, /* A0-AF */
  0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 7, 7, /* B0-BF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, /* C0-CF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, /* D0-DF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, /* E0-EF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7  /* F0-FF */
};

static uint8_t flagsNZ[] = {
  4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00-0F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10-1F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 20-2F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 30-3F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 40-4F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 50-5F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 60-6F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 70-7F */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* 80-8F */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* 90-9F */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* A0-AF */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* B0-BF */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* C0-CF */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* D0-DF */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* E0-EF */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8  /* F0-FF */
};


void WPCSCPU_executeNmi();
void WPCSCPU_executeFirq();
void WPCSCPU_executeIrq();

static void setV8(uint16_t a, uint16_t b, uint16_t r);
static void setV16(uint32_t a, uint32_t b, uint32_t r);
static uint16_t getD();
static void setD(uint16_t v);
static void PUSHB(uint8_t b);
static void PUSHW(uint16_t b);
static void PUSHBU(uint8_t b);
static void PUSHWU(uint16_t b);
static uint8_t PULLB();
static uint16_t PULLW();
static uint8_t PULLBU();
static uint16_t PULLWU();
static void PSHS(uint8_t ucTemp);
static void PSHU(uint8_t ucTemp);
static void PULS(uint8_t ucTemp);
static void PULU(uint8_t ucTemp);
static uint16_t getPostByteRegister(uint8_t ucPostByte);
static void setPostByteRegister(uint8_t ucPostByte, uint16_t v);
static void TFREXG(uint8_t ucPostByte, uint8_t bExchange);
static int16_t signed5bit(uint8_t x);
static int16_t signed8(uint16_t x);
static int16_t signed16(uint16_t x);
static uint8_t fetch();
static uint16_t fetch16();
static uint16_t ReadWord(uint16_t addr);
static void WriteWord(uint16_t addr, uint16_t v);
static uint16_t PostByte();
static void flagsNZ16(uint16_t inputWord);
static uint8_t oINC(uint8_t b);
static uint8_t oDEC(uint8_t b);
static uint8_t oSUB(uint8_t b, uint8_t v);
static uint16_t oSUB16(uint16_t b, uint16_t v);
static uint8_t oADD(uint8_t b, uint8_t v);
static uint16_t oADD16(uint16_t b, uint16_t v);
static uint8_t oADC(uint8_t b, uint8_t v);
static uint8_t oSBC(uint8_t b, uint8_t v);
static uint8_t oCMP(uint8_t b, uint8_t v);
static uint16_t oCMP16(uint16_t b, uint16_t v);
static uint8_t oNEG(uint8_t b);
static uint8_t oLSR(uint8_t b);
static uint8_t oASR(uint8_t b);
static uint8_t oASL(uint8_t b);
static uint8_t oROL(uint8_t b);
static uint8_t oROR(uint8_t b);
static uint8_t oEOR(uint8_t b, uint8_t v);
static uint8_t oOR(uint8_t b, uint8_t v);
static uint8_t oAND(uint8_t b, uint8_t v);
static uint8_t oCOM(uint8_t b);
static uint16_t dpadd();

// set overflow flag
static void setV8(uint16_t a, uint16_t b, uint16_t r) {
  WPCSCPUregCC |= ((a ^ b ^ r ^ (r >> 1)) & 0x80) >> 6;
}

// set overflow flag
static void setV16(uint32_t a, uint32_t b, uint32_t r) {
  WPCSCPUregCC |= ((a ^ b ^ r ^ (r >> 1)) & 0x8000) >> 14;
}

uint16_t getD() {
  return ((uint16_t)WPCSCPUregA << 8) + WPCSCPUregB;
}

static void setD(uint16_t v) {
  WPCSCPUregA = (v >> 8) & 0xff;
  WPCSCPUregB = v & 0xff;
}

static void PUSHB(uint8_t b) {
  WPCSCPUregS = (WPCSCPUregS - 1) & 0xFFFF;
  MPUWrite8(WPCSCPUregS, b & 0xFF);
}

static void PUSHW(uint16_t b) {
  WPCSCPUregS = (WPCSCPUregS - 1) & 0xFFFF;
  MPUWrite8(WPCSCPUregS, b & 0xFF);
  WPCSCPUregS = (WPCSCPUregS - 1) & 0xFFFF;
  MPUWrite8(WPCSCPUregS, (b >> 8) & 0xFF);
}

static void PUSHBU(uint8_t b) {
  WPCSCPUregU = (WPCSCPUregU - 1) & 0xFFFF;
  MPUWrite8(WPCSCPUregU, b & 0xFF);
}

static void PUSHWU(uint16_t b) {
  WPCSCPUregU = (WPCSCPUregU - 1) & 0xFFFF;
  MPUWrite8(WPCSCPUregU, b & 0xFF);
  WPCSCPUregU = (WPCSCPUregU - 1) & 0xFFFF;
  MPUWrite8(WPCSCPUregU, (b >> 8) & 0xFF);
}

static uint8_t PULLB() {
  uint8_t tempuint8_t = MPURead8(WPCSCPUregS);
  WPCSCPUregS += 1;
  return tempuint8_t;
}

static uint16_t PULLW() {
  uint16_t tempW = (MPURead8(WPCSCPUregS)<<8) + MPURead8(WPCSCPUregS+1);
  WPCSCPUregS += 2;
  return tempW;
}

static uint8_t PULLBU() {
  uint8_t tempuint8_t = MPURead8(WPCSCPUregU);
  WPCSCPUregU += 1;
  return tempuint8_t;
}

static uint16_t PULLWU() {
  uint16_t tempW = (MPURead8(WPCSCPUregU)<<8) + MPURead8(WPCSCPUregU+1);
  WPCSCPUregU += 2;
  return tempW;
}

//Push A, B, CC, DP, D, X, Y, U, or PC onto hardware stack
static void PSHS(uint8_t ucTemp) {
  uint8_t i = 0;
  if (ucTemp & 0x80) {
    PUSHW(WPCSCPUregPC);
    i += 2;
  }
  if (ucTemp & 0x40) {
    PUSHW(WPCSCPUregU);
    i += 2;
  }
  if (ucTemp & 0x20) {
    PUSHW(WPCSCPUregY);
    i += 2;
  }
  if (ucTemp & 0x10) {
    PUSHW(WPCSCPUregX);
    i += 2;
  }
  if (ucTemp & 0x8) {
    PUSHB(WPCSCPUregDP);
    i++;
  }
  if (ucTemp & 0x4) {
    PUSHB(WPCSCPUregB);
    i++;
  }
  if (ucTemp & 0x2) {
    PUSHB(WPCSCPUregA);
    i++;
  }
  if (ucTemp & 0x1) {
    PUSHB(WPCSCPUregCC);
    i++;
  }
  WPCSCPUTickCount += i; //timing
}

//Push A, B, CC, DP, D, X, Y, S, or PC onto user stack
static void PSHU(uint8_t ucTemp) {
  uint8_t i = 0;
  if (ucTemp & 0x80) {
    PUSHWU(WPCSCPUregPC);
    i += 2;
  }
  if (ucTemp & 0x40) {
    PUSHWU(WPCSCPUregS);
    i += 2;
  }
  if (ucTemp & 0x20) {
    PUSHWU(WPCSCPUregY);
    i += 2;
  }
  if (ucTemp & 0x10) {
    PUSHWU(WPCSCPUregX);
    i += 2;
  }
  if (ucTemp & 0x8) {
    PUSHBU(WPCSCPUregDP);
    i++;
  }
  if (ucTemp & 0x4) {
    PUSHBU(WPCSCPUregB);
    i++;
  }
  if (ucTemp & 0x2) {
    PUSHBU(WPCSCPUregA);
    i++;
  }
  if (ucTemp & 0x1) {
    PUSHBU(WPCSCPUregCC);
    i++;
  }
  WPCSCPUTickCount += i; //timing
}

//Pull A, B, CC, DP, D, X, Y, U, or PC from hardware stack
static void PULS(uint8_t ucTemp) {
  uint8_t i = 0;
  if (ucTemp & 0x1) {
    WPCSCPUregCC = PULLB();
    i++;
  }
  if (ucTemp & 0x2) {
    WPCSCPUregA = PULLB();
    i++;
  }
  if (ucTemp & 0x4) {
    WPCSCPUregB = PULLB();
    i++;
  }
  if (ucTemp & 0x8) {
    WPCSCPUregDP = PULLB();
    i++;
  }
  if (ucTemp & 0x10) {
    WPCSCPUregX = PULLW();
    i += 2;
  }
  if (ucTemp & 0x20) {
    WPCSCPUregY = PULLW();
    i += 2;
  }
  if (ucTemp & 0x40) {
    WPCSCPUregU = PULLW();
    i += 2;
  }
  if (ucTemp & 0x80) {
    WPCSCPUregPC = PULLW();
    i += 2;
  }
  WPCSCPUTickCount += i; //timing
}

//Pull A, B, CC, DP, D, X, Y, S, or PC from hardware stack
static void PULU(uint8_t ucTemp) {
  uint8_t i = 0;
  if (ucTemp & 0x1) {
    WPCSCPUregCC = PULLBU();
    i++;
  }
  if (ucTemp & 0x2) {
    WPCSCPUregA = PULLBU();
    i++;
  }
  if (ucTemp & 0x4) {
    WPCSCPUregB = PULLBU();
    i++;
  }
  if (ucTemp & 0x8) {
    WPCSCPUregDP = PULLBU();
    i++;
  }
  if (ucTemp & 0x10) {
    WPCSCPUregX = PULLWU();
    i += 2;
  }
  if (ucTemp & 0x20) {
    WPCSCPUregY = PULLWU();
    i += 2;
  }
  if (ucTemp & 0x40) {
    WPCSCPUregS = PULLWU();
    i += 2;
  }
  if (ucTemp & 0x80) {
    WPCSCPUregPC = PULLWU();
    i += 2;
  }
  WPCSCPUTickCount += i; //timing
}


void WPCSCPU_executeNmi() {
  PUSHW(WPCSCPUregPC);
  PUSHW(WPCSCPUregU);
  PUSHW(WPCSCPUregY);
  PUSHW(WPCSCPUregX);
  PUSHB(WPCSCPUregDP);
  PUSHB(WPCSCPUregB);
  PUSHB(WPCSCPUregA);
  WPCSCPUregCC |= WPCSCPU_F_ENTIRE;
  PUSHB(WPCSCPUregCC);
  WPCSCPUregCC |= WPCSCPU_F_IRQMASK | WPCSCPU_F_FIRQMASK;
  WPCSCPUregPC = ReadWord(WPCSCPUvecNMI);
  WPCSCPUTickCount += 19;
}

void WPCSCPU_executeFirq() {
  WPCSCPUregCC &= ~WPCSCPU_F_ENTIRE;
  PUSHW(WPCSCPUregPC);
  PUSHB(WPCSCPUregCC);

  WPCSCPUregCC |= WPCSCPU_F_IRQMASK | WPCSCPU_F_FIRQMASK;
  WPCSCPUregPC = ReadWord(WPCSCPUvecFIRQ);
  WPCSCPUTickCount += 10;
}

void WPCSCPU_executeIrq() {
  WPCSCPUregCC |= WPCSCPU_F_ENTIRE;
  PUSHW(WPCSCPUregPC);
  PUSHW(WPCSCPUregU);
  PUSHW(WPCSCPUregY);
  PUSHW(WPCSCPUregX);
  PUSHB(WPCSCPUregDP);
  PUSHB(WPCSCPUregB);
  PUSHB(WPCSCPUregA);
  PUSHB(WPCSCPUregCC);

  WPCSCPUregCC |= WPCSCPU_F_IRQMASK;
  WPCSCPUregPC = ReadWord(WPCSCPUvecIRQ);
  WPCSCPUTickCount += 19;
}

uint16_t getPostByteRegister(uint8_t ucPostByte) {
  switch (ucPostByte & 0xF) {
    case 0x00:
      return getD();
    case 0x1:
      return WPCSCPUregX;
    case 0x2:
      return WPCSCPUregY;
    case 0x3:
      return WPCSCPUregU;
    case 0x4:
      return WPCSCPUregS;
    case 0x5:
      return WPCSCPUregPC;
    case 0x8:
      return WPCSCPUregA;
    case 0x9:
      return WPCSCPUregB;
    case 0xA:
      return WPCSCPUregCC;
    case 0xB:
      return WPCSCPUregDP;
    default:
      /* illegal */
      //throw new Error('getPBR_INVALID_' + ucPostByte);
      break;
  }
  return 0;
}

static void setPostByteRegister(uint8_t ucPostByte, uint16_t v) {
  /* Get destination WPCSCPUregister */
  switch (ucPostByte & 0xF) {
    case 0x00:
      setD(v);
      return;
    case 0x1:
      WPCSCPUregX = v;
      return;
    case 0x2:
      WPCSCPUregY = v;
      return;
    case 0x3:
      WPCSCPUregU = v;
      return;
    case 0x4:
      WPCSCPUregS = v;
      return;
    case 0x5:
      WPCSCPUregPC = v;
      return;
    case 0x8:
      WPCSCPUregA = v & 0xFF;
      return;
    case 0x9:
      WPCSCPUregB = v & 0xFF;
      return;
    case 0xA:
      WPCSCPUregCC = v & 0xFF;
      return;
    case 0xB:
      WPCSCPUregDP = v & 0xFF;
      return;
    default:
      /* illegal */
      //throw new Error('setPBR_INVALID_' + ucPostByte);
      break;
  }
}

// Transfer or exchange two WPCSCPUregisters.
static void TFREXG(uint8_t ucPostByte, uint8_t bExchange) {
  uint16_t ucTemp = ucPostByte & 0x88;
  if (ucTemp == 0x80 || ucTemp == 0x08) {
    //throw new Error('TFREXG_ERROR_MIXING_8_AND_16BIT_REGISTER!');
  }

  ucTemp = getPostByteRegister(ucPostByte >> 4);
  if (bExchange) {
    setPostByteRegister(ucPostByte >> 4, getPostByteRegister(ucPostByte));
  }
  /* Transfer */
  setPostByteRegister(ucPostByte, ucTemp);
}

static int16_t signed5bit(uint8_t x) {
  return x > 0xF ? x - 0x20 : x;
}

static int16_t signed8(uint16_t x) {
  return x > 0x7F ? x - 0x100 : x;
}

static int16_t signed16(uint16_t x) {
  return x > 0x7FFF ? x - 0x10000 : x;
}

static uint8_t fetch() {
  uint8_t tempuint8_t = MPURead8(WPCSCPUregPC);
  WPCSCPUregPC += 1;
  return tempuint8_t;
}

static uint16_t fetch16() {
  uint16_t v1 = MPURead8(WPCSCPUregPC);
  uint16_t v2 = MPURead8(WPCSCPUregPC+1);
  WPCSCPUregPC += 2;
  return (v1 << 8) + v2;
}

static uint16_t ReadWord(uint16_t addr) {
  uint16_t v1 = MPURead8(addr);
  uint16_t v2 = MPURead8((addr + 1) & 0xFFFF);
  return (v1 << 8) + v2;
}

static void WriteWord(uint16_t addr, uint16_t v) {
  MPUWrite8(addr, (v >> 8) & 0xff);
  MPUWrite8((addr + 1) & 0xFFFF, v & 0xff);
}

//PURPOSE: Calculate the EA for INDEXING addressing mode.
//
// Offset sizes for postByte
// ±4-bit (-16 to +15)
// ±7-bit (-128 to +127)
// ±15-bit (-32768 to +32767)
static uint16_t PostByte() {
  uint8_t INDIRECT_FIELD = 0x10;
  uint8_t REGISTER_FIELD = 0x60;
  uint8_t COMPLEXTYPE_FIELD = 0x80;
  uint8_t ADDRESSINGMODE_FIELD = 0x0F;

  uint8_t postByte = fetch();
  uint16_t WPCSCPUregisterField = 0;
  // Isolate WPCSCPUregister is used for the indexed operation
  // see Table 3-6. Indexed Addressing PostByte Register
  switch (postByte & REGISTER_FIELD) {
    case 0x00:
      WPCSCPUregisterField = WPCSCPUregX;
      break;
    case 0x20:
      WPCSCPUregisterField = WPCSCPUregY;
      break;
    case 0x40:
      WPCSCPUregisterField = WPCSCPUregU;
      break;
    case 0x60:
      WPCSCPUregisterField = WPCSCPUregS;
      break;
    default:
      //throw new Error('INVALID_ADDRESS_PB');
      break;
  }

  uint16_t xchg = 0;
  uint16_t EA = 0; //Effective Address
  if (postByte & COMPLEXTYPE_FIELD) {
    // Complex stuff
    switch (postByte & ADDRESSINGMODE_FIELD) {
      case 0x00: // R+
        EA = WPCSCPUregisterField;
        xchg = WPCSCPUregisterField + 1;
        WPCSCPUTickCount += 2;
        break;
      case 0x01: // R++
        EA = WPCSCPUregisterField;
        xchg = WPCSCPUregisterField + 2;
        WPCSCPUTickCount += 3;
        break;
      case 0x02: // -R
        xchg = WPCSCPUregisterField - 1;
        EA = xchg;
        WPCSCPUTickCount += 2;
        break;
      case 0x03: // --R
        xchg = WPCSCPUregisterField - 2;
        EA = xchg;
        WPCSCPUTickCount += 3;
        break;
      case 0x04: // EA = R + 0 OFFSET
        EA = WPCSCPUregisterField;
        break;
      case 0x05: // EA = R + REGB OFFSET
        EA = WPCSCPUregisterField + signed8(WPCSCPUregB);
        WPCSCPUTickCount += 1;
        break;
      case 0x06: // EA = R + REGA OFFSET
        EA = WPCSCPUregisterField + signed8(WPCSCPUregA);
        WPCSCPUTickCount += 1;
        break;
      // case 0x07 is ILLEGAL
      case 0x08: // EA = R + 7bit OFFSET
        EA = WPCSCPUregisterField + signed8(fetch());
        WPCSCPUTickCount += 1;
        break;
      case 0x09: // EA = R + 15bit OFFSET
        EA = WPCSCPUregisterField + signed16(fetch16());
        WPCSCPUTickCount += 4;
        break;
      // case 0x0A is ILLEGAL
      case 0x0B: // EA = R + D OFFSET
        EA = WPCSCPUregisterField + getD();
        WPCSCPUTickCount += 4;
        break;
      case 0x0C: { // EA = PC + 7bit OFFSET
        // NOTE: fetch increases WPCSCPUregPC - so order is important!
        int16_t tempOffset = signed8(fetch());
        EA = WPCSCPUregPC + tempOffset;
        WPCSCPUTickCount += 1;
        break;
      }
      case 0x0D: { // EA = PC + 15bit OFFSET
        // NOTE: fetch increases WPCSCPUregPC - so order is important!
        int16_t word = signed16(fetch16());
        EA = WPCSCPUregPC + word;
        WPCSCPUTickCount += 5;
        break;
      }
      // case 0xE is ILLEGAL
      case 0x0F: // EA = ADDRESS
        EA = fetch16();
        WPCSCPUTickCount += 5;
        break;
      default: {
        //uint8_t mode = postByte & ADDRESSINGMODE_FIELD;
        //throw new Error('INVALID_ADDRESS_MODE_' + mode);
        break;
      }
    }

    EA &= 0xFFFF;
    if (postByte & INDIRECT_FIELD) {
      /* TODO: Indirect "Increment/Decrement by 1" is not valid
      const adrmode = postByte & ADDRESSINGMODE_FIELD
      if (adrmode == 0 || adrmode == 2) {
        throw new Error('INVALID_INDIRECT_ADDRESSMODE_', adrmode);
      }
      */
      // INDIRECT addressing
      EA = ReadWord(EA);
      WPCSCPUTickCount += 3;
    }
  } else {
    // Just a 5 bit signed offset + WPCSCPUregister, NO INDIRECT ADDRESS MODE
    int16_t suint8_t = signed5bit(postByte & 0x1F);
    EA = WPCSCPUregisterField + suint8_t;
    WPCSCPUTickCount += 1;
  }

  if (xchg != 0 ) {
    xchg &= 0xFFFF;
    switch (postByte & REGISTER_FIELD) {
      case 0:
        WPCSCPUregX = xchg;
        break;
      case 0x20:
        WPCSCPUregY = xchg;
        break;
      case 0x40:
        WPCSCPUregU = xchg;
        break;
      case 0x60:
        WPCSCPUregS = xchg;
        break;
      default:
        //throw new Error('PB_INVALID_XCHG_VALUE_' + postByte);
        break;
    }
  }
  // Return the effective address
  return EA & 0xFFFF;
}

static void flagsNZ16(uint16_t inputWord) {
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE);
  if (inputWord == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  }
  if (inputWord & 0x8000) {
    WPCSCPUregCC |= WPCSCPU_F_NEGATIVE;
  }
}

// ============= Operations

static uint8_t oINC(uint8_t b) {
  b = (b + 1) & 0xFF;
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  WPCSCPUregCC |= flagsNZ[b];
  //Docs say:
  //V: Set if the original operand was 01111111
  if (b == 0x80) {
    WPCSCPUregCC |= WPCSCPU_F_OVERFLOW;
  }
  return b;
}

static uint8_t oDEC(uint8_t b) {
  b = (b - 1) & 0xFF;
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  WPCSCPUregCC |= flagsNZ[b];
  //Docs say:
  //V: Set if the original operand was 10000000
  if (b == 0x7f) {
    WPCSCPUregCC |= WPCSCPU_F_OVERFLOW;
  }
  return b;
}

static uint8_t oSUB(uint8_t b, uint8_t v) {
  int16_t temp = b - v;
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  if (temp & 0x100) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  setV8(b, v, temp);
  temp &= 0xFF;
  WPCSCPUregCC |= flagsNZ[temp];
  return temp;
}

static uint16_t oSUB16(uint16_t b, uint16_t v) {
  uint32_t temp = b - v;
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  if (temp & 0x8000) {
    WPCSCPUregCC |= WPCSCPU_F_NEGATIVE;
  }
  if (temp & 0x10000) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  setV16(b, v, temp);
  temp &= 0xFFFF;
  if (temp == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  }
  return temp;
}

static uint8_t oADD(uint8_t b, uint8_t v) {
  int16_t temp = b + v;
  WPCSCPUregCC &= ~(WPCSCPU_F_HALFCARRY | WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  if (temp & 0x100) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  setV8(b, v, temp);
  if ((temp ^ b ^ v) & 0x10) {
    WPCSCPUregCC |= WPCSCPU_F_HALFCARRY;
  }
  temp &= 0xFF;
  WPCSCPUregCC |= flagsNZ[temp];
  return temp;
}

static uint16_t oADD16(uint16_t b, uint16_t v) {
  uint32_t temp = b + v;
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  if (temp & 0x8000) {
    WPCSCPUregCC |= WPCSCPU_F_NEGATIVE;
  }
  if (temp & 0x10000) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  setV16(b, v, temp);
  temp &= 0xFFFF;
  if (temp == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  }
  return temp;
}

static uint8_t oADC(uint8_t b, uint8_t v) {
  int16_t temp = b + v + (WPCSCPUregCC & WPCSCPU_F_CARRY);
  WPCSCPUregCC &= ~(WPCSCPU_F_HALFCARRY | WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  if (temp & 0x100) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  setV8(b, v, temp);
  if ((temp ^ b ^ v) & 0x10) {
    WPCSCPUregCC |= WPCSCPU_F_HALFCARRY;
  }
  temp &= 0xFF;
  WPCSCPUregCC |= flagsNZ[temp];
  return temp;
}

static uint8_t oSBC(uint8_t b, uint8_t v) {
  int16_t temp = b - v - (WPCSCPUregCC & WPCSCPU_F_CARRY);
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  if (temp & 0x100) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  setV8(b, v, temp);
  temp &= 0xFF;
  WPCSCPUregCC |= flagsNZ[temp];
  return temp;
}

static uint8_t oCMP(uint8_t b, uint8_t v) {
  int16_t temp = b - v;
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  if (temp & 0x100) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  setV8(b, v, temp);
  temp &= 0xFF;
  WPCSCPUregCC |= flagsNZ[temp];
  return temp;
}

static uint16_t oCMP16(uint16_t b, uint16_t v) {
  uint32_t temp = b - v;
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  if ((temp & 0xFFFF) == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  }
  if (temp & 0x8000) {
    WPCSCPUregCC |= WPCSCPU_F_NEGATIVE;
  }
  if (temp & 0x10000) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  setV16(b, v, temp);
  return temp;
}

static uint8_t oNEG(uint8_t b) {
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW | WPCSCPU_F_NEGATIVE);
  b = (0 - b) & 0xFF;
  if (b == 0x80) {
    WPCSCPUregCC |= WPCSCPU_F_OVERFLOW;
  }
  if (b == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  }
  if (b & 0x80) {
    WPCSCPUregCC |= WPCSCPU_F_NEGATIVE | WPCSCPU_F_CARRY;
  }
  return b;
}

static uint8_t oLSR(uint8_t b) {
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE);
  if (b & WPCSCPU_F_CARRY) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  b >>= 1;
  if (b == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  }
  return b;
}

static uint8_t oASR(uint8_t b) {
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE);
  if (b & 0x01) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  b = (b & 0x80) | (b >> 1);
  b &= 0xFF;
  WPCSCPUregCC |= flagsNZ[b];
  return b;
}

static uint8_t oASL(uint8_t b) {
  int16_t temp = b;
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  if (b & 0x80) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  b <<= 1;
  if ((b ^ temp) & 0x80) {
    WPCSCPUregCC |= WPCSCPU_F_OVERFLOW;
  }
  b &= 0xFF;
  WPCSCPUregCC |= flagsNZ[b];
  return b;
}

static uint8_t oROL(uint8_t b) {
  int16_t temp = b;
  uint8_t oldCarry = WPCSCPUregCC & WPCSCPU_F_CARRY;
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  if (b & 0x80) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  b = (b << 1) | oldCarry;
  if ((b ^ temp) & 0x80) {
    WPCSCPUregCC |= WPCSCPU_F_OVERFLOW;
  }
  b &= 0xFF;
  WPCSCPUregCC |= flagsNZ[b];
  return b;
}

static uint8_t oROR(uint8_t b) {
  uint8_t oldCarry = WPCSCPUregCC & WPCSCPU_F_CARRY;
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE);
  if (b & 0x01) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  b = (b >> 1) | (oldCarry << 7);
  b &= 0xFF;
  WPCSCPUregCC |= flagsNZ[b];
  return b;
}

static uint8_t oEOR(uint8_t b, uint8_t v) {
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  b ^= v;
  b &= 0xFF;
  WPCSCPUregCC |= flagsNZ[b];
  return b;
}

static uint8_t oOR(uint8_t b, uint8_t v) {
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  b |= v;
  b &= 0xFF;
  WPCSCPUregCC |= flagsNZ[b];
  return b;
}

static uint8_t oAND(uint8_t b, uint8_t v) {
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  b &= v;
  b &= 0xFF;
  WPCSCPUregCC |= flagsNZ[b];
  return b;
}

static uint8_t oCOM(uint8_t b) {
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  b ^= 0xFF;
  b &= 0xFF;
  WPCSCPUregCC |= flagsNZ[b];
  WPCSCPUregCC |= WPCSCPU_F_CARRY;
  return b;
}

//----common
static uint16_t dpadd() {
  //direct page + 8bit index
  return (WPCSCPUregDP << 8) + fetch();
}



typedef void (*OpcodeHandler)(void);

static void opUnimplemented(void) {
}

static void op_00(void) {
  //NEG DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oNEG(MPURead8(addr))
  );
}
static void op_03(void) {
  //COM DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oCOM(MPURead8(addr))
  );
}
static void op_04(void) {
  //LSR DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oLSR(MPURead8(addr))
  );
}
static void op_06(void) {
  //ROR DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oROR(MPURead8(addr))
  );
}
static void op_07(void) {
  //ASR DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oASR(MPURead8(addr))
  );
}
static void op_08(void) {
  //ASL DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oASL(MPURead8(addr))
  );
}
static void op_09(void) {
  //ROL DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oROL(MPURead8(addr))
  );
}
static void op_0A(void) {
  //DEC DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oDEC(MPURead8(addr))
  );
}
static void op_0C(void) {
  //INC DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(
    addr,
    oINC(MPURead8(addr))
  );
}
static void op_0D(void) {
  //TST DP
  int32_t addr;
  uint16_t pb;
  addr = dpadd();
  pb = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[pb];
}
static void op_0E(void) {
  //JMP DP
  int32_t addr;
  addr = dpadd();
  WPCSCPUregPC = addr;
}
static void op_0F(void) {
  //CLR DP
  int32_t addr;
  addr = dpadd();
  MPUWrite8(addr, 0);
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= WPCSCPU_F_ZERO;
}

// --- Page 1 (0x10) Handlers ---

static void op_p1_21(void) { // BRN
    fetch16(); // Consume offset, do nothing
}
static void op_p1_22(void) { // BHI
    int32_t addr = signed16(fetch16());
    if (!(WPCSCPUregCC & (WPCSCPU_F_CARRY | WPCSCPU_F_ZERO))) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_23(void) { // BLS
    int32_t addr = signed16(fetch16());
    if (WPCSCPUregCC & (WPCSCPU_F_CARRY | WPCSCPU_F_ZERO)) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_24(void) { // BCC
    int32_t addr = signed16(fetch16());
    if (!(WPCSCPUregCC & WPCSCPU_F_CARRY)) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_25(void) { // BCS
    int32_t addr = signed16(fetch16());
    if (WPCSCPUregCC & WPCSCPU_F_CARRY) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_26(void) { // BNE
    int32_t addr = signed16(fetch16());
    if (!(WPCSCPUregCC & WPCSCPU_F_ZERO)) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_27(void) { // LBEQ
    int32_t addr = signed16(fetch16());
    if (WPCSCPUregCC & WPCSCPU_F_ZERO) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_28(void) { // BVC
    int32_t addr = signed16(fetch16());
    if (!(WPCSCPUregCC & WPCSCPU_F_OVERFLOW)) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_29(void) { // BVS
    int32_t addr = signed16(fetch16());
    if (WPCSCPUregCC & WPCSCPU_F_OVERFLOW) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_2A(void) { // BPL
    int32_t addr = signed16(fetch16());
    if (!(WPCSCPUregCC & WPCSCPU_F_NEGATIVE)) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_2B(void) { // BMI
    int32_t addr = signed16(fetch16());
    if (WPCSCPUregCC & WPCSCPU_F_NEGATIVE) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_2C(void) { // BGE
    int32_t addr = signed16(fetch16());
    if (!((WPCSCPUregCC & WPCSCPU_F_NEGATIVE) ^ ((WPCSCPUregCC & WPCSCPU_F_OVERFLOW) << 2))) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_2D(void) { // BLT
    int32_t addr = signed16(fetch16());
    if ((WPCSCPUregCC & WPCSCPU_F_NEGATIVE) ^ ((WPCSCPUregCC & WPCSCPU_F_OVERFLOW) << 2)) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_2E(void) { // BGT
    int32_t addr = signed16(fetch16());
    if (!((WPCSCPUregCC & WPCSCPU_F_NEGATIVE) ^ ((WPCSCPUregCC & WPCSCPU_F_OVERFLOW) << 2) || WPCSCPUregCC & WPCSCPU_F_ZERO)) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}
static void op_p1_2F(void) { // BLE
    int32_t addr = signed16(fetch16());
    if ((WPCSCPUregCC & WPCSCPU_F_NEGATIVE) ^ ((WPCSCPUregCC & WPCSCPU_F_OVERFLOW) << 2) || WPCSCPUregCC & WPCSCPU_F_ZERO) { WPCSCPUregPC += addr; WPCSCPUTickCount++; }
}

static void op_p1_3F(void) { // SWI2
    WPCSCPUregCC |= WPCSCPU_F_ENTIRE;
    PUSHW(WPCSCPUregPC); PUSHW(WPCSCPUregU); PUSHW(WPCSCPUregY); PUSHW(WPCSCPUregX);
    PUSHB(WPCSCPUregDP); PUSHB(WPCSCPUregB); PUSHB(WPCSCPUregA); PUSHB(WPCSCPUregCC);
    WPCSCPUregPC = ReadWord(WPCSCPUvecSWI2);
}

// CMPD
static void op_p1_83(void) { oCMP16(getD(), fetch16()); }
static void op_p1_93(void) { oCMP16(getD(), ReadWord(dpadd())); }
static void op_p1_A3(void) { oCMP16(getD(), ReadWord(PostByte())); }
static void op_p1_B3(void) { oCMP16(getD(), ReadWord(fetch16())); }

// CMPY
static void op_p1_8C(void) { oCMP16(WPCSCPUregY, fetch16()); }
static void op_p1_9C(void) { oCMP16(WPCSCPUregY, ReadWord(dpadd())); }
static void op_p1_AC(void) { oCMP16(WPCSCPUregY, ReadWord(PostByte())); }
static void op_p1_BC(void) { oCMP16(WPCSCPUregY, ReadWord(fetch16())); }

// LDY
static void op_p1_8E(void) { WPCSCPUregY = fetch16(); flagsNZ16(WPCSCPUregY); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_9E(void) { WPCSCPUregY = ReadWord(dpadd()); flagsNZ16(WPCSCPUregY); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_AE(void) { WPCSCPUregY = ReadWord(PostByte()); flagsNZ16(WPCSCPUregY); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_BE(void) { WPCSCPUregY = ReadWord(fetch16()); flagsNZ16(WPCSCPUregY); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }

// STY
static void op_p1_9F(void) { int32_t addr = dpadd(); WriteWord(addr, WPCSCPUregY); flagsNZ16(WPCSCPUregY); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_AF(void) { int32_t addr = PostByte(); WriteWord(addr, WPCSCPUregY); flagsNZ16(WPCSCPUregY); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_BF(void) { int32_t addr = fetch16(); WriteWord(addr, WPCSCPUregY); flagsNZ16(WPCSCPUregY); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }

// LDS
static void op_p1_CE(void) { WPCSCPUregS = fetch16(); flagsNZ16(WPCSCPUregS); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_DE(void) { WPCSCPUregS = ReadWord(dpadd()); flagsNZ16(WPCSCPUregS); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_EE(void) { WPCSCPUregS = ReadWord(PostByte()); flagsNZ16(WPCSCPUregS); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_FE(void) { WPCSCPUregS = ReadWord(fetch16()); flagsNZ16(WPCSCPUregS); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }

// STS
static void op_p1_DF(void) { int32_t addr = dpadd(); WriteWord(addr, WPCSCPUregS); flagsNZ16(WPCSCPUregS); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_EF(void) { int32_t addr = PostByte(); WriteWord(addr, WPCSCPUregS); flagsNZ16(WPCSCPUregS); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }
static void op_p1_FF(void) { int32_t addr = fetch16(); WriteWord(addr, WPCSCPUregS); flagsNZ16(WPCSCPUregS); WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW; }

// --- Page 2 (0x11) Handlers ---

static void op_p2_3F(void) { // SWI3
    WPCSCPUregCC |= WPCSCPU_F_ENTIRE;
    PUSHW(WPCSCPUregPC); PUSHW(WPCSCPUregU); PUSHW(WPCSCPUregY); PUSHW(WPCSCPUregX);
    PUSHB(WPCSCPUregDP); PUSHB(WPCSCPUregB); PUSHB(WPCSCPUregA); PUSHB(WPCSCPUregCC);
    WPCSCPUregPC = ReadWord(WPCSCPUvecSWI3);
}

// CMPU
static void op_p2_83(void) { oCMP16(WPCSCPUregU, fetch16()); }
static void op_p2_93(void) { oCMP16(WPCSCPUregU, ReadWord(dpadd())); }
static void op_p2_A3(void) { oCMP16(WPCSCPUregU, ReadWord(PostByte())); }
static void op_p2_B3(void) { oCMP16(WPCSCPUregU, ReadWord(fetch16())); }

// CMPS
static void op_p2_8C(void) { oCMP16(WPCSCPUregS, fetch16()); }
static void op_p2_9C(void) { oCMP16(WPCSCPUregS, ReadWord(dpadd())); }
static void op_p2_AC(void) { oCMP16(WPCSCPUregS, ReadWord(PostByte())); }
static void op_p2_BC(void) { oCMP16(WPCSCPUregS, ReadWord(fetch16())); }

// Define a default/null handler for empty slots
#define __ 0 

static const OpcodeHandler oDispatchTablePage1[256] = {
    /* 0x00 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x10 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x20 */ __, op_p1_21, op_p1_22, op_p1_23, op_p1_24, op_p1_25, op_p1_26, op_p1_27,
    /* 0x28 */ op_p1_28, op_p1_29, op_p1_2A, op_p1_2B, op_p1_2C, op_p1_2D, op_p1_2E, op_p1_2F,
    /* 0x30 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, op_p1_3F,
    /* 0x40 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x50 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x60 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x70 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x80 */ __, __, __, op_p1_83, __, __, __, __, __, __, __, __, op_p1_8C, __, op_p1_8E, __,
    /* 0x90 */ __, __, __, op_p1_93, __, __, __, __, __, __, __, __, op_p1_9C, __, op_p1_9E, op_p1_9F,
    /* 0xA0 */ __, __, __, op_p1_A3, __, __, __, __, __, __, __, __, op_p1_AC, __, op_p1_AE, op_p1_AF,
    /* 0xB0 */ __, __, __, op_p1_B3, __, __, __, __, __, __, __, __, op_p1_BC, __, op_p1_BE, op_p1_BF,
    /* 0xC0 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, op_p1_CE, __,
    /* 0xD0 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, op_p1_DE, op_p1_DF,
    /* 0xE0 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, op_p1_EE, op_p1_EF,
    /* 0xF0 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, op_p1_FE, op_p1_FF
};

static const OpcodeHandler oDispatchTablePage2[256] = {
    /* 0x00 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x10 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x20 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x30 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, op_p2_3F,
    /* 0x40 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x50 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x60 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x70 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0x80 */ __, __, __, op_p2_83, __, __, __, __, __, __, __, __, op_p2_8C, __, __, __,
    /* 0x90 */ __, __, __, op_p2_93, __, __, __, __, __, __, __, __, op_p2_9C, __, __, __,
    /* 0xA0 */ __, __, __, op_p2_A3, __, __, __, __, __, __, __, __, op_p2_AC, __, __, __,
    /* 0xB0 */ __, __, __, op_p2_B3, __, __, __, __, __, __, __, __, op_p2_BC, __, __, __,
    /* 0xC0 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0xD0 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0xE0 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
    /* 0xF0 */ __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __
};

#undef __


static void op_10(void) {
  // Page 1
  uint8_t opcode = fetch();
  WPCSCPUTickCount += WPCSCPUCycles2[opcode];
  
  if (oDispatchTablePage1[opcode]) {
      oDispatchTablePage1[opcode]();
  } else {
      // Optional: Handle invalid Page 1 opcode
  }
}

static void op_11(void) {
  // Page 2

  uint8_t opcode = fetch();
  WPCSCPUTickCount += WPCSCPUCycles2[opcode];

  if (oDispatchTablePage2[opcode]) {
      oDispatchTablePage2[opcode]();
  } else {
      // Optional: Handle invalid Page 2 opcode
  }

}

static void op_12(void) {
  //NOP
}

static void op_13(void) {
  //SYNC
  /*
  This commands stops the CPU, brings the processor bus to high impedance state and waits for an interrupt.
  */
  //console.log('SYNC is broken!');

}

static void op_16(void) {
  //LBRA relative
  int32_t addr = fetch16();
  WPCSCPUregPC += addr;
}

static void op_17(void) {
  //LBSR relative
  int32_t addr = fetch16();
  PUSHW(WPCSCPUregPC);
  WPCSCPUregPC += addr;
}

static void op_19(void) {
  //DAA
  uint8_t correctionFactor = 0;
  uint8_t nhi = WPCSCPUregA & 0xF0;
  uint8_t nlo = WPCSCPUregA & 0x0F;
  int32_t addr;
  uint8_t oldCarry = WPCSCPUregCC & WPCSCPU_F_CARRY; // Save the incoming carry
  
  if (nlo > 0x09 || WPCSCPUregCC & WPCSCPU_F_HALFCARRY) {
    correctionFactor |= 0x06;
  }
  if (nhi > 0x80 && nlo > 0x09) {
    correctionFactor |= 0x60;
  }
  if (nhi > 0x90 || oldCarry) {
    correctionFactor |= 0x60;
  }
  
  addr = correctionFactor + WPCSCPUregA;
  
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE | WPCSCPU_F_ZERO | WPCSCPU_F_OVERFLOW);
  
  // Restore the original carry, or set it if the correction math generated a new one
  if (addr & 0x100 || oldCarry) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  }
  
  WPCSCPUregA = addr & 0xFF;
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA];
}

static void op_1A(void) {
  //ORCC
  WPCSCPUregCC |= fetch();
}

static void op_1C(void) {
  //ANDCC
  WPCSCPUregCC &= fetch();
}

static void op_1D(void) {
  //SEX
  //TODO should we use signed here?
  WPCSCPUregA = WPCSCPUregB & 0x80 ? 0xff : 0;
  flagsNZ16(getD());
}

static void op_1E(void) {
  //EXG
  uint16_t pb = fetch();
  TFREXG(pb, true);
}

static void op_1F(void) {
  //TFR
  uint16_t pb = fetch();
  TFREXG(pb, false);
}

static void op_20(void) {
  //BRA
  int32_t addr = signed8(fetch());
  WPCSCPUregPC += addr;
}

static void op_21(void) {
  //BRN
  fetch();
}

static void op_22(void) {
  //BHI
  int32_t addr = signed8(fetch());
  if (!(WPCSCPUregCC & (WPCSCPU_F_CARRY | WPCSCPU_F_ZERO))) {
    WPCSCPUregPC += addr;
  }
}

static void op_23(void) {
  //BLS
  int32_t addr = signed8(fetch());
  if (WPCSCPUregCC & (WPCSCPU_F_CARRY | WPCSCPU_F_ZERO)) {
    WPCSCPUregPC += addr;
  }
}

static void op_24(void) {
  //BCC
  int32_t addr = signed8(fetch());
  if (!(WPCSCPUregCC & WPCSCPU_F_CARRY)) {
    WPCSCPUregPC += addr;
  }
}

static void op_25(void) {
  //BCS
  int32_t addr = signed8(fetch());
  if (WPCSCPUregCC & WPCSCPU_F_CARRY) {
    WPCSCPUregPC += addr;
  }
}

static void op_26(void) {
  //BNE
  int32_t addr = signed8(fetch());
  if (!(WPCSCPUregCC & WPCSCPU_F_ZERO)) {
    WPCSCPUregPC += addr;
  }
}

static void op_27(void) {
  //BEQ
  int32_t addr = signed8(fetch());
  if (WPCSCPUregCC & WPCSCPU_F_ZERO) {
    WPCSCPUregPC += addr;
  }
}

static void op_28(void) {
  //BVC
  int32_t addr = signed8(fetch());
  if (!(WPCSCPUregCC & WPCSCPU_F_OVERFLOW)) {
    WPCSCPUregPC += addr;
  }
}

static void op_29(void) {
  //BVS
  int32_t addr = signed8(fetch());
  if (WPCSCPUregCC & WPCSCPU_F_OVERFLOW) {
    WPCSCPUregPC += addr;
  }
}

static void op_2A(void) {
  //BPL
  int32_t addr = signed8(fetch());
  if (!(WPCSCPUregCC & WPCSCPU_F_NEGATIVE)) {
    WPCSCPUregPC += addr;
  }
}

static void op_2B(void) {
  //BMI
  int32_t addr = signed8(fetch());
  if (WPCSCPUregCC & WPCSCPU_F_NEGATIVE) {
    WPCSCPUregPC += addr;
  }
}

static void op_2C(void) {
  //BGE
  int32_t addr = signed8(fetch());
  if (!((WPCSCPUregCC & WPCSCPU_F_NEGATIVE) ^ ((WPCSCPUregCC & WPCSCPU_F_OVERFLOW) << 2))) {
    WPCSCPUregPC += addr;
  }
}

static void op_2D(void) {
  //BLT
  int32_t addr = signed8(fetch());
  if ((WPCSCPUregCC & WPCSCPU_F_NEGATIVE) ^ ((WPCSCPUregCC & WPCSCPU_F_OVERFLOW) << 2)) {
    WPCSCPUregPC += addr;
  }
}

static void op_2E(void) {
  //BGT
  int32_t addr = signed8(fetch());
  if (
    !((WPCSCPUregCC & WPCSCPU_F_NEGATIVE) ^ ((WPCSCPUregCC & WPCSCPU_F_OVERFLOW) << 2) ||
      WPCSCPUregCC & WPCSCPU_F_ZERO)
  ) {
    WPCSCPUregPC += addr;
  }
}

static void op_2F(void) {
  //BLE
  int32_t addr = signed8(fetch());
  if (
    (WPCSCPUregCC & WPCSCPU_F_NEGATIVE) ^ ((WPCSCPUregCC & WPCSCPU_F_OVERFLOW) << 2) ||
    WPCSCPUregCC & WPCSCPU_F_ZERO
  ) {
    WPCSCPUregPC += addr;
  }
}

static void op_30(void) {
  //LEAX
  WPCSCPUregX = PostByte();
  WPCSCPUregCC &= ~WPCSCPU_F_ZERO;
  if (WPCSCPUregX == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  }
}

static void op_31(void) {
  //LEAY
  WPCSCPUregY = PostByte();
  WPCSCPUregCC &= ~WPCSCPU_F_ZERO;
  if (WPCSCPUregY == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  }
}

static void op_32(void) {
  //LEAS
  WPCSCPUregS = PostByte();
}

static void op_33(void) {
  //LEAU
  WPCSCPUregU = PostByte();
}

static void op_34(void) {
  //PSHS
  PSHS(fetch());
}

static void op_35(void) {
  //PULS
  PULS(fetch());
}

static void op_36(void) {
  //PSHU
  PSHU(fetch());
}

static void op_37(void) {
  //PULU
  PULU(fetch());
}

static void op_39(void) {
  //RTS
  WPCSCPUregPC = PULLW();
}

static void op_3A(void) {
  //ABX
  WPCSCPUregX += WPCSCPUregB;
}

static void op_3B(void) {
  //RTI
  WPCSCPUregCC = PULLB();
  //debug('RTI', WPCSCPUregCC & WPCSCPU_F_ENTIRE, WPCSCPUTickCount);
  // Check for fast interrupt
  if (WPCSCPUregCC & WPCSCPU_F_ENTIRE) {
    WPCSCPUTickCount += 9;
    WPCSCPUregA = PULLB();
    WPCSCPUregB = PULLB();
    WPCSCPUregDP = PULLB();
    WPCSCPUregX = PULLW();
    WPCSCPUregY = PULLW();
    WPCSCPUregU = PULLW();
  }
  WPCSCPUregPC = PULLW();
}

static void op_3C(void) {
  //CWAI
  //console.log('CWAI is broken!');
  /*
   * CWAI stacks the entire machine state on the hardware stack,
   * then waits for an interrupt; when the interrupt is taken
   * later, the state is *not* saved again after CWAI.
   * see mame-6809.c how to proper implement this opcode
   */
  WPCSCPUregCC &= fetch();
  //TODO - ??? set cwai flag to true, do not exec next interrupt (NMI, FIRQ, IRQ) - but set reset cwai flag afterwards

}

static void op_3D(void) {
  //MUL
  int32_t addr = WPCSCPUregA * WPCSCPUregB;
  if (addr == 0) {
    WPCSCPUregCC |= WPCSCPU_F_ZERO;
  } else {
    WPCSCPUregCC &= ~WPCSCPU_F_ZERO;
  }
  if (addr & 0x80) {
    WPCSCPUregCC |= WPCSCPU_F_CARRY;
  } else {
    WPCSCPUregCC &= ~WPCSCPU_F_CARRY;
  }
  setD(addr);
}

static void op_3F(void) {
  //SWI
  //console.log('SWI is untested!');
  WPCSCPUregCC |= WPCSCPU_F_ENTIRE;
  PUSHW(WPCSCPUregPC);
  PUSHW(WPCSCPUregU);
  PUSHW(WPCSCPUregY);
  PUSHW(WPCSCPUregX);
  PUSHB(WPCSCPUregDP);
  PUSHB(WPCSCPUregB);
  PUSHB(WPCSCPUregA);
  PUSHB(WPCSCPUregCC);
  WPCSCPUregCC |= WPCSCPU_F_IRQMASK | WPCSCPU_F_FIRQMASK;
  WPCSCPUregPC = ReadWord(WPCSCPUvecSWI);
}

static void op_40(void) {
  WPCSCPUregA = oNEG(WPCSCPUregA);
}

static void op_43(void) {
  WPCSCPUregA = oCOM(WPCSCPUregA);
}

static void op_44(void) {
  WPCSCPUregA = oLSR(WPCSCPUregA);
}

static void op_46(void) {
  WPCSCPUregA = oROR(WPCSCPUregA);
}

static void op_47(void) {
  WPCSCPUregA = oASR(WPCSCPUregA);
}

static void op_48(void) {
  WPCSCPUregA = oASL(WPCSCPUregA);
}

static void op_49(void) {
  WPCSCPUregA = oROL(WPCSCPUregA);
}

static void op_4A(void) {
  WPCSCPUregA = oDEC(WPCSCPUregA);
}

static void op_4C(void) {
  WPCSCPUregA = oINC(WPCSCPUregA);
}

static void op_4D(void) {
  // tsta
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA & 0xFF];
}

static void op_4F(void) {
  /* CLRA */
  WPCSCPUregA = 0;
  WPCSCPUregCC &= ~(WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW | WPCSCPU_F_CARRY);
  WPCSCPUregCC |= WPCSCPU_F_ZERO;
}

static void op_50(void) {
  /* NEGB */
  WPCSCPUregB = oNEG(WPCSCPUregB);
}

static void op_53(void) {
  WPCSCPUregB = oCOM(WPCSCPUregB);
}

static void op_54(void) {
  WPCSCPUregB = oLSR(WPCSCPUregB);
}

static void op_56(void) {
  WPCSCPUregB = oROR(WPCSCPUregB);
}

static void op_57(void) {
  WPCSCPUregB = oASR(WPCSCPUregB);
}

static void op_58(void) {
  WPCSCPUregB = oASL(WPCSCPUregB);
}

static void op_59(void) {
  WPCSCPUregB = oROL(WPCSCPUregB);
}

static void op_5A(void) {
  WPCSCPUregB = oDEC(WPCSCPUregB);
}

static void op_5C(void) {
  // INCB
  WPCSCPUregB = oINC(WPCSCPUregB);
}

static void op_5D(void) {
  /* TSTB */
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregB & 0xFF];
}

static void op_5F(void) {
  //CLRB
  WPCSCPUregB = 0;
  WPCSCPUregCC &= ~(WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW | WPCSCPU_F_CARRY);
  WPCSCPUregCC |= WPCSCPU_F_ZERO;
}

static void op_60(void) {
  //NEG indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oNEG(MPURead8(addr))
  );
}

static void op_63(void) {
  //COM indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oCOM(MPURead8(addr))
  );
}

static void op_64(void) {
  //LSR indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oLSR(MPURead8(addr))
  );
}

static void op_66(void) {
  //ROR indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oROR(MPURead8(addr))
  );
}

static void op_67(void) {
  //ASR indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oASR(MPURead8(addr))
  );
}

static void op_68(void) {
  //ASL indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oASL(MPURead8(addr))
  );
}

static void op_69(void) {
  //ROL indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oROL(MPURead8(addr))
  );
}

static void op_6A(void) {
  //DEC indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oDEC(MPURead8(addr))
  );
}

static void op_6C(void) {
  //INC indexed
  int32_t addr = PostByte();
  MPUWrite8(
    addr,
    oINC(MPURead8(addr))
  );
}

static void op_6D(void) {
  //TST indexed
  int32_t addr = PostByte();
  uint16_t pb = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[pb];
}

static void op_6E(void) {
  //JMP indexed
  int32_t addr = PostByte();
  WPCSCPUregPC = addr;
}

static void op_6F(void) {
  //CLR indexed
  int32_t addr = PostByte();
  MPUWrite8(addr, 0);
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= WPCSCPU_F_ZERO;
}

static void op_70(void) {
  //NEG extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oNEG(MPURead8(addr))
  );
}

static void op_73(void) {
  //COM extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oCOM(MPURead8(addr))
  );
}

static void op_74(void) {
  //LSR extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oLSR(MPURead8(addr))
  );
}

static void op_76(void) {
  //ROR extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oROR(MPURead8(addr))
  );
}

static void op_77(void) {
  //ASR extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oASR(MPURead8(addr))
  );
}

static void op_78(void) {
  //ASL extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oASL(MPURead8(addr))
  );
}

static void op_79(void) {
  //ROL extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oROL(MPURead8(addr))
  );
}

static void op_7A(void) {
  //DEC extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oDEC(MPURead8(addr))
  );
}

static void op_7C(void) {
  //INC extended
  int32_t addr = fetch16();
  MPUWrite8(
    addr,
    oINC(MPURead8(addr))
  );
}

static void op_7D(void) {
  //TST extended
  int32_t addr = fetch16();
  uint16_t pb = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[pb];
}

static void op_7E(void) {
  //JMP extended
  int32_t addr = fetch16();
  WPCSCPUregPC = addr;
}

static void op_7F(void) {
  //CLR extended
  int32_t addr = fetch16();
  MPUWrite8(addr, 0);
  WPCSCPUregCC &= ~(WPCSCPU_F_CARRY | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= WPCSCPU_F_ZERO;
}

static void op_80(void) {
  //SUBA imm
  WPCSCPUregA = oSUB(WPCSCPUregA, fetch());
}

static void op_81(void) {
  //CMPA imm
  oCMP(WPCSCPUregA, fetch());
}

static void op_82(void) {
  //SBCA imm
  WPCSCPUregA = oSBC(WPCSCPUregA, fetch());
}

static void op_83(void) {
  //SUBD imm
  setD(oSUB16(getD(), fetch16()));
}

static void op_84(void) {
  //ANDA imm
  WPCSCPUregA = oAND(WPCSCPUregA, fetch());
}

static void op_85(void) {
  //BITA imm
  oAND(WPCSCPUregA, fetch());
}

static void op_86(void) {
  //LDA imm
  WPCSCPUregA = fetch();
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA & 0xFF];
}

static void op_88(void) {
  //EORA imm
  WPCSCPUregA = oEOR(WPCSCPUregA, fetch());
}

static void op_89(void) {
  //ADCA imm
  WPCSCPUregA = oADC(WPCSCPUregA, fetch());
}

static void op_8A(void) {
  //ORA imm
  WPCSCPUregA = oOR(WPCSCPUregA, fetch());
}

static void op_8B(void) {
  //ADDA imm
  WPCSCPUregA = oADD(WPCSCPUregA, fetch());
}

static void op_8C(void) {
  //CMPX imm
  oCMP16(WPCSCPUregX, fetch16());
}

static void op_8D(void) {
  //JSR imm
  int32_t addr = signed8(fetch());
  PUSHW(WPCSCPUregPC);
  WPCSCPUregPC += addr;
}

static void op_8E(void) {
  //LDX imm
  WPCSCPUregX = fetch16();
  flagsNZ16(WPCSCPUregX);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_90(void) {
  //SUBA direct
  int32_t addr = dpadd();
  WPCSCPUregA = oSUB(WPCSCPUregA, MPURead8(addr));
}

static void op_91(void) {
  //CMPA direct
  int32_t addr = dpadd();
  oCMP(WPCSCPUregA, MPURead8(addr));
}

static void op_92(void) {
  //SBCA direct
  int32_t addr = dpadd();
  WPCSCPUregA = oSBC(WPCSCPUregA, MPURead8(addr));
}

static void op_93(void) {
  //SUBD direct
  int32_t addr = dpadd();
  setD(oSUB16(getD(), ReadWord(addr)));
}

static void op_94(void) {
  //ANDA direct
  int32_t addr = dpadd();
  WPCSCPUregA = oAND(WPCSCPUregA, MPURead8(addr));
}

static void op_95(void) {
  //BITA direct
  int32_t addr = dpadd();
  oAND(WPCSCPUregA, MPURead8(addr));
}

static void op_96(void) {
  //LDA direct
  int32_t addr = dpadd();
  WPCSCPUregA = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA & 0xFF];
}

static void op_97(void) {
  //STA direct
  int32_t addr = dpadd();
  MPUWrite8(addr, WPCSCPUregA);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA & 0xFF];
}

static void op_98(void) {
  //EORA direct
  int32_t addr = dpadd();
  WPCSCPUregA = oEOR(WPCSCPUregA, MPURead8(addr));
}

static void op_99(void) {
  //ADCA direct
  int32_t addr = dpadd();
  WPCSCPUregA = oADC(WPCSCPUregA, MPURead8(addr));
}

static void op_9A(void) {
  //ORA direct
  int32_t addr = dpadd();
  WPCSCPUregA = oOR(WPCSCPUregA, MPURead8(addr));
}

static void op_9B(void) {
  //ADDA direct
  int32_t addr = dpadd();
  WPCSCPUregA = oADD(WPCSCPUregA, MPURead8(addr));
}

static void op_9C(void) {
  //CMPX direct
  int32_t addr = dpadd();
  oCMP16(WPCSCPUregX, ReadWord(addr));
}

static void op_9D(void) {
  //JSR direct
  int32_t addr = dpadd();
  PUSHW(WPCSCPUregPC);
  WPCSCPUregPC = addr;
}

static void op_9E(void) {
  //LDX direct
  int32_t addr = dpadd();
  WPCSCPUregX = ReadWord(addr);
  flagsNZ16(WPCSCPUregX);
  WPCSCPUregCC &= ~(WPCSCPU_F_OVERFLOW);
}

static void op_9F(void) {
  //STX direct
  int32_t addr = dpadd();
  WriteWord(addr, WPCSCPUregX);
  flagsNZ16(WPCSCPUregX);
  WPCSCPUregCC &= ~(WPCSCPU_F_OVERFLOW);
}

static void op_A0(void) {
  //SUBA indexed
  int32_t addr = PostByte();
  WPCSCPUregA = oSUB(WPCSCPUregA, MPURead8(addr));
}

static void op_A1(void) {
  //CMPA indexed
  int32_t addr = PostByte();
  oCMP(WPCSCPUregA, MPURead8(addr));
}

static void op_A2(void) {
  //SBCA indexed
  int32_t addr = PostByte();
  WPCSCPUregA = oSBC(WPCSCPUregA, MPURead8(addr));
}

static void op_A3(void) {
  //SUBD indexed
  int32_t addr = PostByte();
  setD(oSUB16(getD(), ReadWord(addr)));
}

static void op_A4(void) {
  //ANDA indexed
  int32_t addr = PostByte();
  WPCSCPUregA = oAND(WPCSCPUregA, MPURead8(addr));
}

static void op_A5(void) {
  //BITA indexed
  int32_t addr = PostByte();
  oAND(WPCSCPUregA, MPURead8(addr));
}

static void op_A6(void) {
  //LDA indexed
  int32_t addr = PostByte();
  WPCSCPUregA = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA & 0xFF];
}

static void op_A7(void) {
  //STA indexed
  int32_t addr = PostByte();
  MPUWrite8(addr, WPCSCPUregA);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA & 0xFF];
}

static void op_A8(void) {
  //EORA indexed
  int32_t addr = PostByte();
  WPCSCPUregA = oEOR(WPCSCPUregA, MPURead8(addr));
}

static void op_A9(void) {
  //ADCA indexed
  int32_t addr = PostByte();
  WPCSCPUregA = oADC(WPCSCPUregA, MPURead8(addr));
}

static void op_AA(void) {
  //ORA indexed
  int32_t addr = PostByte();
  WPCSCPUregA = oOR(WPCSCPUregA, MPURead8(addr));
}

static void op_AB(void) {
  //ADDA indexed
  int32_t addr = PostByte();
  WPCSCPUregA = oADD(WPCSCPUregA, MPURead8(addr));
}

static void op_AC(void) {
  //CMPX indexed
  int32_t addr = PostByte();
  oCMP16(WPCSCPUregX, ReadWord(addr));
}

static void op_AD(void) {
  //JSR indexed
  int32_t addr = PostByte();
  PUSHW(WPCSCPUregPC);
  WPCSCPUregPC = addr;
}

static void op_AE(void) {
  //LDX indexed
  int32_t addr = PostByte();
  WPCSCPUregX = ReadWord(addr);
  flagsNZ16(WPCSCPUregX);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_AF(void) {
  //STX indexed
  int32_t addr = PostByte();
  WriteWord(addr, WPCSCPUregX);
  flagsNZ16(WPCSCPUregX);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_B0(void) {
  //SUBA extended
  int32_t addr = fetch16();
  WPCSCPUregA = oSUB(WPCSCPUregA, MPURead8(addr));
}

static void op_B1(void) {
  //CMPA extended
  int32_t addr = fetch16();
  oCMP(WPCSCPUregA, MPURead8(addr));
}

static void op_B2(void) {
  //SBCA extended
  int32_t addr = fetch16();
  WPCSCPUregA = oSBC(WPCSCPUregA, MPURead8(addr));
}

static void op_B3(void) {
  //SUBD extended
  int32_t addr = fetch16();
  setD(oSUB16(getD(), ReadWord(addr)));
}

static void op_B4(void) {
  //ANDA extended
  int32_t addr = fetch16();
  WPCSCPUregA = oAND(WPCSCPUregA, MPURead8(addr));
}

static void op_B5(void) {
  //BITA extended
  int32_t addr = fetch16();
  oAND(WPCSCPUregA, MPURead8(addr));
}

static void op_B6(void) {
  //LDA extended
  int32_t addr = fetch16();
  WPCSCPUregA = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA & 0xFF];
}

static void op_B7(void) {
  //STA extended
  int32_t addr = fetch16();
  MPUWrite8(addr, WPCSCPUregA);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregA & 0xFF];
}

static void op_B8(void) {
  //EORA extended
  int32_t addr = fetch16();
  WPCSCPUregA = oEOR(WPCSCPUregA, MPURead8(addr));
}

static void op_B9(void) {
  //ADCA extended
  int32_t addr = fetch16();
  WPCSCPUregA = oADC(WPCSCPUregA, MPURead8(addr));
}

static void op_BA(void) {
  //ORA extended
  int32_t addr = fetch16();
  WPCSCPUregA = oOR(WPCSCPUregA, MPURead8(addr));
}

static void op_BB(void) {
  //ADDA extended
  int32_t addr = fetch16();
  WPCSCPUregA = oADD(WPCSCPUregA, MPURead8(addr));
}

static void op_BC(void) {
  //CMPX extended
  int32_t addr = fetch16();
  oCMP16(WPCSCPUregX, ReadWord(addr));
}

static void op_BD(void) {
  //JSR extended
  int32_t addr = fetch16();
  PUSHW(WPCSCPUregPC);
  WPCSCPUregPC = addr;
}

static void op_BE(void) {
  //LDX extended
  int32_t addr = fetch16();
  WPCSCPUregX = ReadWord(addr);
  flagsNZ16(WPCSCPUregX);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_BF(void) {
  //STX extended
  int32_t addr = fetch16();
  WriteWord(addr, WPCSCPUregX);
  flagsNZ16(WPCSCPUregX);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_C0(void) {
  //SUBB imm
  WPCSCPUregB = oSUB(WPCSCPUregB, fetch());
}

static void op_C1(void) {
  //CMPB imm
  oCMP(WPCSCPUregB, fetch());
}

static void op_C2(void) {
  //SBCB imm
  WPCSCPUregB = oSBC(WPCSCPUregB, fetch());
}

static void op_C3(void) {
  //ADDD imm
  setD(oADD16(getD(), fetch16()));
}

static void op_C4(void) {
  //ANDB imm
  WPCSCPUregB = oAND(WPCSCPUregB, fetch());
}

static void op_C5(void) {
  //BITB imm
  oAND(WPCSCPUregB, fetch());
}

static void op_C6(void) {
  //LDB imm
  WPCSCPUregB = fetch();
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregB & 0xFF];
}

static void op_C8(void) {
  //EORB imm
  WPCSCPUregB = oEOR(WPCSCPUregB, fetch());
}

static void op_C9(void) {
  //ADCB imm
  WPCSCPUregB = oADC(WPCSCPUregB, fetch());
}

static void op_CA(void) {
  //ORB imm
  WPCSCPUregB = oOR(WPCSCPUregB, fetch());
}

static void op_CB(void) {
  //ADDB imm
  WPCSCPUregB = oADD(WPCSCPUregB, fetch());
}

static void op_CC(void) {
  //LDD imm
  int32_t addr = fetch16();
  setD(addr);
  flagsNZ16(addr);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_CE(void) {
  //LDU imm
  WPCSCPUregU = fetch16();
  flagsNZ16(WPCSCPUregU);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_D0(void) {
  //SUBB direct
  int32_t addr = dpadd();
  WPCSCPUregB = oSUB(WPCSCPUregB, MPURead8(addr));
}

static void op_D1(void) {
  //CMPB direct
  int32_t addr = dpadd();
  oCMP(WPCSCPUregB, MPURead8(addr));
}

static void op_D2(void) {
  //SBCB direct
  int32_t addr = dpadd();
  WPCSCPUregB = oSBC(WPCSCPUregB, MPURead8(addr));
}

static void op_D3(void) {
  //ADDD direct
  int32_t addr = dpadd();
  setD(oADD16(getD(), ReadWord(addr)));
}

static void op_D4(void) {
  //ANDB direct
  int32_t addr = dpadd();
  WPCSCPUregB = oAND(WPCSCPUregB, MPURead8(addr));
}

static void op_D5(void) {
  //BITB direct
  int32_t addr = dpadd();
  oAND(WPCSCPUregB, MPURead8(addr));
}

static void op_D6(void) {
  //LDB direct
  int32_t addr = dpadd();
  WPCSCPUregB = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregB & 0xFF];
}

static void op_D7(void) {
  //STB direct
  int32_t addr = dpadd();
  MPUWrite8(addr, WPCSCPUregB);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregB & 0xFF];
}

static void op_D8(void) {
  //EORB direct
  int32_t addr = dpadd();
  WPCSCPUregB = oEOR(WPCSCPUregB, MPURead8(addr));
}

static void op_D9(void) {
  //ADCB direct
  int32_t addr = dpadd();
  WPCSCPUregB = oADC(WPCSCPUregB, MPURead8(addr));
}

static void op_DA(void) {
  //ORB direct
  int32_t addr = dpadd();
  WPCSCPUregB = oOR(WPCSCPUregB, MPURead8(addr));
}

static void op_DB(void) {
  //ADDB direct
  int32_t addr = dpadd();
  WPCSCPUregB = oADD(WPCSCPUregB, MPURead8(addr));
}

static void op_DC(void) {
  //LDD direct
  int32_t addr = dpadd();
  uint16_t pb = ReadWord(addr);
  setD(pb);
  flagsNZ16(pb);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_DD(void) {
  //STD direct
  int32_t addr = dpadd();
  WriteWord(addr, getD());
  flagsNZ16(getD());
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_DE(void) {
  //LDU direct
  int32_t addr = dpadd();
  WPCSCPUregU = ReadWord(addr);
  flagsNZ16(WPCSCPUregU);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_DF(void) {
  //STU direct
  int32_t addr = dpadd();
  WriteWord(addr, WPCSCPUregU);
  flagsNZ16(WPCSCPUregU);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_E0(void) {
  //SUBB indexed
  int32_t addr = PostByte();
  WPCSCPUregB = oSUB(WPCSCPUregB, MPURead8(addr));
}

static void op_E1(void) {
  //CMPB indexed
  int32_t addr = PostByte();
  oCMP(WPCSCPUregB, MPURead8(addr));
}

static void op_E2(void) {
  //SBCB indexed
  int32_t addr = PostByte();
  WPCSCPUregB = oSBC(WPCSCPUregB, MPURead8(addr));
}

static void op_E3(void) {
  //ADDD indexed
  int32_t addr = PostByte();
  setD(oADD16(getD(), ReadWord(addr)));
}

static void op_E4(void) {
  //ANDB indexed
  int32_t addr = PostByte();
  WPCSCPUregB = oAND(WPCSCPUregB, MPURead8(addr));
}

static void op_E5(void) {
  //BITB indexed
  int32_t addr = PostByte();
  oAND(WPCSCPUregB, MPURead8(addr));
}

static void op_E6(void) {
  //LDB indexed
  int32_t addr = PostByte();
  WPCSCPUregB = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregB & 0xFF];
}

static void op_E7(void) {
  //STB indexed
  int32_t addr = PostByte();
  MPUWrite8(addr, WPCSCPUregB);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregB & 0xFF];
}

static void op_E8(void) {
  //EORB indexed
  int32_t addr = PostByte();
  WPCSCPUregB = oEOR(WPCSCPUregB, MPURead8(addr));
}

static void op_E9(void) {
  //ADCB indexed
  int32_t addr = PostByte();
  WPCSCPUregB = oADC(WPCSCPUregB, MPURead8(addr));
}

static void op_EA(void) {
  //ORB indexed
  int32_t addr = PostByte();
  WPCSCPUregB = oOR(WPCSCPUregB, MPURead8(addr));
}

static void op_EB(void) {
  //ADDB indexed
  int32_t addr = PostByte();
  WPCSCPUregB = oADD(WPCSCPUregB, MPURead8(addr));
}

static void op_EC(void) {
  //LDD indexed
  int32_t addr = PostByte();
  uint16_t pb = ReadWord(addr);
  setD(pb);
  flagsNZ16(pb);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_ED(void) {
  //STD indexed
  int32_t addr = PostByte();
  WriteWord(addr, getD());
  flagsNZ16(getD());
  WPCSCPUregCC &= ~(WPCSCPU_F_OVERFLOW);
}

static void op_EE(void) {
  //LDU indexed
  int32_t addr = PostByte();
  WPCSCPUregU = ReadWord(addr);
  flagsNZ16(WPCSCPUregU);
  WPCSCPUregCC &= ~(WPCSCPU_F_OVERFLOW);
}

static void op_EF(void) {
  //STU indexed
  int32_t addr = PostByte();
  WriteWord(addr, WPCSCPUregU);
  flagsNZ16(WPCSCPUregU);
  WPCSCPUregCC &= ~(WPCSCPU_F_OVERFLOW);
}

static void op_F0(void) {
  //SUBB extended
  int32_t addr = fetch16();
  WPCSCPUregB = oSUB(WPCSCPUregB, MPURead8(addr));
}

static void op_F1(void) {
  //CMPB extended
  int32_t addr = fetch16();
  oCMP(WPCSCPUregB, MPURead8(addr));
}

static void op_F2(void) {
  //SBCB extended
  int32_t addr = fetch16();
  WPCSCPUregB = oSBC(WPCSCPUregB, MPURead8(addr));
}

static void op_F3(void) {
  //ADDD extended
  int32_t addr = fetch16();
  setD(oADD16(getD(), ReadWord(addr)));
}

static void op_F4(void) {
  //ANDB extended
  int32_t addr = fetch16();
  WPCSCPUregB = oAND(WPCSCPUregB, MPURead8(addr));
}

static void op_F5(void) {
  //BITB extended
  int32_t addr = fetch16();
  oAND(WPCSCPUregB, MPURead8(addr));
}

static void op_F6(void) {
  //LDB extended
  int32_t addr = fetch16();
  WPCSCPUregB = MPURead8(addr);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregB & 0xFF];
}

static void op_F7(void) {
  //STB extended
  int32_t addr = fetch16();
  MPUWrite8(addr, WPCSCPUregB);
  WPCSCPUregCC &= ~(WPCSCPU_F_ZERO | WPCSCPU_F_NEGATIVE | WPCSCPU_F_OVERFLOW);
  WPCSCPUregCC |= flagsNZ[WPCSCPUregB & 0xFF];
}

static void op_F8(void) {
  //EORB extended
  int32_t addr = fetch16();
  WPCSCPUregB = oEOR(WPCSCPUregB, MPURead8(addr));
}

static void op_F9(void) {
  //ADCB extended
  int32_t addr = fetch16();
  WPCSCPUregB = oADC(WPCSCPUregB, MPURead8(addr));
}

static void op_FA(void) {
  //ORB extended
  int32_t addr = fetch16();
  WPCSCPUregB = oOR(WPCSCPUregB, MPURead8(addr));
}

static void op_FB(void) {
  //ADDB extended
  int32_t addr = fetch16();
  WPCSCPUregB = oADD(WPCSCPUregB, MPURead8(addr));
}

static void op_FC(void) {
  //LDD extended
  int32_t addr = fetch16();
  uint16_t pb = ReadWord(addr);
  setD(pb);
  flagsNZ16(pb);
  WPCSCPUregCC &= ~WPCSCPU_F_OVERFLOW;
}

static void op_FD(void) {
  //STD extended
  int32_t addr = fetch16();
  WriteWord(addr, getD());
  flagsNZ16(getD());
  WPCSCPUregCC &= ~(WPCSCPU_F_OVERFLOW);
}

static void op_FE(void) {
  //LDU extended
  int32_t addr = fetch16();
  WPCSCPUregU = ReadWord(addr);
  flagsNZ16(WPCSCPUregU);
  WPCSCPUregCC &= ~(WPCSCPU_F_OVERFLOW);
}

static void op_FF(void) {
  //STU extended
  int32_t addr = fetch16();
  WriteWord(addr, WPCSCPUregU);
  flagsNZ16(WPCSCPUregU);
  WPCSCPUregCC &= ~(WPCSCPU_F_OVERFLOW);
}

/*
static const OpcodeHandler oDispatchTable[256] = {
    [0x00] = op_00,
    [0x01] = opUnimplemented,
    [0x02] = opUnimplemented,
    [0x03] = op_03,
    [0x04] = op_04,
    [0x05] = opUnimplemented,
    [0x06] = op_06,
    [0x07] = op_07,
    [0x08] = op_08,
    [0x09] = op_09,
    [0x0A] = op_0A,
    [0x0B] = opUnimplemented,
    [0x0C] = op_0C,
    [0x0D] = op_0D,
    [0x0E] = op_0E,
    [0x0F] = op_0F,
    [0x10] = op_10,
    [0x11] = op_11,
    [0x12] = op_12,
    [0x13] = op_13,
    [0x14] = opUnimplemented,
    [0x15] = opUnimplemented,
    [0x16] = op_16,
    [0x17] = op_17,
    [0x18] = opUnimplemented,
    [0x19] = op_19,
    [0x1A] = op_1A,
    [0x1B] = opUnimplemented,
    [0x1C] = op_1C,
    [0x1D] = op_1D,
    [0x1E] = op_1E,
    [0x1F] = op_1F,
    [0x20] = op_20,
    [0x21] = op_21,
    [0x22] = op_22,
    [0x23] = op_23,
    [0x24] = op_24,
    [0x25] = op_25,
    [0x26] = op_26,
    [0x27] = op_27,
    [0x28] = op_28,
    [0x29] = op_29,
    [0x2A] = op_2A,
    [0x2B] = op_2B,
    [0x2C] = op_2C,
    [0x2D] = op_2D,
    [0x2E] = op_2E,
    [0x2F] = op_2F,
    [0x30] = op_30,
    [0x31] = op_31,
    [0x32] = op_32,
    [0x33] = op_33,
    [0x34] = op_34,
    [0x35] = op_35,
    [0x36] = op_36,
    [0x37] = op_37,
    [0x38] = opUnimplemented,
    [0x39] = op_39,
    [0x3A] = op_3A,
    [0x3B] = op_3B,
    [0x3C] = op_3C,
    [0x3D] = op_3D,
    [0x3E] = opUnimplemented,
    [0x3F] = op_3F,
    [0x40] = op_40,
    [0x41] = opUnimplemented,
    [0x42] = opUnimplemented,
    [0x43] = op_43,
    [0x44] = op_44,
    [0x45] = opUnimplemented,
    [0x46] = op_46,
    [0x47] = op_47,
    [0x48] = op_48,
    [0x49] = op_49,
    [0x4A] = op_4A,
    [0x4B] = opUnimplemented,
    [0x4C] = op_4C,
    [0x4D] = op_4D,
    [0x4E] = opUnimplemented,
    [0x4F] = op_4F,
    [0x50] = op_50,
    [0x51] = opUnimplemented,
    [0x52] = opUnimplemented,
    [0x53] = op_53,
    [0x54] = op_54,
    [0x55] = opUnimplemented,
    [0x56] = op_56,
    [0x57] = op_57,
    [0x58] = op_58,
    [0x59] = op_59,
    [0x5A] = op_5A,
    [0x5B] = opUnimplemented,
    [0x5C] = op_5C,
    [0x5D] = op_5D,
    [0x5E] = opUnimplemented,
    [0x5F] = op_5F,
    [0x60] = op_60,
    [0x61] = opUnimplemented,
    [0x62] = opUnimplemented,
    [0x63] = op_63,
    [0x64] = op_64,
    [0x65] = opUnimplemented,
    [0x66] = op_66,
    [0x67] = op_67,
    [0x68] = op_68,
    [0x69] = op_69,
    [0x6A] = op_6A,
    [0x6B] = opUnimplemented,
    [0x6C] = op_6C,
    [0x6D] = op_6D,
    [0x6E] = op_6E,
    [0x6F] = op_6F,
    [0x70] = op_70,
    [0x71] = opUnimplemented,
    [0x72] = opUnimplemented,
    [0x73] = op_73,
    [0x74] = op_74,
    [0x75] = opUnimplemented,
    [0x76] = op_76,
    [0x77] = op_77,
    [0x78] = op_78,
    [0x79] = op_79,
    [0x7A] = op_7A,
    [0x7B] = opUnimplemented,
    [0x7C] = op_7C,
    [0x7D] = op_7D,
    [0x7E] = op_7E,
    [0x7F] = op_7F,
    [0x80] = op_80,
    [0x81] = op_81,
    [0x82] = op_82,
    [0x83] = op_83,
    [0x84] = op_84,
    [0x85] = op_85,
    [0x86] = op_86,
    [0x87] = opUnimplemented,
    [0x88] = op_88,
    [0x89] = op_89,
    [0x8A] = op_8A,
    [0x8B] = op_8B,
    [0x8C] = op_8C,
    [0x8D] = op_8D,
    [0x8E] = op_8E,
    [0x8F] = opUnimplemented,
    [0x90] = op_90,
    [0x91] = op_91,
    [0x92] = op_92,
    [0x93] = op_93,
    [0x94] = op_94,
    [0x95] = op_95,
    [0x96] = op_96,
    [0x97] = op_97,
    [0x98] = op_98,
    [0x99] = op_99,
    [0x9A] = op_9A,
    [0x9B] = op_9B,
    [0x9C] = op_9C,
    [0x9D] = op_9D,
    [0x9E] = op_9E,
    [0x9F] = op_9F,
    [0xA0] = op_A0,
    [0xA1] = op_A1,
    [0xA2] = op_A2,
    [0xA3] = op_A3,
    [0xA4] = op_A4,
    [0xA5] = op_A5,
    [0xA6] = op_A6,
    [0xA7] = op_A7,
    [0xA8] = op_A8,
    [0xA9] = op_A9,
    [0xAA] = op_AA,
    [0xAB] = op_AB,
    [0xAC] = op_AC,
    [0xAD] = op_AD,
    [0xAE] = op_AE,
    [0xAF] = op_AF,
    [0xB0] = op_B0,
    [0xB1] = op_B1,
    [0xB2] = op_B2,
    [0xB3] = op_B3,
    [0xB4] = op_B4,
    [0xB5] = op_B5,
    [0xB6] = op_B6,
    [0xB7] = op_B7,
    [0xB8] = op_B8,
    [0xB9] = op_B9,
    [0xBA] = op_BA,
    [0xBB] = op_BB,
    [0xBC] = op_BC,
    [0xBD] = op_BD,
    [0xBE] = op_BE,
    [0xBF] = op_BF,
    [0xC0] = op_C0,
    [0xC1] = op_C1,
    [0xC2] = op_C2,
    [0xC3] = op_C3,
    [0xC4] = op_C4,
    [0xC5] = op_C5,
    [0xC6] = op_C6,
    [0xC7] = opUnimplemented,
    [0xC8] = op_C8,
    [0xC9] = op_C9,
    [0xCA] = op_CA,
    [0xCB] = op_CB,
    [0xCC] = op_CC,
    [0xCD] = opUnimplemented,
    [0xCE] = op_CE,
    [0xCF] = opUnimplemented,
    [0xD0] = op_D0,
    [0xD1] = op_D1,
    [0xD2] = op_D2,
    [0xD3] = op_D3,
    [0xD4] = op_D4,
    [0xD5] = op_D5,
    [0xD6] = op_D6,
    [0xD7] = op_D7,
    [0xD8] = op_D8,
    [0xD9] = op_D9,
    [0xDA] = op_DA,
    [0xDB] = op_DB,
    [0xDC] = op_DC,
    [0xDD] = op_DD,
    [0xDE] = op_DE,
    [0xDF] = op_DF,
    [0xE0] = op_E0,
    [0xE1] = op_E1,
    [0xE2] = op_E2,
    [0xE3] = op_E3,
    [0xE4] = op_E4,
    [0xE5] = op_E5,
    [0xE6] = op_E6,
    [0xE7] = op_E7,
    [0xE8] = op_E8,
    [0xE9] = op_E9,
    [0xEA] = op_EA,
    [0xEB] = op_EB,
    [0xEC] = op_EC,
    [0xED] = op_ED,
    [0xEE] = op_EE,
    [0xEF] = op_EF,
    [0xF0] = op_F0,
    [0xF1] = op_F1,
    [0xF2] = op_F2,
    [0xF3] = op_F3,
    [0xF4] = op_F4,
    [0xF5] = op_F5,
    [0xF6] = op_F6,
    [0xF7] = op_F7,
    [0xF8] = op_F8,
    [0xF9] = op_F9,
    [0xFA] = op_FA,
    [0xFB] = op_FB,
    [0xFC] = op_FC,
    [0xFD] = op_FD,
    [0xFE] = op_FE,
    [0xFF] = op_FF
};
*/

static void WPCSCPUDispatch(uint8_t opcode) {
  switch (opcode) {
    case 0x00: op_00(); break;
    case 0x03: op_03(); break;
    case 0x04: op_04(); break;
    case 0x06: op_06(); break;
    case 0x07: op_07(); break;
    case 0x08: op_08(); break;
    case 0x09: op_09(); break;
    case 0x0A: op_0A(); break;
    case 0x0C: op_0C(); break;
    case 0x0D: op_0D(); break;
    case 0x0E: op_0E(); break;
    case 0x0F: op_0F(); break;
    case 0x10: op_10(); break;
    case 0x11: op_11(); break;
    case 0x12: op_12(); break;
    case 0x13: op_13(); break;
    case 0x16: op_16(); break;
    case 0x17: op_17(); break;
    case 0x19: op_19(); break;
    case 0x1A: op_1A(); break;
    case 0x1C: op_1C(); break;
    case 0x1D: op_1D(); break;
    case 0x1E: op_1E(); break;
    case 0x1F: op_1F(); break;
    case 0x20: op_20(); break;
    case 0x21: op_21(); break;
    case 0x22: op_22(); break;
    case 0x23: op_23(); break;
    case 0x24: op_24(); break;
    case 0x25: op_25(); break;
    case 0x26: op_26(); break;
    case 0x27: op_27(); break;
    case 0x28: op_28(); break;
    case 0x29: op_29(); break;
    case 0x2A: op_2A(); break;
    case 0x2B: op_2B(); break;
    case 0x2C: op_2C(); break;
    case 0x2D: op_2D(); break;
    case 0x2E: op_2E(); break;
    case 0x2F: op_2F(); break;
    case 0x30: op_30(); break;
    case 0x31: op_31(); break;
    case 0x32: op_32(); break;
    case 0x33: op_33(); break;
    case 0x34: op_34(); break;
    case 0x35: op_35(); break;
    case 0x36: op_36(); break;
    case 0x37: op_37(); break;
    case 0x39: op_39(); break;
    case 0x3A: op_3A(); break;
    case 0x3B: op_3B(); break;
    case 0x3C: op_3C(); break;
    case 0x3D: op_3D(); break;
    case 0x3F: op_3F(); break;
    case 0x40: op_40(); break;
    case 0x43: op_43(); break;
    case 0x44: op_44(); break;
    case 0x46: op_46(); break;
    case 0x47: op_47(); break;
    case 0x48: op_48(); break;
    case 0x49: op_49(); break;
    case 0x4A: op_4A(); break;
    case 0x4C: op_4C(); break;
    case 0x4D: op_4D(); break;
    case 0x4F: op_4F(); break;
    case 0x50: op_50(); break;
    case 0x53: op_53(); break;
    case 0x54: op_54(); break;
    case 0x56: op_56(); break;
    case 0x57: op_57(); break;
    case 0x58: op_58(); break;
    case 0x59: op_59(); break;
    case 0x5A: op_5A(); break;
    case 0x5C: op_5C(); break;
    case 0x5D: op_5D(); break;
    case 0x5F: op_5F(); break;
    case 0x60: op_60(); break;
    case 0x63: op_63(); break;
    case 0x64: op_64(); break;
    case 0x66: op_66(); break;
    case 0x67: op_67(); break;
    case 0x68: op_68(); break;
    case 0x69: op_69(); break;
    case 0x6A: op_6A(); break;
    case 0x6C: op_6C(); break;
    case 0x6D: op_6D(); break;
    case 0x6E: op_6E(); break;
    case 0x6F: op_6F(); break;
    case 0x70: op_70(); break;
    case 0x73: op_73(); break;
    case 0x74: op_74(); break;
    case 0x76: op_76(); break;
    case 0x77: op_77(); break;
    case 0x78: op_78(); break;
    case 0x79: op_79(); break;
    case 0x7A: op_7A(); break;
    case 0x7C: op_7C(); break;
    case 0x7D: op_7D(); break;
    case 0x7E: op_7E(); break;
    case 0x7F: op_7F(); break;
    case 0x80: op_80(); break;
    case 0x81: op_81(); break;
    case 0x82: op_82(); break;
    case 0x83: op_83(); break;
    case 0x84: op_84(); break;
    case 0x85: op_85(); break;
    case 0x86: op_86(); break;
    case 0x88: op_88(); break;
    case 0x89: op_89(); break;
    case 0x8A: op_8A(); break;
    case 0x8B: op_8B(); break;
    case 0x8C: op_8C(); break;
    case 0x8D: op_8D(); break;
    case 0x8E: op_8E(); break;
    case 0x90: op_90(); break;
    case 0x91: op_91(); break;
    case 0x92: op_92(); break;
    case 0x93: op_93(); break;
    case 0x94: op_94(); break;
    case 0x95: op_95(); break;
    case 0x96: op_96(); break;
    case 0x97: op_97(); break;
    case 0x98: op_98(); break;
    case 0x99: op_99(); break;
    case 0x9A: op_9A(); break;
    case 0x9B: op_9B(); break;
    case 0x9C: op_9C(); break;
    case 0x9D: op_9D(); break;
    case 0x9E: op_9E(); break;
    case 0x9F: op_9F(); break;
    case 0xA0: op_A0(); break;
    case 0xA1: op_A1(); break;
    case 0xA2: op_A2(); break;
    case 0xA3: op_A3(); break;
    case 0xA4: op_A4(); break;
    case 0xA5: op_A5(); break;
    case 0xA6: op_A6(); break;
    case 0xA7: op_A7(); break;
    case 0xA8: op_A8(); break;
    case 0xA9: op_A9(); break;
    case 0xAA: op_AA(); break;
    case 0xAB: op_AB(); break;
    case 0xAC: op_AC(); break;
    case 0xAD: op_AD(); break;
    case 0xAE: op_AE(); break;
    case 0xAF: op_AF(); break;
    case 0xB0: op_B0(); break;
    case 0xB1: op_B1(); break;
    case 0xB2: op_B2(); break;
    case 0xB3: op_B3(); break;
    case 0xB4: op_B4(); break;
    case 0xB5: op_B5(); break;
    case 0xB6: op_B6(); break;
    case 0xB7: op_B7(); break;
    case 0xB8: op_B8(); break;
    case 0xB9: op_B9(); break;
    case 0xBA: op_BA(); break;
    case 0xBB: op_BB(); break;
    case 0xBC: op_BC(); break;
    case 0xBD: op_BD(); break;
    case 0xBE: op_BE(); break;
    case 0xBF: op_BF(); break;
    case 0xC0: op_C0(); break;
    case 0xC1: op_C1(); break;
    case 0xC2: op_C2(); break;
    case 0xC3: op_C3(); break;
    case 0xC4: op_C4(); break;
    case 0xC5: op_C5(); break;
    case 0xC6: op_C6(); break;
    case 0xC8: op_C8(); break;
    case 0xC9: op_C9(); break;
    case 0xCA: op_CA(); break;
    case 0xCB: op_CB(); break;
    case 0xCC: op_CC(); break;
    case 0xCE: op_CE(); break;
    case 0xD0: op_D0(); break;
    case 0xD1: op_D1(); break;
    case 0xD2: op_D2(); break;
    case 0xD3: op_D3(); break;
    case 0xD4: op_D4(); break;
    case 0xD5: op_D5(); break;
    case 0xD6: op_D6(); break;
    case 0xD7: op_D7(); break;
    case 0xD8: op_D8(); break;
    case 0xD9: op_D9(); break;
    case 0xDA: op_DA(); break;
    case 0xDB: op_DB(); break;
    case 0xDC: op_DC(); break;
    case 0xDD: op_DD(); break;
    case 0xDE: op_DE(); break;
    case 0xDF: op_DF(); break;
    case 0xE0: op_E0(); break;
    case 0xE1: op_E1(); break;
    case 0xE2: op_E2(); break;
    case 0xE3: op_E3(); break;
    case 0xE4: op_E4(); break;
    case 0xE5: op_E5(); break;
    case 0xE6: op_E6(); break;
    case 0xE7: op_E7(); break;
    case 0xE8: op_E8(); break;
    case 0xE9: op_E9(); break;
    case 0xEA: op_EA(); break;
    case 0xEB: op_EB(); break;
    case 0xEC: op_EC(); break;
    case 0xED: op_ED(); break;
    case 0xEE: op_EE(); break;
    case 0xEF: op_EF(); break;
    case 0xF0: op_F0(); break;
    case 0xF1: op_F1(); break;
    case 0xF2: op_F2(); break;
    case 0xF3: op_F3(); break;
    case 0xF4: op_F4(); break;
    case 0xF5: op_F5(); break;
    case 0xF6: op_F6(); break;
    case 0xF7: op_F7(); break;
    case 0xF8: op_F8(); break;
    case 0xF9: op_F9(); break;
    case 0xFA: op_FA(); break;
    case 0xFB: op_FB(); break;
    case 0xFC: op_FC(); break;
    case 0xFD: op_FD(); break;
    case 0xFE: op_FE(); break;
    case 0xFF: op_FF(); break;
    default:   opUnimplemented(); break;
  }
}
 

uint32_t WPCS_CPUNumberOfFirqsHandled = 0;

uint16_t WPCSCPUStep() {
  uint16_t oldWPCSCPUTickCount = WPCSCPUTickCount;
  //WPCSCPUfirqPending = (g_YmStatus & 0x03) ? true : false;
  //if (((WPCSCPUregCC & WPCSCPU_F_IRQMASK) == 0) && (g_YmStatus & 0x03)) WPCSCPUfirqPending = true;

  // Top of WPCSCPUStep
//  if ((g_YmStatus & 0x03) && (WPCSCPUregCC & WPCSCPU_F_FIRQMASK)) {
//  }

  // LATCH IRQ lines, see 6803 diagram "figure3-1.jpg"
  if (WPCSCPUfirqPending) {
    if ((WPCSCPUregCC & WPCSCPU_F_FIRQMASK) == 0) {
      WPCSCPUfirqPending = false;
      WPCSCPUfirqCount++;
      WPCSCPU_executeFirq();
      WPCS_CPUNumberOfFirqsHandled += 1;
      return WPCSCPUTickCount - oldWPCSCPUTickCount;
    }
    WPCSCPUmissedFIRQ++;
  }

  if (WPCSCPUirqPending) {
    if ((WPCSCPUregCC & WPCSCPU_F_IRQMASK) == 0) {
      WPCSCPUirqPending = false;
      WPCSCPUirqCount++;
      WPCSCPU_executeIrq();
      return WPCSCPUTickCount - oldWPCSCPUTickCount;
    }
    WPCSCPUmissedIRQ++;
  }

//  int32_t addr = 0;
//  uint16_t pb = 0;
  uint8_t opcode = fetch();
  WPCSCPUTickCount += WPCSCPUCycles[opcode];

  // This approach is a LUT of opcode functions
  //oDispatchTable[opcode]();

  // This approach is a dispatch switch
  WPCSCPUDispatch(opcode);
//  if (((WPCSCPUregCC & WPCSCPU_F_IRQMASK) == 0) && (g_YmStatus & 0x03)) WPCSCPUfirqPending = true;

  WPCSCPUregA &= 0xff;
  WPCSCPUregB &= 0xff;
  WPCSCPUregCC &= 0xff;
  WPCSCPUregDP &= 0xff;
  WPCSCPUregX &= 0xffff;
  WPCSCPUregY &= 0xffff;
  WPCSCPUregU &= 0xffff;
  WPCSCPUregS &= 0xffff;
  WPCSCPUregPC &= 0xffff;

  return WPCSCPUTickCount - oldWPCSCPUTickCount;
}

void WPCSCPUReset() {
  WPCSCPUTickCount = 0;
  WPCSCPUmissedIRQ = 0;
  WPCSCPUmissedFIRQ = 0;
  WPCSCPUirqCount = 0;
  WPCSCPUfirqCount = 0;
  WPCSCPUnmiCount = 0;

  WPCSCPUregA = 0;
  WPCSCPUregB = 0;
  WPCSCPUregX = 0;
  WPCSCPUregY = 0;
  WPCSCPUregU = 0;
  WPCSCPUregS = 0;
  WPCSCPUregCC = 0;
  WPCSCPUregPC = 0;
  WPCSCPUregDP = 0;  
  WPCSCPUirqPending = false;
  WPCSCPUfirqPending = false;
  WPCSCPUregDP = 0;

  WPCSCPUregCC = WPCSCPU_F_IRQMASK | WPCSCPU_F_FIRQMASK;
  WPCSCPUregPC = ReadWord(WPCSCPUvecRESET);
  WPCSCPUTickCount = 0;
}



uint16_t WPCSCPUSteps(uint16_t numTicks) {
  uint16_t preWPCSCPUTickCount = WPCSCPUTickCount;
  uint16_t ticksToRun = numTicks;
  uint8_t invalidStepDetected = 0;
  int16_t ticksRemaining = numTicks;
  while (ticksRemaining > 0) {
    uint16_t cycles = WPCSCPUStep();
    if (cycles < 1) {
      invalidStepDetected++;
      ticksRemaining--;
    }
    ticksRemaining -= cycles;
  }
  if (invalidStepDetected && ticksToRun > 1 && invalidStepDetected == ticksToRun) {
  }
  return WPCSCPUTickCount - preWPCSCPUTickCount;
}


void WPCSCPUIRQ() {
  WPCSCPUirqPending = true;
}

void WPCSCPUFIRQ(bool firqOn) {
  WPCSCPUfirqPending = firqOn;
}

void WPCSCPUNMI() {
  WPCSCPUnmiCount++;
  WPCSCPU_executeNmi();
}

uint16_t WPCSCPUGetPC() {
  return WPCSCPUregPC;
}


void WPCSCPUEnableLogging(bool loggingOn) {
  WPCSCPUWriteToLog = loggingOn;
}