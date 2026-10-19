//#include <stdio.h>
#include "mpu89.h"
#include "HPSoundCard.h"
#include "gd32f4xx.h"
#include "string.h"
#include "RPU-WPC-Display.h"
#include "RPU-WPC-OperatorMenu.h"
#include "HPSDCardReader.h"
#include <stdio.h>
#include "SecurityPIC.h"

extern volatile uint8_t GIBrightness[5];
extern volatile uint8_t LastTriacValue;

void SwitchTo240MHz(void) {
    /* 1. Reset RCU to a known state */
    rcu_deinit();
    
    /* 2. Enable HXTAL (External Crystal) */
    rcu_osci_on(RCU_HXTAL);
    if (ERROR == rcu_osci_stab_wait(RCU_HXTAL)) {
        while(1); // Crystal failed
    }

    /* 3. Set Flash Wait States (WS) for 240MHz */
    /* Note: Compiler suggested WS_WSCNT_15 */
//    fmc_wscnt_set(WS_WSCNT_15); 
    fmc_wscnt_set(WS_WSCNT_7); 

    /* 4. Configure PLL: 25MHz / 25 * 480 / 2 = 240MHz */
    /* Parameters: Source, PSC, N, P, Q */
    rcu_pll_config(RCU_PLLSRC_HXTAL, 25, 480, 2, 10);

    /* 5. Enable PLL and wait */
    rcu_osci_on(RCU_PLL_CK);
    while (ERROR == rcu_osci_stab_wait(RCU_PLL_CK));

    /* 5.5 Configure Bus Dividers for 240MHz Operation */
    /* We write to the RCU_CFG0 register directly to bypass missing library functions. */
    /* Clear AHB, APB1, and APB2 prescaler bits */
    RCU_CFG0 &= ~(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    /* Set AHB=DIV1 (240MHz), APB1=DIV4 (60MHz), APB2=DIV2 (120MHz) */
    RCU_CFG0 |= (RCU_AHB_CKSYS_DIV1 | RCU_APB1_CKAHB_DIV4 | RCU_APB2_CKAHB_DIV2);

    /* 6. Switch System Clock to PLLP */
    /* Note: Compiler suggested RCU_CKSYSSRC_PLLP */
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);
    
    /* 7. Wait for switch to complete (0x02 is PLLP status) */
    while (RCU_SCSS_PLLP != rcu_system_clock_source_get());

    /* 8. Update global SystemCoreClock variable */
    SystemCoreClockUpdate();
}

void OldSwitchTo240MHz(void) {
    /* 1. Reset RCU to a known state */
    rcu_deinit();
    
    /* 2. Enable HXTAL (External Crystal) */
    rcu_osci_on(RCU_HXTAL);
    if (ERROR == rcu_osci_stab_wait(RCU_HXTAL)) {
        while(1); // Crystal failed
    }

    /* 3. Set Flash Wait States (WS) for 240MHz */
    /* Note: Compiler suggested WS_WSCNT_15 */
    fmc_wscnt_set(WS_WSCNT_15); 

    /* 4. Configure PLL: 25MHz / 25 * 480 / 2 = 240MHz */
    /* Parameters: Source, PSC, N, P, Q */
    rcu_pll_config(RCU_PLLSRC_HXTAL, 25, 480, 2, 10);

    /* 5. Enable PLL and wait */
    rcu_osci_on(RCU_PLL_CK);
    while (ERROR == rcu_osci_stab_wait(RCU_PLL_CK));

    /* 5.5 Configure Bus Dividers for 240MHz Operation */
    /* We write to the RCU_CFG0 register directly to bypass missing library functions. */
    /* Clear AHB, APB1, and APB2 prescaler bits */
    RCU_CFG0 &= ~(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    /* Set AHB=DIV1 (240MHz), APB1=DIV4 (60MHz), APB2=DIV2 (120MHz) */
    RCU_CFG0 |= (RCU_AHB_CKSYS_DIV1 | RCU_APB1_CKAHB_DIV4 | RCU_APB2_CKAHB_DIV2);

    /* 6. Switch System Clock to PLLP */
    /* Note: Compiler suggested RCU_CKSYSSRC_PLLP */
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);
    
    /* 7. Wait for switch to complete (0x02 is PLLP status) */
    while (RCU_SCSS_PLLP != rcu_system_clock_source_get());

    /* 8. Update global SystemCoreClock variable */
    SystemCoreClockUpdate();
}

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


