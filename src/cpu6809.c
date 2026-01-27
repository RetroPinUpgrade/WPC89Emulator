#include "cpu6809.h"

void (*CPUMemoryWrite)(uint16_t, uint8_t);
uint8_t (*CPUMemoryRead)(uint16_t);
uint16_t CPUTickCount;

uint8_t CPUirqPending = false;
uint8_t CPUfirqPending = false;
uint8_t CPUmissedIRQ = 0;
uint8_t CPUmissedFIRQ = 0;

uint8_t CPUirqCount = 0;
uint8_t CPUfirqCount = 0;
uint8_t CPUnmiCount = 0;

uint8_t regA = 0;
uint8_t regB = 0;
uint16_t regX = 0;
uint16_t regY = 0;
uint16_t regU = 0;
uint16_t regS = 0;
uint8_t regCC = 0;
uint16_t regPC = 0;
uint8_t regDP = 0;

uint8_t F_CARRY = 1;
uint8_t F_OVERFLOW = 2;
uint8_t F_ZERO = 4;
uint8_t F_NEGATIVE = 8;
uint8_t F_IRQMASK = 16;
uint8_t F_HALFCARRY = 32;
uint8_t F_FIRQMASK = 64;
uint8_t F_ENTIRE = 128;

uint16_t vecRESET = 0xFFFE;
uint16_t vecFIRQ = 0xFFF6;
uint16_t vecIRQ = 0xFFF8;
uint16_t vecNMI = 0xFFFC;
uint16_t vecSWI = 0xFFFA;
uint16_t vecSWI2 = 0xFFF4;
uint16_t vecSWI3 = 0xFFF2;

