#include "mpu89.h"
// Williams part number A-12742-xx

bool DisplayUseShadowVariables = false;
//extern volatile uint32_t clock_idx;

byte RAM[RAM_SIZE];
byte *ROM;
uint32_t ROMSize;
byte DisplayRAM[8192];
byte DisplayLowPage = 0;
byte DisplayHighPage = 0;
byte *DisplayLowPageStartAddress = NULL;
byte *DisplayHighPageStartAddress = NULL;
byte DisplayActivePage = 0;
byte DisplayNextActivePage = 0;
byte DisplayScanlineTrigger = 31;
byte DisplayCurrentScanline = 0;
int memoryWrites;
int protectedMemoryWriteAttempts = 0;
int ticksIrq = 0;
byte *ROM;

uint32_t UpperAddressMask = 0x00000000;

#ifndef MPU89_BUILD_FOR_COMPUTER
#include "gd32f4xx.h"

void MCUPortInit(void) {
  /* 1. Enable all necessary GPIO Clocks */
  rcu_periph_clock_enable(RCU_GPIOA);
  rcu_periph_clock_enable(RCU_GPIOB);
  rcu_periph_clock_enable(RCU_GPIOC);
  rcu_periph_clock_enable(RCU_GPIOD);
  rcu_periph_clock_enable(RCU_GPIOE);

  // ========================================================================
  // PORT A
  // ========================================================================
  // IO (PA8): Push-Pull, Initial=1
  gpio_bit_set(GPIOA, GPIO_PIN_8);
  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
  gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);

  // ========================================================================
  // PORT B
  // ========================================================================
  // Inputs: IRQ (PB0), FIRQ (PB1)
  gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1);

  // Push-Pull Outputs: RW (PB2), DREN (PB4), RESET (PB5), DISEN (PB13)
  // Initial=1
  uint32_t pb_pp_pins = GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_13 | GPIO_PIN_15;
  gpio_bit_set(GPIOB, pb_pp_pins);
  gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pb_pp_pins);
  gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pb_pp_pins);

  // Open-Drain Outputs: TRIAC (PB3), DISSTROBE (PB12), LAMPCol (PB14), LAMPRow (PB15), 
  // DIS1 (PB8), DIS2 (PB9), DIS3 (PB10), DIS4 (PB11)
  // Initial=1
  uint32_t pb_od_pins = GPIO_PIN_3 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | 
                        GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_14;
  gpio_bit_set(GPIOB, pb_od_pins);
  gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, pb_od_pins);
  gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pb_od_pins);

  // ========================================================================
  // PORT C
  // ========================================================================
  // High Impedance (Floating Input): E (PC0), Q (PC1)
  // Setting mode to INPUT with PUPD_NONE disconnects the pin driver and resistors.
  gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1);

  // Push-Pull Outputs: WDEN (PC8), IOEN (PC9)
  // Initial=1
  // (Note: Removed PC0 and PC1 from this list)
  gpio_bit_set(GPIOC, GPIO_PIN_8 | GPIO_PIN_9);
  gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9);
  gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8 | GPIO_PIN_9);

  // Open-Drain Outputs: SOL1-4 (PC4, PC5, PC6, PC7)
  // Initial=1
  uint32_t pc_od_pins = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  gpio_bit_set(GPIOC, pc_od_pins);
  gpio_output_options_set(GPIOC, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, pc_od_pins);
  gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pc_od_pins);

  // ========================================================================
  // PORT D
  // ========================================================================
  // AddressBus (PD0-PD15): Push-Pull, Initial=0xFFFF
  gpio_bit_set(GPIOD, GPIO_PIN_ALL);
  gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_ALL);
  gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_ALL);

  // ========================================================================
  // PORT E
  // ========================================================================
  // Inputs: DataBus (PE0-PE7), WDD (PE12)
  gpio_mode_set(GPIOE, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | 
                GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_12);

  // Push-Pull Outputs: SWJMP (PE8), SWROW (PE10), SWDIR (PE11), Blank (PE13)
  // Initial=1
  gpio_bit_set(GPIOE, GPIO_PIN_8 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13);

  // SWCOL (PE9): Initial=0
  gpio_bit_reset(GPIOE, GPIO_PIN_9);

  uint32_t pe_pp_pins = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13;
  gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pe_pp_pins);
  gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pe_pp_pins);
}
#endif


