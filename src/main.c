//#include <stdio.h>
#include "mpu89.h"
#include "gd32f4xx.h"

#define FAKE_DISPLAY_FIRQ
#define ROM_IN_C_FILE_ARRAY

#ifdef ROM_IN_C_FILE_ARRAY
uint8_t* GetROMPointer(void);
uint32_t GetROMSize(void);
#endif

extern void initialise_monitor_handles(void);

// Define the target frequency
#define TARGET_FREQ 2000000 // 2 MHz

// Add these as global or static variables so the setup can see them
static uint32_t last_cycles = 0;
static uint32_t accumulated_ticks = 0;
static uint32_t leftover_cycles = 0;

void EnableCycleCounter(void) {
    // 1. Enable TRC (Trace)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    
    // 2. Reset the hardware counter
    DWT->CYCCNT = 0;
    
    // 3. IMPORTANT: Reset our software tracking variables too
    last_cycles = 0;
    accumulated_ticks = 0;
    leftover_cycles = 0;

    // 4. Enable cycle counter hardware
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t TwoMHzTicksSinceStart() {
    static uint32_t last_cycles = 0;
    static uint32_t accumulated_ticks = 0;
    static uint32_t leftover_cycles = 0;

    uint32_t current_cycles = DWT->CYCCNT;
    uint32_t cycles_per_tick = SystemCoreClock / TARGET_FREQ;

    // 1. Calculate how many cycles passed since we last checked
    // This subtraction works even if current_cycles wrapped around (e.g., 5 - 0xFFFFFFFB = 10)
    uint32_t elapsed_cycles = current_cycles - last_cycles;
    last_cycles = current_cycles;

    // 2. Add any leftover cycles from the previous call
    uint32_t total_to_process = elapsed_cycles + leftover_cycles;

    // 3. Update the tick count and save the remainder
    accumulated_ticks += (total_to_process / cycles_per_tick);
    leftover_cycles = total_to_process % cycles_per_tick;

    return accumulated_ticks;
}

void InitPE9_PushPull(void) {
    // 1. Enable Port E Clock
    RCU_AHB1EN |= (1U << 4);

    // 2. Set Mode to Output (01) for Pin 9
    // Clear bits 18-19, set bit 18
    GPIO_CTL(GPIOE) &= ~(3U << 18);
    GPIO_CTL(GPIOE) |=  (1U << 18);

    // 3. Set Output Type to Push-Pull (0) for Pin 9
    // Explicitly clear bit 9
    GPIO_OMODE(GPIOE) &= ~(1U << 9);

    // 4. Set Speed to Very High (11) for Pin 9
    GPIO_OSPD(GPIOE) |= (3U << 18);

    // 5. Preset Low
    GPIO_BC(GPIOE) = (1U << 9);
}

void PC4OD() {
/* 1. Ensure clock is on */
rcu_periph_clock_enable(RCU_GPIOC);

/* 2. Set the Output Type FIRST (Open-Drain) */
gpio_output_options_set(GPIOC, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_9);

/* 3. Set the Mode LAST (Output) */
gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_9);
}

void InitPC4_PushPull(void) {
    
    // 1. Enable Port C Clock (Bit 2)
    RCU_AHB1EN |= (1U << 2);

    // 2. Set Mode to Output (01) for Pin 4
    // Bits 8 and 9 (Pin 4 << 1)
    GPIO_CTL(GPIOC) &= ~(3U << 8);
    GPIO_CTL(GPIOC) |=  (1U << 8);

    // 3. Set Output Type to Push-Pull (0)
    // Clear bit 4
    GPIO_OMODE(GPIOC) &= ~(1U << 4);

    // 4. Set Speed to Very High (11)
    GPIO_OSPD(GPIOC) |= (3U << 8);

    // 5. Initial State: LOW
    GPIO_BC(GPIOC) = (1U << 4);

}

#include "gd32f4xx.h"


void pc4_tristate_init(void) {
    rcu_periph_clock_enable(RCU_GPIOC);
    
    // Ensure the output latch is 0 so it pulls to GND when we enable Output mode
    gpio_bit_reset(GPIOC, GPIO_PIN_4);
    
    // Start as Input (High-Z) so the 1.5k resistor pulls the clock line HIGH
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
}