uint8_t cycles[] = {
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

uint8_t cycles2[] = {
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

uint8_t flagsNZ[] = {
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


void _executeNmi();
void _executeFirq();
void _executeIrq();

void setV8(uint16_t a, uint16_t b, uint16_t r);
void setV16(uint32_t a, uint32_t b, uint32_t r);
uint16_t getD();
void setD(uint16_t v);
void PUSHB(uint8_t b);
void PUSHW(uint16_t b);
void PUSHBU(uint8_t b);
void PUSHWU(uint16_t b);
uint8_t PULLB();
uint16_t PULLW();
uint8_t PULLBU();
uint16_t PULLWU();
void PSHS(uint8_t ucTemp);
void PSHU(uint8_t ucTemp);
void PULS(uint8_t ucTemp);
void PULU(uint8_t ucTemp);
uint16_t getPostByteRegister(uint8_t ucPostByte);
void setPostByteRegister(uint8_t ucPostByte, uint16_t v);
void TFREXG(uint8_t ucPostByte, uint8_t bExchange);
int16_t signed5bit(uint8_t x);
int16_t signed8(uint16_t x);
int16_t signed16(uint16_t x);
uint8_t fetch();
uint16_t fetch16();
uint16_t ReadWord(uint16_t addr);
void WriteWord(uint16_t addr, uint16_t v);
uint16_t PostByte();
void flagsNZ16(uint16_t inputWord);
uint8_t oINC(uint8_t b);
uint8_t oDEC(uint8_t b);
uint8_t oSUB(uint8_t b, uint8_t v);
uint16_t oSUB16(uint16_t b, uint16_t v);
uint8_t oADD(uint8_t b, uint8_t v);
uint16_t oADD16(uint16_t b, uint16_t v);
uint8_t oADC(uint8_t b, uint8_t v);
uint8_t oSBC(uint8_t b, uint8_t v);
uint8_t oCMP(uint8_t b, uint8_t v);
uint16_t oCMP16(uint16_t b, uint16_t v);
uint8_t oNEG(uint8_t b);
uint8_t oLSR(uint8_t b);
uint8_t oASR(uint8_t b);
uint8_t oASL(uint8_t b);
uint8_t oROL(uint8_t b);
uint8_t oROR(uint8_t b);
uint8_t oEOR(uint8_t b, uint8_t v);
uint8_t oOR(uint8_t b, uint8_t v);
uint8_t oAND(uint8_t b, uint8_t v);
uint8_t oCOM(uint8_t b);
uint16_t dpadd();


bool CPUSetCallbacks(void (*memoryWriteFunction)(uint16_t, uint8_t), uint8_t (*memoryReadFunction)(uint16_t)) {
  CPUMemoryWrite = memoryWriteFunction;
  CPUMemoryRead = memoryReadFunction;

  CPUTickCount = 0;

  CPUirqPending = false;
  CPUfirqPending = false;
  CPUmissedIRQ = 0;
  CPUmissedFIRQ = 0;

  CPUirqCount = 0;
  CPUfirqCount = 0;
  CPUnmiCount = 0;

  regA = 0;
  regB = 0;
  regX = 0;
  regY = 0;
  regU = 0;
  regS = 0;
  regCC = 0;
  regPC = 0;
  regDP = 0;
  return true;
}


// set overflow flag
void setV8(uint16_t a, uint16_t b, uint16_t r) {
  regCC |= ((a ^ b ^ r ^ (r >> 1)) & 0x80) >> 6;
}

// set overflow flag
void setV16(uint32_t a, uint32_t b, uint32_t r) {
  regCC |= ((a ^ b ^ r ^ (r >> 1)) & 0x8000) >> 14;
}

uint16_t getD() {
  return ((uint16_t)regA << 8) + regB;
}

void setD(uint16_t v) {
  regA = (v >> 8) & 0xff;
  regB = v & 0xff;
}

void PUSHB(uint8_t b) {
  regS = (regS - 1) & 0xFFFF;
  CPUMemoryWrite(regS, b & 0xFF);
}

void PUSHW(uint16_t b) {
  regS = (regS - 1) & 0xFFFF;
  CPUMemoryWrite(regS, b & 0xFF);
  regS = (regS - 1) & 0xFFFF;
  CPUMemoryWrite(regS, (b >> 8) & 0xFF);
}

void PUSHBU(uint8_t b) {
  regU = (regU - 1) & 0xFFFF;
  CPUMemoryWrite(regU, b & 0xFF);
}

void PUSHWU(uint16_t b) {
  regU = (regU - 1) & 0xFFFF;
  CPUMemoryWrite(regU, b & 0xFF);
  regU = (regU - 1) & 0xFFFF;
  CPUMemoryWrite(regU, (b >> 8) & 0xFF);
}

uint8_t PULLB() {
  uint8_t tempuint8_t = CPUMemoryRead(regS);
  regS += 1;
  return tempuint8_t;
}

uint16_t PULLW() {
  uint16_t tempW = (CPUMemoryRead(regS)<<8) + CPUMemoryRead(regS+1);
  regS += 2;
  return tempW;
}

uint8_t PULLBU() {
  uint8_t tempuint8_t = CPUMemoryRead(regU);
  regU += 1;
  return tempuint8_t;
}

uint16_t PULLWU() {
  uint16_t tempW = (CPUMemoryRead(regU)<<8) + CPUMemoryRead(regU+1);
  regU += 2;
  return tempW;
}

//Push A, B, CC, DP, D, X, Y, U, or PC onto hardware stack
void PSHS(uint8_t ucTemp) {
  uint8_t i = 0;
  if (ucTemp & 0x80) {
    PUSHW(regPC);
    i += 2;
  }
  if (ucTemp & 0x40) {
    PUSHW(regU);
    i += 2;
  }
  if (ucTemp & 0x20) {
    PUSHW(regY);
    i += 2;
  }
  if (ucTemp & 0x10) {
    PUSHW(regX);
    i += 2;
  }
  if (ucTemp & 0x8) {
    PUSHB(regDP);
    i++;
  }
  if (ucTemp & 0x4) {
    PUSHB(regB);
    i++;
  }
  if (ucTemp & 0x2) {
    PUSHB(regA);
    i++;
  }
  if (ucTemp & 0x1) {
    PUSHB(regCC);
    i++;
  }
  CPUTickCount += i; //timing
}

//Push A, B, CC, DP, D, X, Y, S, or PC onto user stack
void PSHU(uint8_t ucTemp) {
  uint8_t i = 0;
  if (ucTemp & 0x80) {
    PUSHWU(regPC);
    i += 2;
  }
  if (ucTemp & 0x40) {
    PUSHWU(regS);
    i += 2;
  }
  if (ucTemp & 0x20) {
    PUSHWU(regY);
    i += 2;
  }
  if (ucTemp & 0x10) {
    PUSHWU(regX);
    i += 2;
  }
  if (ucTemp & 0x8) {
    PUSHBU(regDP);
    i++;
  }
  if (ucTemp & 0x4) {
    PUSHBU(regB);
    i++;
  }
  if (ucTemp & 0x2) {
    PUSHBU(regA);
    i++;
  }
  if (ucTemp & 0x1) {
    PUSHBU(regCC);
    i++;
  }
  CPUTickCount += i; //timing
}

//Pull A, B, CC, DP, D, X, Y, U, or PC from hardware stack
void PULS(uint8_t ucTemp) {
  uint8_t i = 0;
  if (ucTemp & 0x1) {
    regCC = PULLB();
    i++;
  }
  if (ucTemp & 0x2) {
    regA = PULLB();
    i++;
  }
  if (ucTemp & 0x4) {
    regB = PULLB();
    i++;
  }
  if (ucTemp & 0x8) {
    regDP = PULLB();
    i++;
  }
  if (ucTemp & 0x10) {
    regX = PULLW();
    i += 2;
  }
  if (ucTemp & 0x20) {
    regY = PULLW();
    i += 2;
  }
  if (ucTemp & 0x40) {
    regU = PULLW();
    i += 2;
  }
  if (ucTemp & 0x80) {
    regPC = PULLW();
    i += 2;
  }
  CPUTickCount += i; //timing
}

//Pull A, B, CC, DP, D, X, Y, S, or PC from hardware stack
void PULU(uint8_t ucTemp) {
  uint8_t i = 0;
  if (ucTemp & 0x1) {
    regCC = PULLBU();
    i++;
  }
  if (ucTemp & 0x2) {
    regA = PULLBU();
    i++;
  }
  if (ucTemp & 0x4) {
    regB = PULLBU();
    i++;
  }
  if (ucTemp & 0x8) {
    regDP = PULLBU();
    i++;
  }
  if (ucTemp & 0x10) {
    regX = PULLWU();
    i += 2;
  }
  if (ucTemp & 0x20) {
    regY = PULLWU();
    i += 2;
  }
  if (ucTemp & 0x40) {
    regS = PULLWU();
    i += 2;
  }
  if (ucTemp & 0x80) {
    regPC = PULLWU();
    i += 2;
  }
  CPUTickCount += i; //timing
}


void _executeNmi() {
  PUSHW(regPC);
  PUSHW(regU);
  PUSHW(regY);
  PUSHW(regX);
  PUSHB(regDP);
  PUSHB(regB);
  PUSHB(regA);
  regCC |= F_ENTIRE;
  PUSHB(regCC);
  regCC |= F_IRQMASK | F_FIRQMASK;
  regPC = ReadWord(vecNMI);
  CPUTickCount += 19;
}

void _executeFirq() {
  // Later games might call this from the high-performance timer
  regCC &= ~F_ENTIRE;
  PUSHW(regPC);
  PUSHB(regCC);

  regCC |= F_IRQMASK | F_FIRQMASK;
  regPC = ReadWord(vecFIRQ);
  CPUTickCount += 10;
}

void _executeIrq() {
  regCC |= F_ENTIRE;
  PUSHW(regPC);
  PUSHW(regU);
  PUSHW(regY);
  PUSHW(regX);
  PUSHB(regDP);
  PUSHB(regB);
  PUSHB(regA);
  PUSHB(regCC);

  regCC |= F_IRQMASK;
  regPC = ReadWord(vecIRQ);
  CPUTickCount += 19;
}

uint16_t getPostByteRegister(uint8_t ucPostByte) {
  switch (ucPostByte & 0xF) {
    case 0x00:
      return getD();
    case 0x1:
      return regX;
    case 0x2:
      return regY;
    case 0x3:
      return regU;
    case 0x4:
      return regS;
    case 0x5:
      return regPC;
    case 0x8:
      return regA;
    case 0x9:
      return regB;
    case 0xA:
      return regCC;
    case 0xB:
      return regDP;
    default:
      /* illegal */
      //throw new Error('getPBR_INVALID_' + ucPostByte);
      break;
  }
  return 0;
}

void setPostByteRegister(uint8_t ucPostByte, uint16_t v) {
  /* Get destination register */
  switch (ucPostByte & 0xF) {
    case 0x00:
      setD(v);
      return;
    case 0x1:
      regX = v;
      return;
    case 0x2:
      regY = v;
      return;
    case 0x3:
      regU = v;
      return;
    case 0x4:
      regS = v;
      return;
    case 0x5:
      regPC = v;
      return;
    case 0x8:
      regA = v & 0xFF;
      return;
    case 0x9:
      regB = v & 0xFF;
      return;
    case 0xA:
      regCC = v & 0xFF;
      return;
    case 0xB:
      regDP = v & 0xFF;
      return;
    default:
      /* illegal */
      //throw new Error('setPBR_INVALID_' + ucPostByte);
      break;
  }
}

// Transfer or exchange two registers.
void TFREXG(uint8_t ucPostByte, uint8_t bExchange) {
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

int16_t signed5bit(uint8_t x) {
  return x > 0xF ? x - 0x20 : x;
}

int16_t signed8(uint16_t x) {
  return x > 0x7F ? x - 0x100 : x;
}

int16_t signed16(uint16_t x) {
  return x > 0x7FFF ? x - 0x10000 : x;
}

uint8_t fetch() {
  uint8_t tempuint8_t = CPUMemoryRead(regPC);
  regPC += 1;
  return tempuint8_t;
}

uint16_t fetch16() {
  uint16_t v1 = CPUMemoryRead(regPC);
  uint16_t v2 = CPUMemoryRead(regPC+1);
  regPC += 2;
  return (v1 << 8) + v2;
}

uint16_t ReadWord(uint16_t addr) {
  uint16_t v1 = CPUMemoryRead(addr);
  uint16_t v2 = CPUMemoryRead((addr + 1) & 0xFFFF);
  return (v1 << 8) + v2;
}

void WriteWord(uint16_t addr, uint16_t v) {
  CPUMemoryWrite(addr, (v >> 8) & 0xff);
  CPUMemoryWrite((addr + 1) & 0xFFFF, v & 0xff);
}

//PURPOSE: Calculate the EA for INDEXING addressing mode.
//
// Offset sizes for postByte
// ±4-bit (-16 to +15)
// ±7-bit (-128 to +127)
// ±15-bit (-32768 to +32767)
uint16_t PostByte() {
  uint8_t INDIRECT_FIELD = 0x10;
  uint8_t REGISTER_FIELD = 0x60;
  uint8_t COMPLEXTYPE_FIELD = 0x80;
  uint8_t ADDRESSINGMODE_FIELD = 0x0F;

  uint8_t postByte = fetch();
  uint16_t registerField = 0;
  // Isolate register is used for the indexed operation
  // see Table 3-6. Indexed Addressing PostByte Register
  switch (postByte & REGISTER_FIELD) {
    case 0x00:
      registerField = regX;
      break;
    case 0x20:
      registerField = regY;
      break;
    case 0x40:
      registerField = regU;
      break;
    case 0x60:
      registerField = regS;
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
        EA = registerField;
        xchg = registerField + 1;
        CPUTickCount += 2;
        break;
      case 0x01: // R++
        EA = registerField;
        xchg = registerField + 2;
        CPUTickCount += 3;
        break;
      case 0x02: // -R
        xchg = registerField - 1;
        EA = xchg;
        CPUTickCount += 2;
        break;
      case 0x03: // --R
        xchg = registerField - 2;
        EA = xchg;
        CPUTickCount += 3;
        break;
      case 0x04: // EA = R + 0 OFFSET
        EA = registerField;
        break;
      case 0x05: // EA = R + REGB OFFSET
        EA = registerField + signed8(regB);
        CPUTickCount += 1;
        break;
      case 0x06: // EA = R + REGA OFFSET
        EA = registerField + signed8(regA);
        CPUTickCount += 1;
        break;
      // case 0x07 is ILLEGAL
      case 0x08: // EA = R + 7bit OFFSET
        EA = registerField + signed8(fetch());
        CPUTickCount += 1;
        break;
      case 0x09: // EA = R + 15bit OFFSET
        EA = registerField + signed16(fetch16());
        CPUTickCount += 4;
        break;
      // case 0x0A is ILLEGAL
      case 0x0B: // EA = R + D OFFSET
        EA = registerField + getD();
        CPUTickCount += 4;
        break;
      case 0x0C: { // EA = PC + 7bit OFFSET
        // NOTE: fetch increases regPC - so order is important!
        int16_t tempOffset = signed8(fetch());
        EA = regPC + tempOffset;
        CPUTickCount += 1;
        break;
      }
      case 0x0D: { // EA = PC + 15bit OFFSET
        // NOTE: fetch increases regPC - so order is important!
        int16_t word = signed16(fetch16());
        EA = regPC + word;
        CPUTickCount += 5;
        break;
      }
      // case 0xE is ILLEGAL
      case 0x0F: // EA = ADDRESS
        EA = fetch16();
        CPUTickCount += 5;
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
      CPUTickCount += 3;
    }
  } else {
    // Just a 5 bit signed offset + register, NO INDIRECT ADDRESS MODE
    int16_t suint8_t = signed5bit(postByte & 0x1F);
    EA = registerField + suint8_t;
    CPUTickCount += 1;
  }

  if (xchg != 0 ) {
    xchg &= 0xFFFF;
    switch (postByte & REGISTER_FIELD) {
      case 0:
        regX = xchg;
        break;
      case 0x20:
        regY = xchg;
        break;
      case 0x40:
        regU = xchg;
        break;
      case 0x60:
        regS = xchg;
        break;
      default:
        //throw new Error('PB_INVALID_XCHG_VALUE_' + postByte);
        break;
    }
  }
  // Return the effective address
  return EA & 0xFFFF;
}

void flagsNZ16(uint16_t inputWord) {
  regCC &= ~(F_ZERO | F_NEGATIVE);
  if (inputWord == 0) {
    regCC |= F_ZERO;
  }
  if (inputWord & 0x8000) {
    regCC |= F_NEGATIVE;
  }
}

// ============= Operations

uint8_t oINC(uint8_t b) {
  b = (b + 1) & 0xFF;
  regCC &= ~(F_ZERO | F_OVERFLOW | F_NEGATIVE);
  regCC |= flagsNZ[b];
  //Docs say:
  //V: Set if the original operand was 01111111
  if (b == 0x80) {
    regCC |= F_OVERFLOW;
  }
  return b;
}

uint8_t oDEC(uint8_t b) {
  b = (b - 1) & 0xFF;
  regCC &= ~(F_ZERO | F_OVERFLOW | F_NEGATIVE);
  regCC |= flagsNZ[b];
  //Docs say:
  //V: Set if the original operand was 10000000
  if (b == 0x7f) {
    regCC |= F_OVERFLOW;
  }
  return b;
}

uint8_t oSUB(uint8_t b, uint8_t v) {
  int16_t temp = b - v;
  regCC &= ~(F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  if (temp & 0x100) {
    regCC |= F_CARRY;
  }
  setV8(b, v, temp);
  temp &= 0xFF;
  regCC |= flagsNZ[temp];
  return temp;
}

uint16_t oSUB16(uint16_t b, uint16_t v) {
  uint32_t temp = b - v;
  regCC &= ~(F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  if (temp & 0x8000) {
    regCC |= F_NEGATIVE;
  }
  if (temp & 0x10000) {
    regCC |= F_CARRY;
  }
  setV16(b, v, temp);
  temp &= 0xFFFF;
  if (temp == 0) {
    regCC |= F_ZERO;
  }
  return temp;
}

uint8_t oADD(uint8_t b, uint8_t v) {
  int16_t temp = b + v;
  regCC &= ~(F_HALFCARRY | F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  if (temp & 0x100) {
    regCC |= F_CARRY;
  }
  setV8(b, v, temp);
  if ((temp ^ b ^ v) & 0x10) {
    regCC |= F_HALFCARRY;
  }
  temp &= 0xFF;
  regCC |= flagsNZ[temp];
  return temp;
}

uint16_t oADD16(uint16_t b, uint16_t v) {
  uint32_t temp = b + v;
  regCC &= ~(F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  if (temp & 0x8000) {
    regCC |= F_NEGATIVE;
  }
  if (temp & 0x10000) {
    regCC |= F_CARRY;
  }
  setV16(b, v, temp);
  temp &= 0xFFFF;
  if (temp == 0) {
    regCC |= F_ZERO;
  }
  return temp;
}

uint8_t oADC(uint8_t b, uint8_t v) {
  int16_t temp = b + v + (regCC & F_CARRY);
  regCC &= ~(F_HALFCARRY | F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  if (temp & 0x100) {
    regCC |= F_CARRY;
  }
  setV8(b, v, temp);
  if ((temp ^ b ^ v) & 0x10) {
    regCC |= F_HALFCARRY;
  }
  temp &= 0xFF;
  regCC |= flagsNZ[temp];
  return temp;
}

uint8_t oSBC(uint8_t b, uint8_t v) {
  int16_t temp = b - v - (regCC & F_CARRY);
  regCC &= ~(F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  if (temp & 0x100) {
    regCC |= F_CARRY;
  }
  setV8(b, v, temp);
  temp &= 0xFF;
  regCC |= flagsNZ[temp];
  return temp;
}

uint8_t oCMP(uint8_t b, uint8_t v) {
  int16_t temp = b - v;
  regCC &= ~(F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  if (temp & 0x100) {
    regCC |= F_CARRY;
  }
  setV8(b, v, temp);
  temp &= 0xFF;
  regCC |= flagsNZ[temp];
  return temp;
}

uint16_t oCMP16(uint16_t b, uint16_t v) {
  uint32_t temp = b - v;
  regCC &= ~(F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  if ((temp & 0xFFFF) == 0) {
    regCC |= F_ZERO;
  }
  if (temp & 0x8000) {
    regCC |= F_NEGATIVE;
  }
  if (temp & 0x10000) {
    regCC |= F_CARRY;
  }
  setV16(b, v, temp);
  return temp;
}

uint8_t oNEG(uint8_t b) {
  regCC &= ~(F_CARRY | F_ZERO | F_OVERFLOW | F_NEGATIVE);
  b = (0 - b) & 0xFF;
  if (b == 0x80) {
    regCC |= F_OVERFLOW;
  }
  if (b == 0) {
    regCC |= F_ZERO;
  }
  if (b & 0x80) {
    regCC |= F_NEGATIVE | F_CARRY;
  }
  return b;
}

uint8_t oLSR(uint8_t b) {
  regCC &= ~(F_ZERO | F_CARRY | F_NEGATIVE);
  if (b & F_CARRY) {
    regCC |= F_CARRY;
  }
  b >>= 1;
  if (b == 0) {
    regCC |= F_ZERO;
  }
  return b;
}

uint8_t oASR(uint8_t b) {
  regCC &= ~(F_ZERO | F_CARRY | F_NEGATIVE);
  if (b & 0x01) {
    regCC |= F_CARRY;
  }
  b = (b & 0x80) | (b >> 1);
  b &= 0xFF;
  regCC |= flagsNZ[b];
  return b;
}

uint8_t oASL(uint8_t b) {
  int16_t temp = b;
  regCC &= ~(F_ZERO | F_CARRY | F_NEGATIVE | F_OVERFLOW);
  if (b & 0x80) {
    regCC |= F_CARRY;
  }
  b <<= 1;
  if ((b ^ temp) & 0x80) {
    regCC |= F_OVERFLOW;
  }
  b &= 0xFF;
  regCC |= flagsNZ[b];
  return b;
}

uint8_t oROL(uint8_t b) {
  int16_t temp = b;
  uint8_t oldCarry = regCC & F_CARRY;
  regCC &= ~(F_ZERO | F_CARRY | F_NEGATIVE | F_OVERFLOW);
  if (b & 0x80) {
    regCC |= F_CARRY;
  }
  b = (b << 1) | oldCarry;
  if ((b ^ temp) & 0x80) {
    regCC |= F_OVERFLOW;
  }
  b &= 0xFF;
  regCC |= flagsNZ[b];
  return b;
}

uint8_t oROR(uint8_t b) {
  uint8_t oldCarry = regCC & F_CARRY;
  regCC &= ~(F_ZERO | F_CARRY | F_NEGATIVE);
  if (b & 0x01) {
    regCC |= F_CARRY;
  }
  b = (b >> 1) | (oldCarry << 7);
  b &= 0xFF;
  regCC |= flagsNZ[b];
  return b;
}

uint8_t oEOR(uint8_t b, uint8_t v) {
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  b ^= v;
  b &= 0xFF;
  regCC |= flagsNZ[b];
  return b;
}

uint8_t oOR(uint8_t b, uint8_t v) {
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  b |= v;
  b &= 0xFF;
  regCC |= flagsNZ[b];
  return b;
}

uint8_t oAND(uint8_t b, uint8_t v) {
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  b &= v;
  b &= 0xFF;
  regCC |= flagsNZ[b];
  return b;
}

uint8_t oCOM(uint8_t b) {
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  b ^= 0xFF;
  b &= 0xFF;
  regCC |= flagsNZ[b];
  regCC |= F_CARRY;
  return b;
}

//----common
uint16_t dpadd() {
  //direct page + 8bit index
  return (regDP << 8) + fetch();
}



typedef void (*OpcodeHandler)(void);

void opUnimplemented(void) {
}

void op_00(void) {
  //NEG DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oNEG(CPUMemoryRead(addr))
  );
}
void op_03(void) {
  //COM DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oCOM(CPUMemoryRead(addr))
  );
}
void op_04(void) {
  //LSR DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oLSR(CPUMemoryRead(addr))
  );
}
void op_06(void) {
  //ROR DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oROR(CPUMemoryRead(addr))
  );
}
void op_07(void) {
  //ASR DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oASR(CPUMemoryRead(addr))
  );
}
void op_08(void) {
  //ASL DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oASL(CPUMemoryRead(addr))
  );
}
void op_09(void) {
  //ROL DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oROL(CPUMemoryRead(addr))
  );
}
void op_0A(void) {
  //DEC DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oDEC(CPUMemoryRead(addr))
  );
}
void op_0C(void) {
  //INC DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(
    addr,
    oINC(CPUMemoryRead(addr))
  );
}
void op_0D(void) {
  //TST DP
  int32_t addr;
  uint16_t pb;
  addr = dpadd();
  pb = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[pb];
}
void op_0E(void) {
  //JMP DP
  int32_t addr;
  addr = dpadd();
  regPC = addr;
}
void op_0F(void) {
  //CLR DP
  int32_t addr;
  addr = dpadd();
  CPUMemoryWrite(addr, 0);
  regCC &= ~(F_CARRY | F_NEGATIVE | F_OVERFLOW);
  regCC |= F_ZERO;
}

// --- Page 1 (0x10) Handlers ---

void op_p1_21(void) { // BRN
    fetch16(); // Consume offset, do nothing
}
void op_p1_22(void) { // BHI
    int32_t addr = signed16(fetch16());
    if (!(regCC & (F_CARRY | F_ZERO))) { regPC += addr; CPUTickCount++; }
}
void op_p1_23(void) { // BLS
    int32_t addr = signed16(fetch16());
    if (regCC & (F_CARRY | F_ZERO)) { regPC += addr; CPUTickCount++; }
}
void op_p1_24(void) { // BCC
    int32_t addr = signed16(fetch16());
    if (!(regCC & F_CARRY)) { regPC += addr; CPUTickCount++; }
}
void op_p1_25(void) { // BCS
    int32_t addr = signed16(fetch16());
    if (regCC & F_CARRY) { regPC += addr; CPUTickCount++; }
}
void op_p1_26(void) { // BNE
    int32_t addr = signed16(fetch16());
    if (!(regCC & F_ZERO)) { regPC += addr; CPUTickCount++; }
}
void op_p1_27(void) { // LBEQ
    int32_t addr = signed16(fetch16());
    if (regCC & F_ZERO) { regPC += addr; CPUTickCount++; }
}
void op_p1_28(void) { // BVC
    int32_t addr = signed16(fetch16());
    if (!(regCC & F_OVERFLOW)) { regPC += addr; CPUTickCount++; }
}
void op_p1_29(void) { // BVS
    int32_t addr = signed16(fetch16());
    if (regCC & F_OVERFLOW) { regPC += addr; CPUTickCount++; }
}
void op_p1_2A(void) { // BPL
    int32_t addr = signed16(fetch16());
    if (!(regCC & F_NEGATIVE)) { regPC += addr; CPUTickCount++; }
}
void op_p1_2B(void) { // BMI
    int32_t addr = signed16(fetch16());
    if (regCC & F_NEGATIVE) { regPC += addr; CPUTickCount++; }
}
void op_p1_2C(void) { // BGE
    int32_t addr = signed16(fetch16());
    if (!((regCC & F_NEGATIVE) ^ ((regCC & F_OVERFLOW) << 2))) { regPC += addr; CPUTickCount++; }
}
void op_p1_2D(void) { // BLT
    int32_t addr = signed16(fetch16());
    if ((regCC & F_NEGATIVE) ^ ((regCC & F_OVERFLOW) << 2)) { regPC += addr; CPUTickCount++; }
}
void op_p1_2E(void) { // BGT
    int32_t addr = signed16(fetch16());
    if (!((regCC & F_NEGATIVE) ^ ((regCC & F_OVERFLOW) << 2) || regCC & F_ZERO)) { regPC += addr; CPUTickCount++; }
}
void op_p1_2F(void) { // BLE
    int32_t addr = signed16(fetch16());
    if ((regCC & F_NEGATIVE) ^ ((regCC & F_OVERFLOW) << 2) || regCC & F_ZERO) { regPC += addr; CPUTickCount++; }
}

void op_p1_3F(void) { // SWI2
    regCC |= F_ENTIRE;
    PUSHW(regPC); PUSHW(regU); PUSHW(regY); PUSHW(regX);
    PUSHB(regDP); PUSHB(regB); PUSHB(regA); PUSHB(regCC);
    regPC = ReadWord(vecSWI2);
}

// CMPD
void op_p1_83(void) { oCMP16(getD(), fetch16()); }
void op_p1_93(void) { oCMP16(getD(), ReadWord(dpadd())); }
void op_p1_A3(void) { oCMP16(getD(), ReadWord(PostByte())); }
void op_p1_B3(void) { oCMP16(getD(), ReadWord(fetch16())); }

// CMPY
void op_p1_8C(void) { oCMP16(regY, fetch16()); }
void op_p1_9C(void) { oCMP16(regY, ReadWord(dpadd())); }
void op_p1_AC(void) { oCMP16(regY, ReadWord(PostByte())); }
void op_p1_BC(void) { oCMP16(regY, ReadWord(fetch16())); }

// LDY
void op_p1_8E(void) { regY = fetch16(); flagsNZ16(regY); regCC &= ~F_OVERFLOW; }
void op_p1_9E(void) { regY = ReadWord(dpadd()); flagsNZ16(regY); regCC &= ~F_OVERFLOW; }
void op_p1_AE(void) { regY = ReadWord(PostByte()); flagsNZ16(regY); regCC &= ~F_OVERFLOW; }
void op_p1_BE(void) { regY = ReadWord(fetch16()); flagsNZ16(regY); regCC &= ~F_OVERFLOW; }

// STY
void op_p1_9F(void) { int32_t addr = dpadd(); WriteWord(addr, regY); flagsNZ16(regY); regCC &= ~F_OVERFLOW; }
void op_p1_AF(void) { int32_t addr = PostByte(); WriteWord(addr, regY); flagsNZ16(regY); regCC &= ~F_OVERFLOW; }
void op_p1_BF(void) { int32_t addr = fetch16(); WriteWord(addr, regY); flagsNZ16(regY); regCC &= ~F_OVERFLOW; }

// LDS
void op_p1_CE(void) { regS = fetch16(); flagsNZ16(regS); regCC &= ~F_OVERFLOW; }
void op_p1_DE(void) { regS = ReadWord(dpadd()); flagsNZ16(regS); regCC &= ~F_OVERFLOW; }
void op_p1_EE(void) { regS = ReadWord(PostByte()); flagsNZ16(regS); regCC &= ~F_OVERFLOW; }
void op_p1_FE(void) { regS = ReadWord(fetch16()); flagsNZ16(regS); regCC &= ~F_OVERFLOW; }

// STS
void op_p1_DF(void) { int32_t addr = dpadd(); WriteWord(addr, regS); flagsNZ16(regS); regCC &= ~F_OVERFLOW; }
void op_p1_EF(void) { int32_t addr = PostByte(); WriteWord(addr, regS); flagsNZ16(regS); regCC &= ~F_OVERFLOW; }
void op_p1_FF(void) { int32_t addr = fetch16(); WriteWord(addr, regS); flagsNZ16(regS); regCC &= ~F_OVERFLOW; }

// --- Page 2 (0x11) Handlers ---

void op_p2_3F(void) { // SWI3
    regCC |= F_ENTIRE;
    PUSHW(regPC); PUSHW(regU); PUSHW(regY); PUSHW(regX);
    PUSHB(regDP); PUSHB(regB); PUSHB(regA); PUSHB(regCC);
    regPC = ReadWord(vecSWI3);
}

// CMPU
void op_p2_83(void) { oCMP16(regU, fetch16()); }
void op_p2_93(void) { oCMP16(regU, ReadWord(dpadd())); }
void op_p2_A3(void) { oCMP16(regU, ReadWord(PostByte())); }
void op_p2_B3(void) { oCMP16(regU, ReadWord(fetch16())); }

// CMPS
void op_p2_8C(void) { oCMP16(regS, fetch16()); }
void op_p2_9C(void) { oCMP16(regS, ReadWord(dpadd())); }
void op_p2_AC(void) { oCMP16(regS, ReadWord(PostByte())); }
void op_p2_BC(void) { oCMP16(regS, ReadWord(fetch16())); }

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


void op_10(void) {
  // Page 1
  uint8_t opcode = fetch();
  CPUTickCount += cycles2[opcode];
  
  if (oDispatchTablePage1[opcode]) {
      oDispatchTablePage1[opcode]();
  } else {
      // Optional: Handle invalid Page 1 opcode
  }
}

void op_11(void) {
  // Page 2
  uint8_t opcode = fetch();
  CPUTickCount += cycles2[opcode];

  if (oDispatchTablePage2[opcode]) {
      oDispatchTablePage2[opcode]();
  } else {
      // Optional: Handle invalid Page 2 opcode
  }
}

void op_12(void) {
  //NOP
}

void op_13(void) {
  //SYNC
  /*
  This commands stops the CPU, brings the processor bus to high impedance state and waits for an interrupt.
  */
  //console.log('SYNC is broken!');
}

void op_16(void) {
  //LBRA relative
  int32_t addr = fetch16();
  regPC += addr;
}

void op_17(void) {
  //LBSR relative
  int32_t addr = fetch16();
  PUSHW(regPC);
  regPC += addr;
}

void op_19(void) {
  //DAA
  uint8_t correctionFactor = 0;
  uint8_t nhi = regA & 0xF0;
  uint8_t nlo = regA & 0x0F;
  int32_t addr;
  
  if (nlo > 0x09 || regCC & F_HALFCARRY) {
    correctionFactor |= 0x06;
  }
  if (nhi > 0x80 && nlo > 0x09) {
    correctionFactor |= 0x60;
  }
  if (nhi > 0x90 || regCC & F_CARRY) {
    correctionFactor |= 0x60;
  }
  addr = correctionFactor + regA;
  // TODO Check, mame does not clear carry here
  regCC &= ~(F_CARRY | F_NEGATIVE | F_ZERO | F_OVERFLOW);
  if (addr & 0x100) {
    regCC |= F_CARRY;
  }
  regA = addr & 0xFF;
  regCC |= flagsNZ[regA];
}

void op_1A(void) {
  //ORCC
  regCC |= fetch();
}

void op_1C(void) {
  //ANDCC
  regCC &= fetch();
}

void op_1D(void) {
  //SEX
  //TODO should we use signed here?
  regA = regB & 0x80 ? 0xff : 0;
  flagsNZ16(getD());
}

void op_1E(void) {
  //EXG
  uint16_t pb = fetch();
  TFREXG(pb, true);
}

void op_1F(void) {
  //TFR
  uint16_t pb = fetch();
  TFREXG(pb, false);
}

void op_20(void) {
  //BRA
  int32_t addr = signed8(fetch());
  regPC += addr;
}

void op_21(void) {
  //BRN
  fetch();
}

void op_22(void) {
  //BHI
  int32_t addr = signed8(fetch());
  if (!(regCC & (F_CARRY | F_ZERO))) {
    regPC += addr;
  }
}

void op_23(void) {
  //BLS
  int32_t addr = signed8(fetch());
  if (regCC & (F_CARRY | F_ZERO)) {
    regPC += addr;
  }
}

void op_24(void) {
  //BCC
  int32_t addr = signed8(fetch());
  if (!(regCC & F_CARRY)) {
    regPC += addr;
  }
}

void op_25(void) {
  //BCS
  int32_t addr = signed8(fetch());
  if (regCC & F_CARRY) {
    regPC += addr;
  }
}

void op_26(void) {
  //BNE
  int32_t addr = signed8(fetch());
  if (!(regCC & F_ZERO)) {
    regPC += addr;
  }
}

void op_27(void) {
  //BEQ
  int32_t addr = signed8(fetch());
  if (regCC & F_ZERO) {
    regPC += addr;
  }
}

void op_28(void) {
  //BVC
  int32_t addr = signed8(fetch());
  if (!(regCC & F_OVERFLOW)) {
    regPC += addr;
  }
}

void op_29(void) {
  //BVS
  int32_t addr = signed8(fetch());
  if (regCC & F_OVERFLOW) {
    regPC += addr;
  }
}

void op_2A(void) {
  //BPL
  int32_t addr = signed8(fetch());
  if (!(regCC & F_NEGATIVE)) {
    regPC += addr;
  }
}

void op_2B(void) {
  //BMI
  int32_t addr = signed8(fetch());
  if (regCC & F_NEGATIVE) {
    regPC += addr;
  }
}

void op_2C(void) {
  //BGE
  int32_t addr = signed8(fetch());
  if (!((regCC & F_NEGATIVE) ^ ((regCC & F_OVERFLOW) << 2))) {
    regPC += addr;
  }
}

void op_2D(void) {
  //BLT
  int32_t addr = signed8(fetch());
  if ((regCC & F_NEGATIVE) ^ ((regCC & F_OVERFLOW) << 2)) {
    regPC += addr;
  }
}

void op_2E(void) {
  //BGT
  int32_t addr = signed8(fetch());
  if (
    !((regCC & F_NEGATIVE) ^ ((regCC & F_OVERFLOW) << 2) ||
      regCC & F_ZERO)
  ) {
    regPC += addr;
  }
}

void op_2F(void) {
  //BLE
  int32_t addr = signed8(fetch());
  if (
    (regCC & F_NEGATIVE) ^ ((regCC & F_OVERFLOW) << 2) ||
    regCC & F_ZERO
  ) {
    regPC += addr;
  }
}

void op_30(void) {
  //LEAX
  regX = PostByte();
  regCC &= ~F_ZERO;
  if (regX == 0) {
    regCC |= F_ZERO;
  }
}

void op_31(void) {
  //LEAY
  regY = PostByte();
  regCC &= ~F_ZERO;
  if (regY == 0) {
    regCC |= F_ZERO;
  }
}

void op_32(void) {
  //LEAS
  regS = PostByte();
}

void op_33(void) {
  //LEAU
  regU = PostByte();
}

void op_34(void) {
  //PSHS
  PSHS(fetch());
}

void op_35(void) {
  //PULS
  PULS(fetch());
}

void op_36(void) {
  //PSHU
  PSHU(fetch());
}

void op_37(void) {
  //PULU
  PULU(fetch());
}

void op_39(void) {
  //RTS
  regPC = PULLW();
}

void op_3A(void) {
  //ABX
  regX += regB;
}

void op_3B(void) {
  //RTI
  regCC = PULLB();
  //debug('RTI', regCC & F_ENTIRE, CPUTickCount);
  // Check for fast interrupt
  if (regCC & F_ENTIRE) {
    CPUTickCount += 9;
    regA = PULLB();
    regB = PULLB();
    regDP = PULLB();
    regX = PULLW();
    regY = PULLW();
    regU = PULLW();
  }
  regPC = PULLW();
}

void op_3C(void) {
  //CWAI
  //console.log('CWAI is broken!');
  /*
   * CWAI stacks the entire machine state on the hardware stack,
   * then waits for an interrupt; when the interrupt is taken
   * later, the state is *not* saved again after CWAI.
   * see mame-6809.c how to proper implement this opcode
   */
  regCC &= fetch();
  //TODO - ??? set cwai flag to true, do not exec next interrupt (NMI, FIRQ, IRQ) - but set reset cwai flag afterwards
}

void op_3D(void) {
  //MUL
  int32_t addr = regA * regB;
  if (addr == 0) {
    regCC |= F_ZERO;
  } else {
    regCC &= ~F_ZERO;
  }
  if (addr & 0x80) {
    regCC |= F_CARRY;
  } else {
    regCC &= ~F_CARRY;
  }
  setD(addr);
}

void op_3F(void) {
  //SWI
  //console.log('SWI is untested!');
  regCC |= F_ENTIRE;
  PUSHW(regPC);
  PUSHW(regU);
  PUSHW(regY);
  PUSHW(regX);
  PUSHB(regDP);
  PUSHB(regB);
  PUSHB(regA);
  PUSHB(regCC);
  regCC |= F_IRQMASK | F_FIRQMASK;
  regPC = ReadWord(vecSWI);
}

void op_40(void) {
  regA = oNEG(regA);
}

void op_43(void) {
  regA = oCOM(regA);
}

void op_44(void) {
  regA = oLSR(regA);
}

void op_46(void) {
  regA = oROR(regA);
}

void op_47(void) {
  regA = oASR(regA);
}

void op_48(void) {
  regA = oASL(regA);
}

void op_49(void) {
  regA = oROL(regA);
}

void op_4A(void) {
  regA = oDEC(regA);
}

void op_4C(void) {
  regA = oINC(regA);
}

void op_4D(void) {
  // tsta
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regA & 0xFF];
}

void op_4F(void) {
  /* CLRA */
  regA = 0;
  regCC &= ~(F_NEGATIVE | F_OVERFLOW | F_CARRY);
  regCC |= F_ZERO;
}

void op_50(void) {
  /* NEGB */
  regB = oNEG(regB);
}

void op_53(void) {
  regB = oCOM(regB);
}

void op_54(void) {
  regB = oLSR(regB);
}

void op_56(void) {
  regB = oROR(regB);
}

void op_57(void) {
  regB = oASR(regB);
}

void op_58(void) {
  regB = oASL(regB);
}

void op_59(void) {
  regB = oROL(regB);
}

void op_5A(void) {
  regB = oDEC(regB);
}

void op_5C(void) {
  // INCB
  regB = oINC(regB);
}

void op_5D(void) {
  /* TSTB */
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regB & 0xFF];
}