bool MPUInit() {

  // clear RAM and Display's shadow RAM
  uint32_t *p = (uint32_t *)RAM;
  uint32_t words = RAM_SIZE / 4;  
  while(words--) *p++ = 0;
  p = (uint32_t *)DisplayRAM;
  words = 8192 / 4;  
  while(words--) *p++ = 0;

  MCUPortInit();
  SetDataBusDirection(true);
  ticksIrq = 0;
  protectedMemoryWriteAttempts = 0;
  memoryWrites = 0;
  UpperAddressMask = 0x00000000;
  ROM = NULL;
  ROMSize = 0;

  return true;
}

void MPURelease() {
}

void MPUReset() {
  ticksIrq = 0;
  protectedMemoryWriteAttempts = 0;
  memoryWrites = 0;
  CPUReset();
  ASICReset();
  DisplayLowPage = 0;
  DisplayHighPage = 0;
  DisplayLowPageStartAddress = &DisplayRAM[DisplayLowPage*512];
  DisplayHighPageStartAddress = &DisplayRAM[DisplayHighPage*512];
}

void MPUSetROMAddress(uint8_t *romLocation, uint32_t romSize) {
  ROM = romLocation;
  ROMSize = romSize;
  UpperAddressMask = (romSize-1) & 0xF0000;
}

void MPUSetCabinetInput(byte value) {
  ASICSetCabinetInput(value);
}

void MPUSetSwitchInput(int switchNr, int optionalValue) {
  ASICSetSwitchInput(switchNr, optionalValue);
}

void MPUSetFliptronicsInput(int value, int optionalValue) {
  ASICSetFliptronicsInput(value, optionalValue);
}

void MPUToggleMidnightMadnessMode() {
  ASICToggleMidnightMadnessMode();
}

void MPUSetDipSwitchByte(byte dipSwitch) {
  ASICSetDipSwitchByte(dipSwitch);
}

byte MPUGetDipSwitchByte() {
  return ASICGetDipSwitchByte();
}

void MPUStart() {
  MPUReset();
}

unsigned short MPUExecuteCycle(unsigned short ticksToRun, unsigned short tickSteps) {
  unsigned short ticksExecuted = 0;
  while (ticksExecuted < ticksToRun) {
    unsigned short singleTicks = CPUSteps(tickSteps);
    ticksExecuted += singleTicks;
    ticksIrq += singleTicks;

    if (ticksIrq>=2048) {
      ticksIrq = 0;
//      if (ASICPeriodicIRQTimerEnabled) {
        CPUIRQ();
//      }
    }

    ASICExecuteCycle(singleTicks);
  }

  return ticksExecuted;
}

void MPUWriteMemory(unsigned int offset, byte value) {
  //this.memoryWriteHandler.writeMemory(offset, value);
}

byte MPURead8(unsigned short offset) {

  if (offset==WPC_SWITCH_COL_SELECT) {
    uint8_t test = ReadDataBus();
    if (test) {
      test = 0;
    }
  }
  SetAddressBus(offset);
  if (offset<RAM1_UPPER_ADDRESS) {
    return RAM[offset];
  }
  if (offset>=RAM2_LOWER_ADDRESS && offset<=RAM2_UPPER_ADDRESS) {
    return RAM[offset];
  }
  if (offset<=HARDWARE_UPPER_ADDRESS) {
    return MPUHardwareRead(offset);
  }
  if (offset>=BANKED_ROM_LOWER_ADDRESS && offset<=BANKED_ROM_UPPER_ADDRESS) {
    return MPUBankswitchedRead(offset);
  }
  if (offset>=SYSTEM_ROM_LOWER_ADDRESS && offset<=SYSTEM_ROM_UPPER_ADDRESS) {
    if (ROM) {
      uint32_t adjustedAddress = (uint32_t)offset | UpperAddressMask;
      return ROM[adjustedAddress];
    }
    return 0;
  }

  return 0;
}

void MPUWrite8(unsigned short offset, byte value) {
//  value &= 0xFF;

  if (offset<=RAM1_UPPER_ADDRESS) RAM[offset] = value;
  else if (offset>=RAM2_LOWER_ADDRESS && offset<=RAM2_UPPER_ADDRESS) RAM[offset] = value;
  else if (offset<=HARDWARE_UPPER_ADDRESS) MPUHardwareWrite(offset, value);

}