/* * 32-bit state array for the GPIO_BOP register.
 * Lower 16 bits: Set pins (0=PD0, 1=PD1)
 * Upper 16 bits: Reset pins (16=PD0, 17=PD1)
 * * Standard 6809 Phase: Q leads E by 90 degrees.
 */

uint32_t dma_quadrature_states[4] = {
    0x00030000, // State 0: E=0, Q=0 (Reset PD0, Reset PD1)
    0x00010002, // State 1: E=0, Q=1 (Reset PD0, Set PD1)
    0x00000003, // State 2: E=1, Q=1 (Set PD0, Set PD1)
    0x00020001  // State 3: E=1, Q=0 (Set PD0, Reset PD1)
};


void Init_DMA_Quadrature_Clocks(void) {
    // 1. Enable Clocks (GPIOD, DMA0, and TIMER2)
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_DMA0);
    rcu_periph_clock_enable(RCU_TIMER2);

    // 2. Configure PD0 (E) and PD1 (Q) as standard Push-Pull Outputs
    // Do NOT use AF mode, as the DMA needs to drive the GPIO port directly.
    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_0 | GPIO_PIN_1);

    // 3. Configure DMA0, Channel 2 (Linked to TIMER2 Update Event)
    dma_single_data_parameter_struct dma_init_struct;
    dma_deinit(DMA0, DMA_CH2);
    
    dma_init_struct.periph_addr         = (uint32_t)&GPIO_BOP(GPIOD);
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr        = (uint32_t)dma_quadrature_states;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_32BIT;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_ENABLE;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.number              = 4; // 4 states in the array
    dma_init_struct.priority            = DMA_PRIORITY_ULTRA_HIGH; // Max priority to minimize jitter
    dma_single_data_mode_init(DMA0, DMA_CH2, &dma_init_struct);
    
    // Select the specific subperipheral route for TIMER2_UP on GD32F4
    dma_channel_subperipheral_select(DMA0, DMA_CH2, DMA_SUBPERI5);
    dma_channel_enable(DMA0, DMA_CH2);

    // 4. Configure TIMER2 as the 8MHz pacing clock
    timer_parameter_struct timer_init_para;
    timer_deinit(TIMER2);

    // Assuming a 200MHz System Clock:
    // 200MHz / 25 ticks = 8MHz update rate (125ns per state)
    // 8MHz / 4 states = 2MHz full quadrature cycle (500ns)
    timer_init_para.prescaler         = 0;
    timer_init_para.alignedmode       = TIMER_COUNTER_EDGE;
    timer_init_para.counterdirection  = TIMER_COUNTER_UP;
    // 120MHz / 15 ticks = 8MHz update rate (125ns per state)
    // 120MHz / 12 ticks = 10MHz update rate (100ns per state)
    timer_init_para.period = 12;
    timer_init_para.clockdivision     = TIMER_CKDIV_DIV1;
    timer_init_para.repetitioncounter = 0;
    timer_init(TIMER2, &timer_init_para);

    // 5. Connect the Timer to the DMA Request and Start
    timer_dma_enable(TIMER2, TIMER_DMA_UPD);
    timer_enable(TIMER2);
}


// This timer sets up E & Q lines at 2MHz (with Q leading E by 90 degrees)
// on PA9 and PA10 (for the DMD Controller or any other periperals that need it)
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


void InitProfilePins(void) {
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2 | GPIO_PIN_3);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2 | GPIO_PIN_3);
}


