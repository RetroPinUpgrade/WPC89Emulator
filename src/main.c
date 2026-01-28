//#include <stdio.h>
#include "mpu89.h"
#include "gd32f4xx.h"

//#define FAKE_DISPLAY_FIRQ
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




void oldInitTimer0PWM(void) {
    // 1. Enable Clocks
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_TIMER0);

    // 2. Configure Pins for AF1 (TIMER0)
    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);

    // 3. Timer0 Base Configuration
    timer_parameter_struct timer_init_para;
    timer_deinit(TIMER0);

    // For 2MHz output in Toggle Mode, we need 4MHz toggle events
    // 240MHz / 60 = 4MHz. Period = 60 - 1 = 59
    timer_init_para.prescaler         = 0;
    timer_init_para.alignedmode       = TIMER_COUNTER_EDGE;
    timer_init_para.counterdirection  = TIMER_COUNTER_UP;
    timer_init_para.period            = 59; 
    timer_init_para.clockdivision     = TIMER_CKDIV_DIV1;
    timer_init_para.repetitioncounter = 0;
    timer_init(TIMER0, &timer_init_para);

    // 4. Channel Configuration
    timer_oc_parameter_struct timer_oc_init_para;
    timer_oc_init_para.outputstate  = TIMER_CCX_ENABLE;
    timer_oc_init_para.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_oc_init_para.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;

    // PA9 (CH1): Toggles at counter match 0
    timer_channel_output_config(TIMER0, TIMER_CH_1, &timer_oc_init_para);
    timer_channel_output_pulse_value_config(TIMER0, TIMER_CH_1, 0);
    timer_channel_output_mode_config(TIMER0, TIMER_CH_1, TIMER_OC_MODE_TOGGLE);

    // PA10 (CH2): Toggles at counter match 30 (90 degree shift)
    // 125ns delay = 30 ticks @ 240MHz
    timer_channel_output_config(TIMER0, TIMER_CH_2, &timer_oc_init_para);
    timer_channel_output_pulse_value_config(TIMER0, TIMER_CH_2, 30);
    timer_channel_output_mode_config(TIMER0, TIMER_CH_2, TIMER_OC_MODE_TOGGLE);

    // 5. Enable Main Output (Required for Advanced Timer0)
    timer_primary_output_config(TIMER0, ENABLE);

    // 6. Enable Timer
    timer_enable(TIMER0);
}

void InitTimer0PWM(void) {
    // 1. Enable Clocks
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_TIMER0);

    // 2. Configure Pins for AF1 (TIMER0)
    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);

    // 3. Timer0 Base Configuration
    timer_parameter_struct timer_init_para;
    timer_deinit(TIMER0);

    // 200MHz / 50 = 4MHz toggle events (results in 2MHz waveform)
    // Period = 50 - 1 = 49
    timer_init_para.prescaler         = 0;
    timer_init_para.alignedmode       = TIMER_COUNTER_EDGE;
    timer_init_para.counterdirection  = TIMER_COUNTER_UP;
    timer_init_para.period            = 49; 
    timer_init_para.clockdivision     = TIMER_CKDIV_DIV1;
    timer_init_para.repetitioncounter = 0;
    timer_init(TIMER0, &timer_init_para);

    // 4. Channel Configuration
    timer_oc_parameter_struct timer_oc_init_para;
    timer_oc_init_para.outputstate  = TIMER_CCX_ENABLE;
    timer_oc_init_para.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_oc_init_para.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;

    // PA9 (CH1): Toggles at counter match 0
    timer_channel_output_config(TIMER0, TIMER_CH_1, &timer_oc_init_para);
    timer_channel_output_pulse_value_config(TIMER0, TIMER_CH_1, 0);
    timer_channel_output_mode_config(TIMER0, TIMER_CH_1, TIMER_OC_MODE_TOGGLE);

    // PA10 (CH2): Toggles at counter match 25 (90 degree shift)
    // 125ns delay = 25 ticks @ 200MHz
    timer_channel_output_config(TIMER0, TIMER_CH_2, &timer_oc_init_para);
    timer_channel_output_pulse_value_config(TIMER0, TIMER_CH_2, 25);
    timer_channel_output_mode_config(TIMER0, TIMER_CH_2, TIMER_OC_MODE_TOGGLE);

    // 5. Enable Main Output (Required for Advanced Timer0)
    timer_primary_output_config(TIMER0, ENABLE);

    // 6. Enable Timer
    timer_enable(TIMER0);
}


int main(void) {    
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

    GPIO_BOP(GPIOC) = (1U << 0);

    EnableCycleCounter();
    InitTimer0PWM();
    uint32_t lastTickCount = TwoMHzTicksSinceStart();
#ifdef FAKE_DISPLAY_FIRQ
    uint32_t lastDisplayUpdateTicks = 0;
#endif
    bool FIRQHasBeenTriggered = false;

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
#else
            if (FIRQTriggered()) {
                if (FIRQHasBeenTriggered==false) {
                    // We haven't fired this FIRQ yet
                    FIRQHasBeenTriggered = true;
                    ASICFirqSourceDmd(true);
                    MPUFIRQ();
                }
            } else {
                // Reset for next FIRQ
                FIRQHasBeenTriggered = false;
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