void op_5F(void) {
  //CLRB
  regB = 0;
  regCC &= ~(F_NEGATIVE | F_OVERFLOW | F_CARRY);
  regCC |= F_ZERO;
}

void op_60(void) {
  //NEG indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oNEG(CPUMemoryRead(addr))
  );
}

void op_63(void) {
  //COM indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oCOM(CPUMemoryRead(addr))
  );
}

void op_64(void) {
  //LSR indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oLSR(CPUMemoryRead(addr))
  );
}

void op_66(void) {
  //ROR indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oROR(CPUMemoryRead(addr))
  );
}

void op_67(void) {
  //ASR indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oASR(CPUMemoryRead(addr))
  );
}

void op_68(void) {
  //ASL indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oASL(CPUMemoryRead(addr))
  );
}

void op_69(void) {
  //ROL indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oROL(CPUMemoryRead(addr))
  );
}

void op_6A(void) {
  //DEC indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oDEC(CPUMemoryRead(addr))
  );
}

void op_6C(void) {
  //INC indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(
    addr,
    oINC(CPUMemoryRead(addr))
  );
}

void op_6D(void) {
  //TST indexed
  int32_t addr = PostByte();
  uint16_t pb = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[pb];
}

void op_6E(void) {
  //JMP indexed
  int32_t addr = PostByte();
  regPC = addr;
}