byte MPUBankswitchedRead(unsigned int offset) {
  if (ROM) {
    unsigned int activeBank = ASICGetRomBank();
    unsigned int pageOffset = offset + ((activeBank * ROM_BANK_SIZE)-0x4000);
    return ROM[pageOffset];
  }
  return 0;
}



__attribute__((always_inline)) static inline void WriteDisplay(uint16_t address, uint8_t data) {
  // 1. Setup Phase (Bus is idle, Transceiver is off)
  SetAddressBus(address);
  SetDataBus(data);
  SetDataBusDirection(true);

  // 2. Wait for the "Safe Zone"
  // We wait for E to go LOW ( !E is HIGH ). 
  // This is the period where the RAM is NOT listening.
  while (ReadESignal()); 

  // 3. Prepare the Transceiver
  // We set RW and Enable the 4245 while the RAM window is closed.
  GPIO_BC(GPIOB) = (1U << 2); // RW Low
  GPIO_BC(GPIOC) = (1U << 9); // IOEN Low
  
  // 4. Arm the !AddressValid (PORT)
  // We drop IO now. !WE is still HIGH because !E is still HIGH.
  GPIO_BC(GPIOA) = (1U << 8); // IO Low
  
  // 5. Trigger the Hardware Write
  // Now we wait for E to go HIGH ( !E goes LOW ).
  // The moment this happens, the OR gate (U19A) output drops.
  // !WE goes LOW. The RAM starts its write.
  while (!ReadESignal()); 

  // 6. Stability Window
  // The RAM is now in its transparent write state. 
  // We stay here for a moment to ensure the signal is rock solid.
  __NOP(); __NOP(); __NOP(); __NOP();

  // 7. The Hardware Latch
  // We wait for E to go LOW ( !E goes HIGH ).
  // This hardware edge physically ENDS the write and latches the data.
  // The 4245 is still actively driving the bus.
  while (ReadESignal());

  // 8. Data Hold & Cleanup
  // The write is over. We wait a tiny bit to satisfy RAM hold times
  // before we "let go" of the bus.
  __NOP(); __NOP(); __NOP(); 
  
  GPIO_BOP(GPIOA) = (1U << 8); // IO High (!AddressValid released)
  GPIO_BOP(GPIOC) = (1U << 9); // IOEN High (4245 Tri-state)
  GPIO_BOP(GPIOB) = (1U << 2); // RW High (Return to Read/Idle)
}



void MPUHardwareWrite(unsigned int offset, byte value) {
  // write/mirror value to ram, so its visible using the memory monitor
  RAM[offset] = value;
  
  if (offset>=WPC_FLIPTRONICS_FLIPPER_PORT_A && offset<=WPC_ZEROCROSS_IRQ_CLEAR) {
    ASICWrite(offset, value);
  } else if (offset==0x3FDC || offset==0x3FDD) {
//    if (offset==0x3FDC) mvprintw(1, 20, "Snd Data: 0x%04X = 0x%02X\n", offset, value);
//    else mvprintw(1, 40, "Snd Ctrl: 0x%04X = 0x%02X\n", offset, value);
  } else if (offset>=0x3800 && offset<=0x3A00) {
    // Trying to write to the low page of display
    DisplayLowPageStartAddress[offset-0x3800] = value;
    WriteDisplay(offset, value);
  } else if (offset>=0x3A00 && offset<=0x3C00) {
    // Trying to write to the high page of display
    DisplayHighPageStartAddress[offset-0x3A00] = value;
    WriteDisplay(offset, value);
  } else if (offset==WPC_DMD_LOW_PAGE) {
    DisplayLowPage = value;
    if (DisplayLowPage>15) DisplayLowPage = 15;
    DisplayLowPageStartAddress = &DisplayRAM[DisplayLowPage*512];
    WriteDisplay(offset, value);
  } else if (offset==WPC_DMD_HIGH_PAGE) {
    DisplayHighPage = value;
    if (DisplayHighPage>15) DisplayHighPage = 15;
    DisplayHighPageStartAddress = &DisplayRAM[DisplayHighPage*512];
    WriteDisplay(offset, value);
  } else if (offset==WPC_DMD_ACTIVE_PAGE) {
    DisplayNextActivePage = value;
    if (DisplayNextActivePage>15) DisplayNextActivePage = 15;
    WriteDisplay(offset, value);
  } else if (offset==WPC_DMD_SCANLINE) {
    DisplayScanlineTrigger = value;
//    if (DisplayScanlineTrigger>31) DisplayScanlineTrigger = 31;
    WriteDisplay(offset, value);
  }


}



