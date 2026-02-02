//#include <stdio.h>
#include "mpu89.h"
#include "gd32f4xx.h"
#include "string.h"

//#define FAKE_DISPLAY_FIRQ
#define ROM_IN_C_FILE_ARRAY

#ifdef ROM_IN_C_FILE_ARRAY
uint8_t* GetROMPointer(void);
uint32_t GetROMSize(void);
#endif

//extern void initialise_monitor_handles(void);

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
    fmc_wscnt_set(WS_WSCNT_15); 

    /* 4. Configure PLL: 25MHz / 25 * 480 / 2 = 240MHz */
    /* Parameters: Source, PSC, N, P, Q */
    rcu_pll_config(RCU_PLLSRC_HXTAL, 25, 480, 2, 10);

    /* 5. Enable PLL and wait */
    rcu_osci_on(RCU_PLL_CK);
    while (ERROR == rcu_osci_stab_wait(RCU_PLL_CK));

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

#define RTC_PROOF_VALUE     0x3207  // stored in RTC_BKP0
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


// Define pointer to backup SRAM
#define BACKUP_SRAM_BASE    0x40024000
#define BACKUP_SRAM_SIZE    0x1000  // 4KB
#define BACKUP_SRAM_PROOF_VALUE     0xBC03  // stored in RTC_BKP0
void BackupRAM() {
    uint8_t *ramPtr = MPUGetNVRAMStart();
    uint16_t size = MPUGetNVRAMSize();
    uint8_t *backupRam = (uint8_t *)BACKUP_SRAM_BASE;
    memcpy(backupRam, ramPtr, size);        
}

void RestoreMPURAM() {
    // Check the register to see if RAM has been backed up
    if (RTC_BKP0==BACKUP_SRAM_PROOF_VALUE) {
        // Put the RAM back
        uint8_t *ramPtr = MPUGetNVRAMStart();
        uint16_t size = MPUGetNVRAMSize();
        uint8_t *backupRam = (uint8_t *)BACKUP_SRAM_BASE;
        memcpy(ramPtr, backupRam, size);
    }
}

// This handler is called when the power drops below 2.6 (?) V
// so we can remember anything important before the next run
// (this needs to be quick)
void LVD_IRQHandler(void) {
    if (SET == exti_interrupt_flag_get(EXTI_16)) {
        BackupRAM(); // store settings, audits, and other permanent information

        RTC_BKP0 = BACKUP_SRAM_PROOF_VALUE;  // Direct write

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
        case 1: rtcDOW = RTC_SUNDAY;
        case 2: rtcDOW = RTC_MONDAY;
        case 3: rtcDOW = RTC_TUESDAY;
        case 4: rtcDOW = RTC_WEDSDAY; // just had to guess at this spelling.
        case 5: rtcDOW = RTC_THURSDAY;
        case 6: rtcDOW = RTC_FRIDAY;
        case 7: rtcDOW = RTC_SATURDAY;
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
    uint8_t dow   = ((rtc_date_reg >> 13) & 0x7) +1;
    if (dow==8) dow = 1;
    uint8_t month = ((rtc_date_reg >> 12) & 0x1) * 10 + ((rtc_date_reg >> 8) & 0xF);
    uint8_t day   = ((rtc_date_reg >> 4) & 0x3) * 10 + (rtc_date_reg & 0xF);
    ASICSetCurrentDateTime(year, month, day, dow, hours, minutes);
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
    GPIO_BOP(GPIOE) = (1U << 14); // turn the LED on
    MPUInit(); // RAM is cleared in this function
    RestoreMPURAM();
    MPUSetROMAddress(GetROMPointer(), GetROMSize());
    ASICInit();
    CPUSetCallbacks(MPUWrite8, MPURead8);
    MPUReset();

    rtc_time_reg = RTC_TIME;
    rtc_date_reg = RTC_DATE;
    SetASICFromDateTimeRegisters(rtc_date_reg, rtc_time_reg);

    GPIO_BOP(GPIOC) = (1U << 0);

    EnableCycleCounter();
    InitTimer0PWM();
    uint32_t lastTickCount = TwoMHzTicksSinceStart();

    bool FIRQHasBeenTriggered = false;
//    bool RAMHasBeenBackedUp = false;
    uint32_t FIRQTriggeredTicks = 0;
//    uint32_t lastTimeRAMBackedUp = 0;
    uint32_t lastTimeUpdate = 0;

    while (1) {        
        uint32_t currentTickCount = TwoMHzTicksSinceStart(); 
        if (ASICGetBlanking()) {
            // run faster if we're still in blanking
            if (currentTickCount==lastTickCount) currentTickCount = lastTickCount + 1;
        }/* else {
            // save settings every 30 seconds
            if (currentTickCount>(lastTimeRAMBackedUp+60000000)) RAMHasBeenBackedUp = false;
            if (!RAMHasBeenBackedUp) {
                RAMHasBeenBackedUp = true;
                lastTimeRAMBackedUp = currentTickCount;
                BackupRAM();
            }
        }*/

        // Twice a minute (roughly) update the date & time on ASIC
        if ( (currentTickCount/8000000)!=lastTimeUpdate ) {
            lastTimeUpdate = (currentTickCount/8000000);
            if (ASICDateTimeChanged()) {
//                SetDateTimeFromASIC();
            } else {
                rtc_time_reg = RTC_TIME;
                rtc_date_reg = RTC_DATE;
                SetASICFromDateTimeRegisters(rtc_date_reg, rtc_time_reg);
            }
        }

        // Calculate how many 2MHz ticks have passed since we last checked
        int32_t ticksToRun = (int32_t)(currentTickCount - lastTickCount);

        if (ticksToRun > 0) {

            if (ticksToRun > 20000) ticksToRun = 20000;

            // Run the emulator for exactly that many ticks
            uint32_t ticksExecuted = MPUExecuteCycle(ticksToRun, 1);

            // Advance our "last run" marker by the amount we just executed
            lastTickCount += ticksExecuted;

            if ((currentTickCount/100000) & 0x01) GPIO_BC(GPIOE) = (1U << 14);
            else GPIO_BOP(GPIOE) = (1U << 14);

            if (FIRQTriggered()) {
                if (FIRQHasBeenTriggered==false) {
                    // We haven't fired this FIRQ yet
                    FIRQHasBeenTriggered = true;
                    FIRQTriggeredTicks = currentTickCount;
                    ASICFirqSourceDmd(true);
                    MPUFIRQ();
                } else if (currentTickCount>(FIRQTriggeredTicks+5)) {
//                    FIRQHasBeenTriggered = false;
                }
            } else {
                // Reset for next FIRQ
                FIRQHasBeenTriggered = false;
            }
        }

    }

    (void)rtc_time_reg;
    (void)rtc_date_reg;        
}