void op_6F(void) {
  //CLR indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(addr, 0);
  regCC &= ~(F_CARRY | F_NEGATIVE | F_OVERFLOW);
  regCC |= F_ZERO;
}

void op_70(void) {
  //NEG extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oNEG(CPUMemoryRead(addr))
  );
}

void op_73(void) {
  //COM extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oCOM(CPUMemoryRead(addr))
  );
}

void op_74(void) {
  //LSR extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oLSR(CPUMemoryRead(addr))
  );
}

void op_76(void) {
  //ROR extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oROR(CPUMemoryRead(addr))
  );
}

void op_77(void) {
  //ASR extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oASR(CPUMemoryRead(addr))
  );
}

void op_78(void) {
  //ASL extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oASL(CPUMemoryRead(addr))
  );
}

void op_79(void) {
  //ROL extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oROL(CPUMemoryRead(addr))
  );
}

void op_7A(void) {
  //DEC extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oDEC(CPUMemoryRead(addr))
  );
}

void op_7C(void) {
  //INC extended
  int32_t addr = fetch16();
  CPUMemoryWrite(
    addr,
    oINC(CPUMemoryRead(addr))
  );
}

void op_7D(void) {
  //TST extended
  int32_t addr = fetch16();
  uint16_t pb = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[pb];
}