__attribute__((always_inline)) static inline uint8_t ReadDisplay1(uint16_t address) {
  uint8_t result;

  SetAddressBus(address);    
  // 2. Prepare for Read: Set Port E to Input (High-Z)
  SetDataBusDirection(false); 
  
  // Set RW High (Read Mode)
  GPIO_BOP(GPIOB) = (1U << 2);
  // Set IOEN line Low
  GPIO_BC(GPIOC) = (1U << 9);

  DelayQuarterCycle();
  DelayQuarterCycle();
  DelayQuarterCycle();

  // 4. Trigger: Drop IO to enable the DMD address decoder
  GPIO_BC(GPIOA) = (1U << 8);
  DelayQuarterCycle();
  DelayQuarterCycle();
  DelayQuarterCycle();

  // Grab the data now while the board is still driving the bus
  result = ReadDataBus();

  // 7. Cleanup: Disable external hardware BEFORE restoring outputs
  GPIO_BOP(GPIOA) = (1U << 8); // IO High
  GPIO_BOP(GPIOC) = (1U << 9); // IOEN High
  GPIO_BC(GPIOB) = (1U << 2);  // Restore RW to Write (Default)

  // 8. Safety: Give external hardware ~15ns to Hi-Z its buffers
  DelayQuarterCycle();

  // Restore Data Bus to Output
  SetDataBusDirection(true);

  return result;
}

__attribute__((always_inline)) static inline uint8_t ReadDisplay(uint16_t address) {
  uint8_t result;

  // 1. Setup Phase
  SetAddressBus(address);    
  SetDataBusDirection(false); // MCU pins to Input (High-Z)

  // 2. Prepare the Path
  // Set RW High (Read Mode) and Enable Transceiver (B <- A)
  GPIO_BOP(GPIOB) = (1U << 2);
  GPIO_BC(GPIOC) = (1U << 9);

  // 3. Wait for the "Safe Zone" (E Low / !E High)
  // We wait for the inactive phase of the clock to "arm" the PORT signal.
  while (ReadESignal()); 

  // 4. Present PORT
  // Drop IO to enable the DMD address decoder. 
  // Data won't hit the bus yet because !E is still High.
  GPIO_BC(GPIOA) = (1U << 8);

  // 5. Hardware Trigger (Wait for E High / !E Low)
  // The moment E goes High, the !WE/OE logic on the peripheral 
  // opens up and the RAM/Buffers start driving the bus.
  while (!ReadESignal()); 

  // 6. Propagation Delay
  // Give the data time to travel from the RAM through the 74LS245 
  // and the 4245 to your MCU pins.
  __NOP(); __NOP(); __NOP(); __NOP();

  // 7. Sample the Bus
  // Grab the data while E is solidly High and the bus is driven.
  result = ReadDataBus();

  // 8. Wait for Hardware Latch/End (Wait for E Low / !E High)
  // This ensures we don't "let go" while the peripheral still thinks 
  // a valid cycle is happening.
  while (ReadESignal());

  // 9. Cleanup
  GPIO_BOP(GPIOA) = (1U << 8); // IO High
  GPIO_BOP(GPIOC) = (1U << 9); // IOEN High
  GPIO_BC(GPIOB) = (1U << 2);  // Restore RW to Write (Default)

  // 10. Turnaround Safety
  // Small delay to ensure peripheral buffers are High-Z before MCU drives again.
  __NOP(); __NOP(); 
  SetDataBusDirection(true);

  return result;
}