#define RTC_PROOF_VALUE     0x3208  // stored in RTC_BKP0
// The backup domain is anything that needs to run on 
// clock power or needs to be stored between power cycles
void BackupDomainInit(void) {
    rcu_periph_clock_enable(RCU_PMU);
    rcu_periph_clock_enable(RCU_BKPSRAM);
    pmu_backup_write_enable();

    // 1. Enable the Backup SRAM low-power regulator
    // This connects the 4KB SRAM array to the backup power domain (VBAT)
    pmu_backup_ldo_config(PMU_BLDOON_ON);

    // 2. Wait for the Backup LDO Ready Flag to be stable
    // This ensures the regulator is actually providing power before we trust the RAM
    while (SET != pmu_flag_get(PMU_FLAG_BLDORF));

    // Check if RTC has been configured before (using backup register as flag)
    if (RTC_BKP1 != RTC_PROOF_VALUE) {
        rcu_osci_on(RCU_LXTAL); 
        if (ERROR == rcu_osci_stab_wait(RCU_LXTAL)) return;
        
        rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
        rcu_periph_clock_enable(RCU_RTC);

        /* --- PRESCALER CONFIGURATION START --- */
        rtc_parameter_struct rtc_init_para;
        
        // 1Hz = 32768 / ((127+1) * (255+1))
        rtc_init_para.factor_asyn = 0x7F; // 127
        rtc_init_para.factor_syn  = 0xFF; // 255
        
        // You must provide initial values when calling rtc_init
        rtc_init_para.year = 0x26; 
        rtc_init_para.month = RTC_FEB;
        rtc_init_para.date = 0x01;
        rtc_init_para.day_of_week = RTC_SUNDAY;
        rtc_init_para.hour = 0x22;
        rtc_init_para.minute = 0x25;
        rtc_init_para.second = 0x00;
        rtc_init_para.display_format = RTC_24HOUR;
        rtc_init_para.am_pm = RTC_AM;

        // This function handles the INITM bit and WPK keys automatically
        rtc_init(&rtc_init_para);
        /* --- PRESCALER CONFIGURATION END --- */
        
        RTC_BKP1 = RTC_PROOF_VALUE;
    } else {
        // SUBSEQUENT RESETS - don't reconfigure, just enable
        rcu_periph_clock_enable(RCU_RTC);
        // EXTREMELY IMPORTANT: Clear the sync flag manually before waiting.
        // This forces the hardware to perform a fresh synchronization.
        RTC_STAT &= ~RTC_STAT_RSYNF;
    }

    rtc_register_sync_wait();

    // LVD setup (always do this part)
    pmu_lvd_select(PMU_LVDT_7);
    exti_init(EXTI_16, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
    exti_interrupt_flag_clear(EXTI_16);
    nvic_irq_enable(LVD_IRQn, 0, 0);
    RTC_CTL |= (1U << 18); // Set BYPSHAD bit
}


// This handler is called when the power drops below 2.6 (?) V
// so we can remember anything important before the next run
// (this needs to be quick)
void LVD_IRQHandler(void) {
    if (SET == exti_interrupt_flag_get(EXTI_16)) {
        SetBlanking(true); // make peripherals go dark
        // Clear flag to allow for next trigger
        exti_interrupt_flag_clear(EXTI_16);
    }
}

#define DEC2BCD(x) ((((x) / 10) << 4) | ((x) % 10))
#define BCD2DEC(x) ((((x) >> 4) * 10) + ((x) & 0x0F))

void SetDateTimeFromASIC() {
    uint16_t year;
    uint8_t month, day, dow, hour, minute;
    ASICGetDateTime(&year, &month, &day, &dow, &hour, &minute);
    
    rtc_parameter_struct rtc_time;

    rtc_time.year   = DEC2BCD((uint8_t)(year % 100));
    rtc_time.month  = DEC2BCD(month);
    rtc_time.date   = DEC2BCD(day);
    uint8_t rtcDOW = RTC_SUNDAY;
    switch (dow) {
        case 1: rtcDOW = RTC_SUNDAY; break;
        case 2: rtcDOW = RTC_MONDAY; break;
        case 3: rtcDOW = RTC_TUESDAY; break;
        case 4: rtcDOW = RTC_WEDSDAY; break; // just had to guess at this spelling.
        case 5: rtcDOW = RTC_THURSDAY; break;
        case 6: rtcDOW = RTC_FRIDAY; break;
        case 7: rtcDOW = RTC_SATURDAY; break;
    }
    rtc_time.day_of_week = rtcDOW; // Ensure this is 1-7
    rtc_time.hour   = DEC2BCD(hour);
    rtc_time.minute = DEC2BCD(minute);
    rtc_time.second = 0;
    
    rtc_time.display_format = RTC_24HOUR;
    rtc_time.am_pm = RTC_AM;

    // 2. MUST include the prescalers so they aren't overwritten with junk
    rtc_time.factor_asyn = 0x7F; // 127
    rtc_time.factor_syn  = 0xFF; // 255

    // 3. Proper RTC write sequence
    pmu_backup_write_enable();
    
    // Note: rtc_init() calls rtc_init_mode_enter() and exit() internally.
    // Calling them again is redundant but harmless.
    rtc_init(&rtc_time); 
    
    // Ensure sync is maintained after a write
    rtc_register_sync_wait();
}


void SetASICFromDateTimeRegisters(uint32_t rtc_date_reg, uint32_t rtc_time_reg) {    
    // Decode TIME register (BCD format)
    // RTC_TIME bits: [22:20]=hour tens, [19:16]=hour ones, [14:12]=min tens, [11:8]=min ones, [6:4]=sec tens, [3:0]=sec ones
    uint8_t hours   = ((rtc_time_reg >> 20) & 0x3) * 10 + ((rtc_time_reg >> 16) & 0xF);
    uint8_t minutes = ((rtc_time_reg >> 12) & 0x7) * 10 + ((rtc_time_reg >> 8) & 0xF);
    //uint8_t seconds = ((rtc_time_reg >> 4) & 0x7) * 10 + (rtc_time_reg & 0xF);
    
    // Decode DATE register (BCD format)
    // RTC_DATE bits: [23:20]=year tens, [19:16]=year ones, [15:13]=DOW, [12]=month tens, [11:8]=month ones, [5:4]=day tens, [3:0]=day ones
    uint16_t year  = 2000 + ((rtc_date_reg >> 20) & 0xF) * 10 + ((rtc_date_reg >> 16) & 0xF);
    uint8_t dow  = ((rtc_date_reg >> 13) & 0x7) + 1;
    if (dow==8) dow = 1;
    uint8_t month = ((rtc_date_reg >> 12) & 0x1) * 10 + ((rtc_date_reg >> 8) & 0xF);
    uint8_t day   = ((rtc_date_reg >> 4) & 0x3) * 10 + (rtc_date_reg & 0xF);
    ASICSetCurrentDateTime(year, month, day, dow, hours, minutes);
}

#define CPU_TICKS_PER_SECOND    2000000U


// This function can be used to test the ports of a new prototype
// board (insert after BackupDomainInit in main)
void TestPorts() {
    EnableCycleCounter();
    uint32_t curTicks = 0;
    uint32_t lastTicks = curTicks;

    MCUPortInit();
    Init_DMA_Quadrature_Clocks();
    SetDISENLow();
    SetBlanking(true);

    while (1) {
        curTicks = TwoMHzTicksSinceStart();

        if (curTicks!=lastTicks) {
            if (ReadZC()) {
                if ((curTicks/200000)%2) SetBoardLEDOff();
                else SetBoardLEDOn();
            } else {
                if ((curTicks/2000000)%2) SetBoardLEDOff();
                else SetBoardLEDOn();
            }

            if ((curTicks/500000)%2) SetDIS4Low();
            else SetDIS4High();

            lastTicks = curTicks;
        }

    }
}



// 0 = 60Hz, 1 = 50Hz
uint8_t ACFrequency = 0; 
// 0 = Incandescent, 1 = LED
uint8_t GIBulbTypes[5] = {0, 0, 0, 0, 0}; 

// Phase Delays in microseconds (Wait time before turning ON)
// This is "phase-cut dimming" aka "leading-edge dimming"
// [Frequency: 0 = 60Hz, 1 = 50Hz][Bulb Type][Brightness 1-7]
// Note: Brightness 8 is handled instantly, Brightness 0 never turns on
const uint16_t GI_PhaseDelays[2][2][8] = {
    // 60Hz (8333us half-cycle period)
    {
        // Incandescent: Slower response, needs longer ON times
        { 0, 8000, 7700, 7300, 6800, 6200, 5500, 4500 },
        // LED: Instantly bright, needs very short ON times (curve is heavily pushed back)
        { 0, 8100, 7900, 7600, 7200, 6700, 6100, 4000 }
    },
    // 50Hz (10000us half-cycle period, +1667us delay to preserve identical ON time)
    {
        // Incandescent
        { 0, 9667, 9367, 8967, 8467, 7867, 7167, 6167 },
        // LED
        { 0, 9767, 9567, 9267, 8867, 8367, 7767, 5667 }
    }
};


// Timeline structures
typedef struct {
    uint16_t delay_us;
    uint8_t  triac_mask;
} GI_Event;

GI_Event GITimeline[5];
uint8_t GITimelineCount = 0;
uint8_t GICurrentEventIdx = 0;
uint8_t GIActiveMask = 0; // Tracks which Triacs are currently ON during the cycle




// Returns the mask of Triacs that should turn on IMMEDIATELY at zero-cross
uint8_t BuildGITimeline(void) {
    GITimelineCount = 0;
    uint8_t zc_mask = 0; 
    
    for (int i = 0; i < 5; i++) {
        uint8_t b = GIBrightness[i];
        if (b == 0) continue; // Always OFF
        
        if (b >= 8) {
            zc_mask |= (1 << i); // 100% Brightness: Turn ON at zero-cross
            continue; 
        }
        
        // Look up delay based on bulb type and brightness (1-7)
        uint16_t delay = GI_PhaseDelays[ACFrequency][GIBulbTypes[i]][b];
        
        // Insert into the sorted timeline
        bool merged = false;
        for (int j = 0; j < GITimelineCount; j++) {
            if (GITimeline[j].delay_us == delay) {
                GITimeline[j].triac_mask |= (1 << i); // Group channels with same delay
                merged = true;
                break;
            }
        }
        
        if (!merged) {
            // Standard insertion sort to keep chronological order
            int j = GITimelineCount - 1;
            while (j >= 0 && GITimeline[j].delay_us > delay) {
                GITimeline[j+1] = GITimeline[j];
                j--;
            }
            GITimeline[j+1].delay_us = delay;
            GITimeline[j+1].triac_mask = (1 << i);
            GITimelineCount++;
        }
    }
    return zc_mask;
}

void ConfigZeroCrossInterrupt(void) {
    // Enable SYSCFG clock for EXTI routing
    rcu_periph_clock_enable(RCU_SYSCFG);

    // Route EXTI line 15 to PE15 for the Zero Cross input
    syscfg_exti_line_config(EXTI_SOURCE_GPIOE, EXTI_SOURCE_PIN15);

    // Configure EXTI line 15
    exti_init(EXTI_15, EXTI_INTERRUPT, EXTI_TRIG_RISING); 
    exti_interrupt_flag_clear(EXTI_15);

    // Enable and set priority for the EXTI10-15 interrupt vector
    nvic_irq_enable(EXTI10_15_IRQn, 2, 0);
}

void ConfigGITimer(void) {
    // Enable clock for TIMER5 
    rcu_periph_clock_enable(RCU_TIMER5);
    timer_deinit(TIMER5);

    timer_parameter_struct timerInitPara;
    // Assuming a 240MHz system clock, APB1 timer clock is 120MHz
    timerInitPara.prescaler         = 119; // 120MHz / 120 = 1MHz (1us tick)
    timerInitPara.alignedmode       = TIMER_COUNTER_EDGE;
    timerInitPara.counterdirection  = TIMER_COUNTER_UP;
    timerInitPara.period            = 7800; 
    timerInitPara.clockdivision     = TIMER_CKDIV_DIV1;
    timerInitPara.repetitioncounter = 0;
    
    timer_init(TIMER5, &timerInitPara);
    timer_interrupt_enable(TIMER5, TIMER_INT_UP);
    nvic_irq_enable(TIMER5_DAC_IRQn, 1, 0);
}


void EXTI10_15_IRQHandler(void) {
    if (exti_interrupt_flag_get(EXTI_15) == SET) {
        exti_interrupt_flag_clear(EXTI_15);
        ASICSetZeroCrossFlag();
        SetProfile1High();
        
        // 1. Build the timeline and get 100% brightness mask
        GIActiveMask = BuildGITimeline();
                
        // 2. Output to bus (Preserve relays on bits 5-7)
        SetDataBus((LastTriacValue & 0xE0) | GIActiveMask); 
        SetDRLine(false);
        StrobeTriac(); 
        DelayQuarterCycle();
        SetDRLine(true);
        
        // 3. Start the timer for the first event (if any exist)
        if (GITimelineCount > 0) {
            GICurrentEventIdx = 0;
            timer_autoreload_value_config(TIMER5, GITimeline[0].delay_us);
            timer_counter_value_config(TIMER5, 0);
            timer_enable(TIMER5);
        } else {
            timer_disable(TIMER5); // Nothing to dim this cycle
        }
    }
}


void TIMER5_DAC_IRQHandler(void) {
    if (timer_interrupt_flag_get(TIMER5, TIMER_INT_FLAG_UP) == SET) {
        timer_interrupt_flag_clear(TIMER5, TIMER_INT_FLAG_UP);
                
        // 1. Add the triacs scheduled for this event to the active mask
        GIActiveMask |= GITimeline[GICurrentEventIdx].triac_mask;
        
        // 2. Output to bus
        SetDataBus((LastTriacValue & 0xE0) | GIActiveMask); 
        SetDRLine(false);
        StrobeTriac(); 
        DelayQuarterCycle();
        SetDRLine(true);

        // 3. Advance to the next event
        GICurrentEventIdx++;
        
        if (GICurrentEventIdx < GITimelineCount) {
            // Calculate the time remaining until the next event
            uint16_t next_delay = GITimeline[GICurrentEventIdx].delay_us - 
                                  GITimeline[GICurrentEventIdx - 1].delay_us;
            
            // Set the new interval and restart the timer
            timer_autoreload_value_config(TIMER5, next_delay);
            timer_counter_value_config(TIMER5, 0);
            // Timer remains enabled and continues counting up to next_delay
        } else {
            // Timeline finished for this AC half-cycle
            timer_disable(TIMER5);
        }
    }
}




int main(void) {
    SwitchTo240MHz();
    // Right after reset, before doing anything else, read
    // some real-time clock values
    volatile uint32_t rtc_time_reg = RTC_TIME;
    volatile uint32_t rtc_date_reg = RTC_DATE;
    BackupDomainInit(); // set up persistant storage between runs

    // LED Setup
    RCU_AHB1EN |= (1U << 4);              
    GPIO_CTL(GPIOE) &= ~(3U << (14 * 2)); 
    GPIO_CTL(GPIOE) |=  (1U << (14 * 2)); 
    GPIO_OMODE(GPIOE) &= ~(1U << 14);     
    GPIO_OSPD(GPIOE) |= (3U << (14 * 2)); 

    // Main Application
    GPIO_BOP(GPIOE) = (1U << 14); // turn the LED off

    InitProfilePins();

    MPUInit(); // RAM is cleared in this function
//    InitTimer0PWM(); // Turn on E and Q clock signals
    Init_DMA_Quadrature_Clocks(); // Turn on E and Q clock signals
    HPSDCardPortInit(); // Initialize the pins for the SD card
    RPU_WPC_SetupPorts();
//    SetBlanking(true); // This is temporary -- should be done through MPU
//    SetBlanking(false);
    EnableCycleCounter();

    if (RPU_WPC_DisplayInit()) {
        // Test Display RAM reads & writes
        // Set low page to 0
        WriteDisplay(WPC_DMD_LOW_PAGE, 0);
        WriteDisplay(WPC_DMD_HIGH_PAGE, 1);
        WriteDisplay(WPC_DMD_ACTIVE_PAGE, 0);

        RPU_WPC_DisplayShowLogoScreen();
        char buf[128];
        snprintf(buf, 128, "Homepin MPU\n");
        RPU_WPC_DisplayTextWithLogo(buf);
    }

    bool ROMIntegrityFailed = false;
    if (!HPSDCheckROMIntegrity()) {
        // Report an error and stop here
        ROMIntegrityFailed = true;
    }
    // The ROM is good -- we can get the name
    // with this:
    // GetROMName();

    MPUInitROM(HPSDGetROMPointer(), HPSDGetROMSize());
    MPUReset(true); 

    rtc_time_reg = RTC_TIME;
    rtc_date_reg = RTC_DATE;
    SetASICFromDateTimeRegisters(rtc_date_reg, rtc_time_reg);

    GPIO_BOP(GPIOC) = (1U << 0); // Turn off LED

    EnableCycleCounter();
    uint32_t lastTickCount = TwoMHzTicksSinceStart();
    uint32_t startingTickCount = TwoMHzTicksSinceStart();

    bool FIRQHasBeenTriggered = false;
    bool pauseEmulationForMenu = false;
    bool romInitComplete = false;
    bool soundCardChecked = false;
    bool speedrunTransitionComplete = false;

    uint32_t lastTimeUpdate = 0;

    if (ROMIntegrityFailed) {
        RPU_WPC_MenuStart();
        pauseEmulationForMenu = true;
    }

    uint32_t savedID = RTC_BKP2;
    if (savedID > 999) {
        savedID = 0; // Guard against garbage memory on a fresh MCU
    }
    SecurityPICSetLearnedID(savedID);

    if (ASICGetGIDimmingAlgorithm()==ASIC_GI_DIMMING_INTERRUPT) {
        ConfigZeroCrossInterrupt();
        ConfigGITimer();
        ASICUseSoftwareZeroCross(false);
    } else {
        ASICUseSoftwareZeroCross(true);
    }

    while (1) {
        //HPSoundCardUpdate();

        if (MPUDisplayHighPageOverride()) RPU_WPC_DisplayShowLogo(1, lastTickCount);
        uint32_t currentTickCount = TwoMHzTicksSinceStart();

        if (ASICRebootRequested()) {
            MPUInitROM(HPSDGetROMPointer(), HPSDGetROMSize());
            MPUReset(true);
            romInitComplete = false;
            lastTickCount = currentTickCount;
            RTC_BKP2 = SecurityPICGetLearnedID();
        }

        if (ASICGetBlanking()) {
            // also, check to see if the operator wants to 
            // boot to the special menu 
            if (!pauseEmulationForMenu && RPU_WPC_CheckForMenuRequest(true)) {
                pauseEmulationForMenu = true;
                RPU_WPC_MenuStart();
            }
            if (!soundCardChecked && (currentTickCount-startingTickCount)>CPU_TICKS_PER_SECOND) {
                soundCardChecked = true;
                if (!HPSoundCardPresent()) {
                    MPUUseLegacySoundCard(true);
                }
            }
        } else {
            if (!pauseEmulationForMenu && RPU_WPC_CheckForMenuRequest(false)) {
                pauseEmulationForMenu = true;
                RPU_WPC_MenuStart();
            }
            if (!romInitComplete) {
                romInitComplete = true;
            }
        }

        // Every minute, update the date & time on ASIC
        if (ASICHasDateTimeChanged()) SetDateTimeFromASIC();
        if ((currentTickCount - lastTimeUpdate >= (CPU_TICKS_PER_SECOND * 60))) {
            lastTimeUpdate = currentTickCount;
            rtc_time_reg = RTC_TIME;
            rtc_date_reg = RTC_DATE;
            SetASICFromDateTimeRegisters(rtc_date_reg, rtc_time_reg);            
        }

        int32_t ticksToRun;

        if (!speedrunTransitionComplete && !MPUBootSequenceFinished()) {
            ticksToRun = 1024; 
        } else {
            // CRITICAL SYNC: Prevent time-jump freeze the exact moment boot finishes
            if (!speedrunTransitionComplete) {
                lastTickCount = currentTickCount;
                speedrunTransitionComplete = true;
            }
            ticksToRun = (int32_t)(currentTickCount - lastTickCount);
        }
            
        if (!pauseEmulationForMenu) {
            if (ticksToRun > 0) {

                if (ticksToRun > 1024) ticksToRun = 1024;

                // Run the emulator for exactly that many ticks
                uint32_t ticksExecuted = MPUExecuteCycle(ticksToRun, ticksToRun);
                
                // Advance our "last run" marker by the amount we just executed
                lastTickCount += ticksExecuted;

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
            }
        } else {
            pauseEmulationForMenu = RPU_WPC_Menu(currentTickCount);

            if (!pauseEmulationForMenu) {
                if (ROMIntegrityFailed) {
                    pauseEmulationForMenu = true;
                    RPU_WPC_DisplayShowLogoScreen();
                    RPU_WPC_DisplayTextWithLogo("ROM not found");
                } else {
                    lastTickCount = currentTickCount;
                    RPU_WPC_DisplayShowLogoScreen();
                    RPU_WPC_DisplayTextWithLogo("Initializing\nROM...");
                    if (RPU_WPC_MenuRequiresReset()) {
                        MPUInitROM(HPSDGetROMPointer(), HPSDGetROMSize());
                        MPUReset(true);
                        romInitComplete = false;
                        lastTickCount = currentTickCount;
//                        SetBlanking(true);
                    }
                }
            } else {
                if ((currentTickCount/CPU_TICKS_PER_SECOND)%2) SetBoardLEDOn();
                else SetBoardLEDOff();
            }
        }

    }

    (void)rtc_time_reg;
    (void)rtc_date_reg;        
}