void op_7E(void) {
  //JMP extended
  int32_t addr = fetch16();
  regPC = addr;
}

void op_7F(void) {
  //CLR extended
  int32_t addr = fetch16();
  CPUMemoryWrite(addr, 0);
  regCC &= ~(F_CARRY | F_NEGATIVE | F_OVERFLOW);
  regCC |= F_ZERO;
}

void op_80(void) {
  //SUBA imm
  regA = oSUB(regA, fetch());
}

void op_81(void) {
  //CMPA imm
  oCMP(regA, fetch());
}

void op_82(void) {
  //SBCA imm
  regA = oSBC(regA, fetch());
}

void op_83(void) {
  //SUBD imm
  setD(oSUB16(getD(), fetch16()));
}

void op_84(void) {
  //ANDA imm
  regA = oAND(regA, fetch());
}

void op_85(void) {
  //BITA imm
  oAND(regA, fetch());
}

void op_86(void) {
  //LDA imm
  regA = fetch();
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regA & 0xFF];
}

void op_88(void) {
  //EORA imm
  regA = oEOR(regA, fetch());
}

void op_89(void) {
  //ADCA imm
  regA = oADC(regA, fetch());
}

void op_8A(void) {
  //ORA imm
  regA = oOR(regA, fetch());
}

void op_8B(void) {
  //ADDA imm
  regA = oADD(regA, fetch());
}