/*
__attribute__((always_inline)) static inline uint8_t ReadDisplay(uint16_t address) {
    uint8_t result;

    // 1. Setup Address
    SetAddressBus(address);
    
    // 2. PREPARE FOR READ
    // Switch Data Bus to Input (High-Z) so we don't fight the RAM
    SetDataBusDirection(false); 
    
    // Set RW High (Read Mode) - PB2
    GPIO_BOP(GPIOB) = (1U << 2);

    // 3. Select the Board
    // Set IO line Low (Active)
    GPIO_BC(GPIOA) = (1U << 8);
    // Set IOEN line Low (Active)
    GPIO_BC(GPIOC) = (1U << 9);

    // ------------------------------------------------------
    // THE INVERTED READ PULSE
    // ------------------------------------------------------

    // 4. Drop Q (PC1) - Start of Cycle
    GPIO_BC(GPIOC) = (1U << 1);
    DelayQuarterCycle(); 

    // 5. Drop E (PC0) - Enable
    // The Display Board sees E go Active and puts data on the bus
    GPIO_BC(GPIOC) = (1U << 0);
    DelayQuarterCycle(); 

    // 6. Raise Q (PC1)
    GPIO_BOP(GPIOC) = (1U << 1);
    
   // Give a tiny bit more time for the external hardware to finish its 
    // internal logic now that Q has transitioned.
    DelayQuarterCycle();

    // CRITICAL: SAMPLE DATA NOW
    // We sample right at the end of the E pulse to give the hardware 
    // the maximum access time to stabilize the bus.
    result = ReadDataBus();
    
    // 7. Raise E (PC0) - End of Cycle
    // The Display Board stops driving the bus here
    GPIO_BOP(GPIOC) = (1U << 0);

    // ------------------------------------------------------

    // 8. Cleanup & Restore Defaults
    // Release IO/IOEN (High)
    GPIO_BOP(GPIOA) = (1U << 8);
    GPIO_BOP(GPIOC) = (1U << 9);

    // Restore RW to Low (Write default)
    GPIO_BC(GPIOB) = (1U << 2);

    // WAIT for the external chip to actually let go of the bus (Hi-Z)
    // before we start driving it again.
    DelayQuarterCycle();

    // Restore Data Bus to Output (Default state)
    SetDataBusDirection(true);

    return result;
}
*/


byte MPUHardwareRead(unsigned int offset) {

  if (offset>=0x3800 && offset<=0x3A00) {
//    return ReadDisplay(offset);
    return DisplayLowPageStartAddress[offset-0x3800];
  } else if (offset>=0x3A00 && offset<=0x3C00) {
//    return ReadDisplay(offset);
    return DisplayHighPageStartAddress[offset-0x3A00];
  } else if (offset==WPC_DMD_LOW_PAGE) {
    if (!DisplayUseShadowVariables) {
      DisplayLowPage = ReadDisplay(offset);
    }
    return DisplayLowPage;
  } else if (offset==WPC_DMD_HIGH_PAGE) {
    if (!DisplayUseShadowVariables) {
      DisplayHighPage = ReadDisplay(offset);
    }
    return DisplayHighPage;
  } else if (offset==WPC_DMD_ACTIVE_PAGE) {
    if (!DisplayUseShadowVariables) {
      DisplayNextActivePage = ReadDisplay(offset);
    }
    return DisplayNextActivePage;
  } else if (offset==WPC_DMD_SCANLINE) {
    if (!DisplayUseShadowVariables) {
      DisplayCurrentScanline = ReadDisplay(offset);
    }
    return DisplayCurrentScanline;
  } else if (offset >= 0x3FDC && offset <= 0x3FFF) {
     return ASICRead(offset);
  }

  return 0;
   
}

#ifdef MPU89_BUILD_FOR_COMPUTER  

bool MPULoadROM(char *filename) {
  long fileSize = 0x80000;

  FILE *fptr;
  // Open a file in read mode
  fptr = fopen(filename, "rb");

  // Print some text if the file does not exist
  if(fptr == NULL) {
    return false;
  }
  fseek(fptr, 0, SEEK_END); // seek to end of file
  fileSize = ftell(fptr); // get current file pointer
  fseek(fptr, 0, SEEK_SET); // seek back to beginning of file

  fread(ROM, 1, fileSize, fptr);

  // Close the file
  fclose(fptr);
  UpperAddressMask = (fileSize-1) & 0xF0000;
  return true;
}
#endif

// This function returns the current data of the
// active page at the given scanline
byte *MPUGetDisplayScanlineData(byte scanLine) {
  if (DisplayRAM==NULL) return NULL;

  byte *displayFrame = &DisplayRAM[DisplayActivePage*512 + scanLine*16];
  return displayFrame;
}

// This function tells the CPU board which
// scanline is currently being drawn by the display
// which is accessed when the CPU reads from WPC_DMD_SCANLINE
void MPUCurrentScanline(byte curScanLine) {
  DisplayCurrentScanline = curScanLine;
}

void MPUVerticalRefresh() {
  DisplayActivePage = DisplayNextActivePage;
}

// This function tells the display which scanline the
// CPU wants an interrupt on
byte MPUGetTriggerScanline() {
  return DisplayScanlineTrigger;
}

void MPUFIRQ() {
  CPUFIRQ();
}