void strobe_74ls374(void) {
    // 1. Switch to Push-Pull Output (Drives PC4 to 0V)
    // This is the leading edge of your clock pulse
    gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_4);

    // 2. Pulse Width Delay
    // LS logic needs ~15-20ns; this loop is plenty for a 74LS374
    for(volatile uint32_t i = 0; i < 50; i++);

    // 3. Switch back to Input (High-Z)
    // The 1.5k resistor pulls the line back to 5V
    // THIS IS THE RISING EDGE THAT LATCHES THE DATA
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
}


#ifdef FAKE_DISPLAY_FIRQ
uint8_t DisplayScanLine = 0;
void UpdateDisplayTracking() {
    MPUCurrentScanline(DisplayScanLine);
    if (DisplayScanLine==MPUGetTriggerScanline()) MPUFIRQ();
    DisplayScanLine += 1;
    if (DisplayScanLine>31) {
        DisplayScanLine = 0;
        MPUVerticalRefresh();
    }
}
#endif

int main(void) {    
    //initialise_monitor_handles();

    //printf("Semihosting Initialized! Waiting for emulator loop...\n");

    // LED Setup
    RCU_AHB1EN |= (1U << 4);              
    GPIO_CTL(GPIOE) &= ~(3U << (14 * 2)); 
    GPIO_CTL(GPIOE) |=  (1U << (14 * 2)); 
    GPIO_OMODE(GPIOE) &= ~(1U << 14);     
    GPIO_OSPD(GPIOE) |= (3U << (14 * 2)); 

    // Main Application
    GPIO_BOP(GPIOE) = (1U << 14); // turn the LED on
    MPUInit();
    MPUSetROMAddress(GetROMPointer(), GetROMSize());
    ASICInit();
    CPUSetCallbacks(MPUWrite8, MPURead8);
    MPUReset();

    EnableCycleCounter();
    uint32_t lastTickCount = TwoMHzTicksSinceStart();
#ifdef FAKE_DISPLAY_FIRQ
    uint32_t lastDisplayUpdateTicks = 0;
#endif    

    while (1) {
        uint32_t currentTickCount = TwoMHzTicksSinceStart();

        // Calculate how many 2MHz ticks have passed since we last checked
        int32_t ticksToRun = (int32_t)(currentTickCount - lastTickCount);

        if (ticksToRun > 0) {
            if (ticksToRun > 20000) ticksToRun = 20000;

            // Run the emulator for exactly that many ticks
            uint32_t ticksExecuted = MPUExecuteCycle(ticksToRun, 1);

            // Advance our "last run" marker by the amount we just executed
            lastTickCount += ticksExecuted;

            if ((currentTickCount/1000000) & 0x01) GPIO_BC(GPIOE) = (1U << 14);
            else GPIO_BOP(GPIOE) = (1U << 14);

#ifdef FAKE_DISPLAY_FIRQ
            lastDisplayUpdateTicks += ticksExecuted;
            if (lastDisplayUpdateTicks>=512) {
                lastDisplayUpdateTicks = 0;
                UpdateDisplayTracking();
            }            
#endif            
        }

    }
        
}









/*
        Old test loop


    while(1) {
        uint32_t current_tick_count = TwoMHzTicksSinceStart();
        GPIO_BC(GPIOC) = (1U << 9);
        GPIO_BC(GPIOB) = (1U << 2);

        bool FIRQPin = (GPIO_ISTAT(GPIOB) & GPIO_PIN_1) != 0;
        bool IRQPin = (GPIO_ISTAT(GPIOB) & GPIO_PIN_0) != 0;

        if (0) {
            GPIO_BC(GPIOE) = (1U << 14);
        } else {
            if ((current_tick_count/200000) & 0x01) {
                GPIO_BC(GPIOB) = (1U << 11);
                GPIO_BC(GPIOE) = (1U << 14);
            } else {
                GPIO_BOP(GPIOB) = (1U << 11);
                GPIO_BOP(GPIOE) = (1U << 14);
            }
        }
    }




*/