void op_8C(void) {
  //CMPX imm
  oCMP16(regX, fetch16());
}

void op_8D(void) {
  //JSR imm
  int32_t addr = signed8(fetch());
  PUSHW(regPC);
  regPC += addr;
}

void op_8E(void) {
  //LDX imm
  regX = fetch16();
  flagsNZ16(regX);
  regCC &= ~F_OVERFLOW;
}

void op_90(void) {
  //SUBA direct
  int32_t addr = dpadd();
  regA = oSUB(regA, CPUMemoryRead(addr));
}

void op_91(void) {
  //CMPA direct
  int32_t addr = dpadd();
  oCMP(regA, CPUMemoryRead(addr));
}

void op_92(void) {
  //SBCA direct
  int32_t addr = dpadd();
  regA = oSBC(regA, CPUMemoryRead(addr));
}

void op_93(void) {
  //SUBD direct
  int32_t addr = dpadd();
  setD(oSUB16(getD(), ReadWord(addr)));
}

void op_94(void) {
  //ANDA direct
  int32_t addr = dpadd();
  regA = oAND(regA, CPUMemoryRead(addr));
}

void op_95(void) {
  //BITA direct
  int32_t addr = dpadd();
  oAND(regA, CPUMemoryRead(addr));
}

void op_96(void) {
  //LDA direct
  int32_t addr = dpadd();
  regA = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regA & 0xFF];
}

void op_97(void) {
  //STA direct
  int32_t addr = dpadd();
  CPUMemoryWrite(addr, regA);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regA & 0xFF];
}

void op_98(void) {
  //EORA direct
  int32_t addr = dpadd();
  regA = oEOR(regA, CPUMemoryRead(addr));
}

void op_99(void) {
  //ADCA direct
  int32_t addr = dpadd();
  regA = oADC(regA, CPUMemoryRead(addr));
}

void op_9A(void) {
  //ORA direct
  int32_t addr = dpadd();
  regA = oOR(regA, CPUMemoryRead(addr));
}

void op_9B(void) {
  //ADDA direct
  int32_t addr = dpadd();
  regA = oADD(regA, CPUMemoryRead(addr));
}

void op_9C(void) {
  //CMPX direct
  int32_t addr = dpadd();
  oCMP16(regX, ReadWord(addr));
}

void op_9D(void) {
  //JSR direct
  int32_t addr = dpadd();
  PUSHW(regPC);
  regPC = addr;
}

void op_9E(void) {
  //LDX direct
  int32_t addr = dpadd();
  regX = ReadWord(addr);
  flagsNZ16(regX);
  regCC &= ~(F_OVERFLOW);
}

void op_9F(void) {
  //STX direct
  int32_t addr = dpadd();
  WriteWord(addr, regX);
  flagsNZ16(regX);
  regCC &= ~(F_OVERFLOW);
}

void op_A0(void) {
  //SUBA indexed
  int32_t addr = PostByte();
  regA = oSUB(regA, CPUMemoryRead(addr));
}

void op_A1(void) {
  //CMPA indexed
  int32_t addr = PostByte();
  oCMP(regA, CPUMemoryRead(addr));
}

void op_A2(void) {
  //SBCA indexed
  int32_t addr = PostByte();
  regA = oSBC(regA, CPUMemoryRead(addr));
}

void op_A3(void) {
  //SUBD indexed
  int32_t addr = PostByte();
  setD(oSUB16(getD(), ReadWord(addr)));
}

void op_A4(void) {
  //ANDA indexed
  int32_t addr = PostByte();
  regA = oAND(regA, CPUMemoryRead(addr));
}

void op_A5(void) {
  //BITA indexed
  int32_t addr = PostByte();
  oAND(regA, CPUMemoryRead(addr));
}

void op_A6(void) {
  //LDA indexed
  int32_t addr = PostByte();
  regA = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regA & 0xFF];
}

void op_A7(void) {
  //STA indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(addr, regA);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regA & 0xFF];
}

void op_A8(void) {
  //EORA indexed
  int32_t addr = PostByte();
  regA = oEOR(regA, CPUMemoryRead(addr));
}

void op_A9(void) {
  //ADCA indexed
  int32_t addr = PostByte();
  regA = oADC(regA, CPUMemoryRead(addr));
}

void op_AA(void) {
  //ORA indexed
  int32_t addr = PostByte();
  regA = oOR(regA, CPUMemoryRead(addr));
}

void op_AB(void) {
  //ADDA indexed
  int32_t addr = PostByte();
  regA = oADD(regA, CPUMemoryRead(addr));
}

void op_AC(void) {
  //CMPX indexed
  int32_t addr = PostByte();
  oCMP16(regX, ReadWord(addr));
}

void op_AD(void) {
  //JSR indexed
  int32_t addr = PostByte();
  PUSHW(regPC);
  regPC = addr;
}

void op_AE(void) {
  //LDX indexed
  int32_t addr = PostByte();
  regX = ReadWord(addr);
  flagsNZ16(regX);
  regCC &= ~F_OVERFLOW;
}

void op_AF(void) {
  //STX indexed
  int32_t addr = PostByte();
  WriteWord(addr, regX);
  flagsNZ16(regX);
  regCC &= ~F_OVERFLOW;
}

void op_B0(void) {
  //SUBA extended
  int32_t addr = fetch16();
  regA = oSUB(regA, CPUMemoryRead(addr));
}

void op_B1(void) {
  //CMPA extended
  int32_t addr = fetch16();
  oCMP(regA, CPUMemoryRead(addr));
}

void op_B2(void) {
  //SBCA extended
  int32_t addr = fetch16();
  regA = oSBC(regA, CPUMemoryRead(addr));
}

void op_B3(void) {
  //SUBD extended
  int32_t addr = fetch16();
  setD(oSUB16(getD(), ReadWord(addr)));
}

void op_B4(void) {
  //ANDA extended
  int32_t addr = fetch16();
  regA = oAND(regA, CPUMemoryRead(addr));
}

void op_B5(void) {
  //BITA extended
  int32_t addr = fetch16();
  oAND(regA, CPUMemoryRead(addr));
}

void op_B6(void) {
  //LDA extended
  int32_t addr = fetch16();
  regA = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regA & 0xFF];
}

void op_B7(void) {
  //STA extended
  int32_t addr = fetch16();
  CPUMemoryWrite(addr, regA);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regA & 0xFF];
}

void op_B8(void) {
  //EORA extended
  int32_t addr = fetch16();
  regA = oEOR(regA, CPUMemoryRead(addr));
}

void op_B9(void) {
  //ADCA extended
  int32_t addr = fetch16();
  regA = oADC(regA, CPUMemoryRead(addr));
}

void op_BA(void) {
  //ORA extended
  int32_t addr = fetch16();
  regA = oOR(regA, CPUMemoryRead(addr));
}

void op_BB(void) {
  //ADDA extended
  int32_t addr = fetch16();
  regA = oADD(regA, CPUMemoryRead(addr));
}

void op_BC(void) {
  //CMPX extended
  int32_t addr = fetch16();
  oCMP16(regX, ReadWord(addr));
}

void op_BD(void) {
  //JSR extended
  int32_t addr = fetch16();
  PUSHW(regPC);
  regPC = addr;
}

void op_BE(void) {
  //LDX extended
  int32_t addr = fetch16();
  regX = ReadWord(addr);
  flagsNZ16(regX);
  regCC &= ~F_OVERFLOW;
}

void op_BF(void) {
  //STX extended
  int32_t addr = fetch16();
  WriteWord(addr, regX);
  flagsNZ16(regX);
  regCC &= ~F_OVERFLOW;
}

void op_C0(void) {
  //SUBB imm
  regB = oSUB(regB, fetch());
}

void op_C1(void) {
  //CMPB imm
  oCMP(regB, fetch());
}

void op_C2(void) {
  //SBCB imm
  regB = oSBC(regB, fetch());
}

void op_C3(void) {
  //ADDD imm
  setD(oADD16(getD(), fetch16()));
}

void op_C4(void) {
  //ANDB imm
  regB = oAND(regB, fetch());
}

void op_C5(void) {
  //BITB imm
  oAND(regB, fetch());
}

void op_C6(void) {
  //LDB imm
  regB = fetch();
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regB & 0xFF];
}

void op_C8(void) {
  //EORB imm
  regB = oEOR(regB, fetch());
}

void op_C9(void) {
  //ADCB imm
  regB = oADC(regB, fetch());
}

void op_CA(void) {
  //ORB imm
  regB = oOR(regB, fetch());
}

void op_CB(void) {
  //ADDB imm
  regB = oADD(regB, fetch());
}

void op_CC(void) {
  //LDD imm
  int32_t addr = fetch16();
  setD(addr);
  flagsNZ16(addr);
  regCC &= ~F_OVERFLOW;
}

void op_CE(void) {
  //LDU imm
  regU = fetch16();
  flagsNZ16(regU);
  regCC &= ~F_OVERFLOW;
}

void op_D0(void) {
  //SUBB direct
  int32_t addr = dpadd();
  regB = oSUB(regB, CPUMemoryRead(addr));
}

void op_D1(void) {
  //CMPB direct
  int32_t addr = dpadd();
  oCMP(regB, CPUMemoryRead(addr));
}

void op_D2(void) {
  //SBCB direct
  int32_t addr = dpadd();
  regB = oSBC(regB, CPUMemoryRead(addr));
}

void op_D3(void) {
  //ADDD direct
  int32_t addr = dpadd();
  setD(oADD16(getD(), ReadWord(addr)));
}

void op_D4(void) {
  //ANDB direct
  int32_t addr = dpadd();
  regB = oAND(regB, CPUMemoryRead(addr));
}

void op_D5(void) {
  //BITB direct
  int32_t addr = dpadd();
  oAND(regB, CPUMemoryRead(addr));
}

void op_D6(void) {
  //LDB direct
  int32_t addr = dpadd();
  regB = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regB & 0xFF];
}

void op_D7(void) {
  //STB direct
  int32_t addr = dpadd();
  CPUMemoryWrite(addr, regB);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regB & 0xFF];
}

void op_D8(void) {
  //EORB direct
  int32_t addr = dpadd();
  regB = oEOR(regB, CPUMemoryRead(addr));
}

void op_D9(void) {
  //ADCB direct
  int32_t addr = dpadd();
  regB = oADC(regB, CPUMemoryRead(addr));
}

void op_DA(void) {
  //ORB direct
  int32_t addr = dpadd();
  regB = oOR(regB, CPUMemoryRead(addr));
}

void op_DB(void) {
  //ADDB direct
  int32_t addr = dpadd();
  regB = oADD(regB, CPUMemoryRead(addr));
}

void op_DC(void) {
  //LDD direct
  int32_t addr = dpadd();
  uint16_t pb = ReadWord(addr);
  setD(pb);
  flagsNZ16(pb);
  regCC &= ~F_OVERFLOW;
}

void op_DD(void) {
  //STD direct
  int32_t addr = dpadd();
  WriteWord(addr, getD());
  flagsNZ16(getD());
  regCC &= ~F_OVERFLOW;
}

void op_DE(void) {
  //LDU direct
  int32_t addr = dpadd();
  regU = ReadWord(addr);
  flagsNZ16(regU);
  regCC &= ~F_OVERFLOW;
}

void op_DF(void) {
  //STU direct
  int32_t addr = dpadd();
  WriteWord(addr, regU);
  flagsNZ16(regU);
  regCC &= ~F_OVERFLOW;
}

void op_E0(void) {
  //SUBB indexed
  int32_t addr = PostByte();
  regB = oSUB(regB, CPUMemoryRead(addr));
}

void op_E1(void) {
  //CMPB indexed
  int32_t addr = PostByte();
  oCMP(regB, CPUMemoryRead(addr));
}

void op_E2(void) {
  //SBCB indexed
  int32_t addr = PostByte();
  regB = oSBC(regB, CPUMemoryRead(addr));
}

void op_E3(void) {
  //ADDD indexed
  int32_t addr = PostByte();
  setD(oADD16(getD(), ReadWord(addr)));
}

void op_E4(void) {
  //ANDB indexed
  int32_t addr = PostByte();
  regB = oAND(regB, CPUMemoryRead(addr));
}

void op_E5(void) {
  //BITB indexed
  int32_t addr = PostByte();
  oAND(regB, CPUMemoryRead(addr));
}

void op_E6(void) {
  //LDB indexed
  int32_t addr = PostByte();
  regB = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regB & 0xFF];
}

void op_E7(void) {
  //STB indexed
  int32_t addr = PostByte();
  CPUMemoryWrite(addr, regB);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regB & 0xFF];
}

void op_E8(void) {
  //EORB indexed
  int32_t addr = PostByte();
  regB = oEOR(regB, CPUMemoryRead(addr));
}

void op_E9(void) {
  //ADCB indexed
  int32_t addr = PostByte();
  regB = oADC(regB, CPUMemoryRead(addr));
}

void op_EA(void) {
  //ORB indexed
  int32_t addr = PostByte();
  regB = oOR(regB, CPUMemoryRead(addr));
}

void op_EB(void) {
  //ADDB indexed
  int32_t addr = PostByte();
  regB = oADD(regB, CPUMemoryRead(addr));
}

void op_EC(void) {
  //LDD indexed
  int32_t addr = PostByte();
  uint16_t pb = ReadWord(addr);
  setD(pb);
  flagsNZ16(pb);
  regCC &= ~F_OVERFLOW;
}

void op_ED(void) {
  //STD indexed
  int32_t addr = PostByte();
  WriteWord(addr, getD());
  flagsNZ16(getD());
  regCC &= ~(F_OVERFLOW);
}

void op_EE(void) {
  //LDU indexed
  int32_t addr = PostByte();
  regU = ReadWord(addr);
  flagsNZ16(regU);
  regCC &= ~(F_OVERFLOW);
}

void op_EF(void) {
  //STU indexed
  int32_t addr = PostByte();
  WriteWord(addr, regU);
  flagsNZ16(regU);
  regCC &= ~(F_OVERFLOW);
}

void op_F0(void) {
  //SUBB extended
  int32_t addr = fetch16();
  regB = oSUB(regB, CPUMemoryRead(addr));
}

void op_F1(void) {
  //CMPB extended
  int32_t addr = fetch16();
  oCMP(regB, CPUMemoryRead(addr));
}

void op_F2(void) {
  //SBCB extended
  int32_t addr = fetch16();
  regB = oSBC(regB, CPUMemoryRead(addr));
}

void op_F3(void) {
  //ADDD extended
  int32_t addr = fetch16();
  setD(oADD16(getD(), ReadWord(addr)));
}

void op_F4(void) {
  //ANDB extended
  int32_t addr = fetch16();
  regB = oAND(regB, CPUMemoryRead(addr));
}

void op_F5(void) {
  //BITB extended
  int32_t addr = fetch16();
  oAND(regB, CPUMemoryRead(addr));
}

void op_F6(void) {
  //LDB extended
  int32_t addr = fetch16();
  regB = CPUMemoryRead(addr);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regB & 0xFF];
}

void op_F7(void) {
  //STB extended
  int32_t addr = fetch16();
  CPUMemoryWrite(addr, regB);
  regCC &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW);
  regCC |= flagsNZ[regB & 0xFF];
}

void op_F8(void) {
  //EORB extended
  int32_t addr = fetch16();
  regB = oEOR(regB, CPUMemoryRead(addr));
}

void op_F9(void) {
  //ADCB extended
  int32_t addr = fetch16();
  regB = oADC(regB, CPUMemoryRead(addr));
}

void op_FA(void) {
  //ORB extended
  int32_t addr = fetch16();
  regB = oOR(regB, CPUMemoryRead(addr));
}

void op_FB(void) {
  //ADDB extended
  int32_t addr = fetch16();
  regB = oADD(regB, CPUMemoryRead(addr));
}

void op_FC(void) {
  //LDD extended
  int32_t addr = fetch16();
  uint16_t pb = ReadWord(addr);
  setD(pb);
  flagsNZ16(pb);
  regCC &= ~F_OVERFLOW;
}

void op_FD(void) {
  //STD extended
  int32_t addr = fetch16();
  WriteWord(addr, getD());
  flagsNZ16(getD());
  regCC &= ~(F_OVERFLOW);
}

void op_FE(void) {
  //LDU extended
  int32_t addr = fetch16();
  regU = ReadWord(addr);
  flagsNZ16(regU);
  regCC &= ~(F_OVERFLOW);
}

void op_FF(void) {
  //STU extended
  int32_t addr = fetch16();
  WriteWord(addr, regU);
  flagsNZ16(regU);
  regCC &= ~(F_OVERFLOW);
}

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


uint16_t CPUStep() {
  uint16_t oldCPUTickCount = CPUTickCount;

  // LATCH IRQ lines, see 6803 diagram "figure3-1.jpg"

  if (CPUfirqPending) {
    if ((regCC & F_FIRQMASK) == 0) {
      CPUfirqPending = false;
      CPUfirqCount++;
      _executeFirq();
      return CPUTickCount - oldCPUTickCount;
    }
    CPUmissedFIRQ++;
  }

  if (CPUirqPending) {
    if ((regCC & F_IRQMASK) == 0) {
      CPUirqPending = false;
      CPUirqCount++;
      _executeIrq();
      return CPUTickCount - oldCPUTickCount;
    }
    CPUmissedIRQ++;
  }

//  int32_t addr = 0;
//  uint16_t pb = 0;
#ifdef MPU89_BUILD_FOR_COMPUTER    
  if (showDebug) printf("PC=0x%04X ", regPC);
#endif

  uint8_t opcode = fetch();
  CPUTickCount += cycles[opcode];

#ifdef MPU89_BUILD_FOR_COMPUTER  
  if (showDebug) printf("OP=%02X ", opcode);
#endif

  oDispatchTable[opcode]();

  regA &= 0xff;
  regB &= 0xff;
  regCC &= 0xff;
  regDP &= 0xff;
  regX &= 0xffff;
  regY &= 0xffff;
  regU &= 0xffff;
  regS &= 0xffff;
  regPC &= 0xffff;

#ifdef MPU89_BUILD_FOR_COMPUTER  
  if (showDebug) {
    printf("A=%02X ", regA);
    printf("B=%02X ", regB);
    printf("CC=%02X ", regCC);
    printf("DP=%02X ", regDP);
    printf("X=%04X ", regX);
    printf("Y=%04X ", regY);
    printf("U=%04X ", regU);
    printf("S=%04X\n", regS);
  }
#endif  
  return CPUTickCount - oldCPUTickCount;
}

void CPUReset() {
  CPUirqPending = false;
  CPUfirqPending = false;

  regDP = 0;
  CPUmissedIRQ = 0;
  CPUmissedFIRQ = 0;
  CPUirqCount = 0;
  CPUfirqCount = 0;

  regCC = F_IRQMASK | F_FIRQMASK;
  regPC = ReadWord(vecRESET);
  CPUTickCount = 0;
}



uint16_t CPUSteps(uint16_t numTicks) {
  uint16_t preCPUTickCount = CPUTickCount;
  uint16_t ticksToRun = numTicks;
  uint8_t invalidStepDetected = 0;
  int16_t ticksRemaining = numTicks;
  while (ticksRemaining > 0) {
    uint16_t cycles = CPUStep();
    if (cycles < 1) {
      invalidStepDetected++;
      ticksRemaining--;
    }
    ticksRemaining -= cycles;
  }
  if (invalidStepDetected && ticksToRun > 1 && invalidStepDetected == ticksToRun) {
  }
  return CPUTickCount - preCPUTickCount;
}

void CPUIRQ() {
  CPUirqPending = true;
}

void CPUFIRQ() {
  CPUfirqPending = true;
}

void CPUNMI() {
  CPUnmiCount++;
  _executeNmi();
}

uint16_t CPUGetPC() {
  return regPC;
}
