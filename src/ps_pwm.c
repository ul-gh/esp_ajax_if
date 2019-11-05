/* Use Espressif ESP32 platform MCPWM hardware module for output of
 * Phase-Shift-PWM waveform on 4 hardware pins.
 * 
 * Application in ZVS-PS-PWM, DAB-DCM and LLC power electronics converters.
 * 
 * This depends on the ESP-IDF SDK
 *
 * 2019-10-28 Ulrich Lukas
 */
#include "freertos/FreeRTOS.h"
#include "soc/mcpwm_struct.h"
#include "ps_pwm.h"

// MMAP IO using direct register access. See comments in mcpwm_struct.h
static mcpwm_dev_t *MCPWM[2] = {&MCPWM0, &MCPWM1};
// Atomic register access
static portMUX_TYPE mcpwm_spinlock = portMUX_INITIALIZER_UNLOCKED;

/* Set frequency and duty cycle of a single timer/PWM operator
 * for using only the PWMxA output signal.
 * 
 * Symmetric output signal with independently configurable
 * rising- and falling edge dead time vlaues can be achieved
 * by using the dead-band generator hardware module, using
 * function set_deadtime.
 * 
 * Duty cycle of the result output waveform is set via the
 * phase-shift value using the pspwm_set_ps_duty() function.
 * 
 * This does not alter prescaler settings.
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float frequency: Frequency of the non-rectified waveform in Hz,
 * float lead_red: dead time value for rising edge, leading leg
 * float lead_fed: dead time value for falling edge, leading leg
 * float lag_red: dead time value for rising edge, lagging leg
 * float lag_fed: dead time value for falling edge, lagging leg 
 */
esp_err_t pspwm_individual_dt_set_frequency(
        mcpwm_unit_t mcpwm_num, float frequency, float lead_red, float lead_fed,
        float lag_red, float lag_fed)
{
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    uint32_t timer_top = (uint32_t)(
            MCPWM_CLK / (frequency * (TIMER_CLK_PRESCALE + 1))
            );
    uint32_t cmpr_0_a = (uint32_t)(
            (timer_top + frequency * (lead_red-lead_fed) * timer_top) / 2
            );
    uint32_t cmpr_1_a = (uint32_t)(
            (timer_top + frequency * (lag_red-lag_fed) * timer_top) / 2
            );
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
    module->timer[MCPWM_TIMER_0].period.period = timer_top;
    module->timer[MCPWM_TIMER_1].period.period = timer_top;
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_0_a;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_1_a;
    DBG("Period register: %d", timer_top);
    DBG("Set Duty: cmpr_0_a: %d", cmpr_0_a);
    DBG("Set Duty: cmpr_1_a: %d", cmpr_1_a);
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}

/* Set frequency and duty cycle of each of the four output
 * signals used for phase-shift-PWM when using the timer
 * in up/down counting mode and generating complementary signals
 * with identical values for rising-edge and falling-edge dead time
 * (i.e. by setting the duty cylcle per output leg - leg-duty).
 * 
 * Duty cycle of the result output waveform is set via the
 * phase-shift value using the pspwm_set_ps_duty() function.
 * 
 * This does not alter prescaler settings.
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float frequency: Frequency of the non-rectified waveform in Hz,
 * float leg_duty: bridge-leg duty cycle (0..1)
 */
esp_err_t pspwm_symmetric_dt_set_frequency(
        mcpwm_unit_t mcpwm_num, float frequency, float leg_duty)
{
    uint32_t timer_top = (uint32_t)(
            MCPWM_CLK / (frequency * 2 * (TIMER_CLK_PRESCALE + 1)));
    uint32_t cmpr_a = (uint32_t)(timer_top * leg_duty);
    uint32_t cmpr_b = (uint32_t)(timer_top * (1.0 - leg_duty));
    portENTER_CRITICAL(&mcpwm_spinlock);
    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Register 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        MCPWM[mcpwm_num]->timer[timer_i].period.period = timer_top;
        // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
        // also for GEN1 with different register offset
        MCPWM[mcpwm_num]->channel[timer_i].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_a;
        // Register 16.18: PWM_GEN0_TSTMP_B_REG (0x0044) etc.
        // also for GEN1 with different register offset
        MCPWM[mcpwm_num]->channel[timer_i].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_b;
        DBG("Timer %d in set freqency: period register: %d", timer_i, timer_top);
        DBG("Set Duty: cmpr_a: %d", cmpr_a);
        DBG("Set Duty: cmpr_b: %d", cmpr_b);
    }
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}

/* Set PS-PWM phase shift based on the current period time setting
 * i.e. state of the PWM hardware "period" register.
 * 
 * The phase-shift value is valid for the individual deadtime setting.
 *
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_individual_dt_set_ps_duty(mcpwm_unit_t mcpwm_num, float ps_duty)
{
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Phase shift value is based on timer 0 period setting but intentionally
    // only set for timer 1. Timer 0 is the reference phase.
    uint32_t curr_period = MCPWM0.timer[MCPWM_TIMER_0].period.period;
    uint32_t phase_setval = (uint32_t)(curr_period * ps_duty / 2);
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

/* Set PS-PWM phase shift based on the current period time setting
 * i.e. state of the PWM hardware "period" register.
 *
 * The phase-shift value is valid for the symmetric dead-time setting
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_symmetric_dt_set_ps_duty(mcpwm_unit_t mcpwm_num, float ps_duty)
{
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Phase shift value is based on timer 0 period setting but intentionally
    // only set for timer 1. Timer 0 is the reference phase.
    uint32_t curr_period = MCPWM0.timer[MCPWM_TIMER_0].period.period;
    uint32_t phase_setval = (uint32_t)(curr_period * ps_duty);
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

/* Set deadtime values individually for leading leg rising and faling edge
 * as well as for lagging leg rising and falling edge.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float lead_red: dead time value for rising edge, leading leg
 * float lead_fed: dead time value for falling edge, leading leg
 * float lag_red: dead time value for rising edge, lagging leg
 * float lag_fed: dead time value for falling edge, lagging leg
 */
esp_err_t pspwm_set_individual_deadtimes(
        mcpwm_unit_t mcpwm_num, float lead_red, float lead_fed,
        float lag_red, float lag_fed)
{
    uint32_t lead_red_reg = (uint32_t)(lead_red * MCPWM_CLK);
    uint32_t lead_fed_reg = (uint32_t)(lead_fed * MCPWM_CLK);
    uint32_t lag_red_reg = (uint32_t)(lag_red * MCPWM_CLK);
    uint32_t lag_fed_reg = (uint32_t)(lag_fed * MCPWM_CLK);
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.25: PWM_DT0_RED_CFG_REG (0x0060) etc.
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].db_red_cfg.red = lead_red_reg;
    // Register 16.24: PWM_DT0_FED_CFG_REG (0x005c) etc.
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].db_fed_cfg.fed = lead_fed_reg;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].db_red_cfg.red = lag_red_reg;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].db_fed_cfg.fed = lag_fed_reg;
    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Register 16.23: PWM_DT0_CFG_REG (0x0058) etc.
        MCPWM[mcpwm_num]->channel[timer_i].db_cfg.fed_upmethod = 1; // TEZ
        MCPWM[mcpwm_num]->channel[timer_i].db_cfg.red_upmethod = 1; // TEZ
        MCPWM[mcpwm_num]->channel[timer_i].db_cfg.clk_sel = 0; // MCPWM_CLK
        MCPWM[mcpwm_num]->channel[timer_i].db_cfg.b_outbypass = 0; //S0
        MCPWM[mcpwm_num]->channel[timer_i].db_cfg.a_outbypass = 0; //S1
        MCPWM[mcpwm_num]->channel[timer_i].db_cfg.red_outinvert = 0; //S2
        MCPWM[mcpwm_num]->channel[timer_i].db_cfg.fed_outinvert = 1; //S3
        // MCPWM[mcpwm_num]->channel[timer_i].db_cfg.red_insel = 0; //S4
        // MCPWM[mcpwm_num]->channel[timer_i].db_cfg.fed_insel = 0; //S5
        // MCPWM[mcpwm_num]->channel[timer_i].db_cfg.a_outswap = 0; //S6
        // MCPWM[mcpwm_num]->channel[timer_i].db_cfg.b_outswap = 0; //S7
        // MCPWM[mcpwm_num]->channel[timer_i].db_cfg.deb_mode = 0;  //S8
    }
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}

/* Initialize ESP32 MCPWM hardware module for output of
 * Phase-Shift-PWM waveform on 4 hardware pins.
 * 
 * This assumes symmetric deadtime values i.e. by specification of a
 * bridge-leg duty cycle.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float frequency: Frequency of the non-rectified waveform in Hz,
 * float leg_duty: bridge-leg duty cycle (0..1),
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_init_symmetric_deadtime(
        mcpwm_unit_t mcpwm_num, float frequency, float ps_duty, float leg_duty)
{
    periph_module_enable(PERIPH_PWM0_MODULE + mcpwm_num);
    pspwm_symmetric_dt_set_frequency(mcpwm_num, frequency, leg_duty);
    pspwm_symmetric_dt_set_ps_duty(mcpwm_num, ps_duty);

    portENTER_CRITICAL(&mcpwm_spinlock);
    // Timer and deadtime units clock prescaler/divider configuration
    // Datasheet 16.1: PWM_CLK_CFG_REG (0x0000)
    MCPWM[mcpwm_num]->clk_cfg.prescale = BASE_CLK_PRESCALE;

    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        MCPWM[mcpwm_num]->timer[timer_i].period.prescale = TIMER_CLK_PRESCALE;
        // Set first bit => update at timer equals zero
        MCPWM[mcpwm_num]->timer[timer_i].period.upmethod = 1;
        // Datasheet 16.3: PWM_TIMER0_CFG1_REG (0x0008) etc.
        MCPWM[mcpwm_num]->timer[timer_i].mode.mode = MCPWM_UP_DOWN_COUNTER;
        // Update/swap shadow registers at timer equals zero
        // Datasheet 16.16: PWM_GEN0_STMP_CFG_REG (0x003c) etc.
        MCPWM[mcpwm_num]->channel[timer_i].cmpr_cfg.a_upmethod = 1;
        MCPWM[mcpwm_num]->channel[timer_i].cmpr_cfg.b_upmethod = 1;
        // 2 => Set output high; 1 => set output low
        // Datasheet 16.21: PWM_GEN0_A_REG (0x0050) etc.
        MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_A].utez = 2;
        MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_A].utea = 1;
        MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_A].dtea = 2;
        // Datasheet 16.21: PWM_GEN0_B_REG (0x0054) etc.
        MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_B].utez = 1;
        MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_B].uteb = 2;
        MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_B].dteb = 1;
    }
    // Datasheet 16.15: PWM_OPERATOR_TIMERSEL_REG (0x0038)
    MCPWM[mcpwm_num]->timer_sel.operator0_sel = 0;
    MCPWM[mcpwm_num]->timer_sel.operator1_sel = 1;
    // MCPWM[mcpwm_num]->timer_sel.operator2_sel = 2;
    // SYNC input coupling setup: Timer 1 input coupled to timer 0 sync output
    // Datasheet 16.14: PWM_TIMER_SYNCI_CFG_REG (0x0034)
    MCPWM[mcpwm_num]->timer_synci_cfg.t0_in_sel = 0; // None
    MCPWM[mcpwm_num]->timer_synci_cfg.t1_in_sel = 1; // timer0 sync out
    // SYNC input and output configuration for both timers
    // Datasheet 16.4: PWM_TIMER0_SYNC_REG (0x000c)
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].sync.in_en = 0; // Off
    // Generate sync output at timer equals zero of first timer
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].sync.out_sel = 2;
    // Second timer is synchronized to first timer
    // Datasheet 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.in_en = 1; // On
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.out_sel = 3; // Off
    // Start continuously running mode
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].mode.start = 2;
    // Start continuously running mode
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].mode.start = 2;
    // Force update on all registers for settings to take effect
    // Datasheet 16.68: PWM_UPDATE_CFG_REG (0x010c)
    MCPWM[mcpwm_num]->update_cfg.global_up_en = 1;
    MCPWM[mcpwm_num]->update_cfg.global_force_up = 1;
    MCPWM[mcpwm_num]->update_cfg.global_force_up = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}

/* Initialize ESP32 MCPWM hardware module for output of
 * Phase-Shift-PWM waveform on 4 hardware pins.
 * 
 * This sets individual dead-time values for rising and falling edges for
 * all four PWM outputs.
 * 
 * Combined output waveform of the phase-shifted full-bridge
 * is symmetric nevertheless.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float frequency: Frequency of the non-rectified waveform in Hz,
 * float lead_red: dead time value for rising edge, leading leg
 * float lead_fed: dead time value for falling edge, leading leg
 * float lag_red: dead time value for rising edge, lagging leg
 * float lag_fed: dead time value for falling edge, lagging leg 
 */
esp_err_t pspwm_init_individual_deadtimes(
        mcpwm_unit_t mcpwm_num, float frequency, float ps_duty,
        float lead_red, float lead_fed, float lag_red, float lag_fed)
{
    periph_module_enable(PERIPH_PWM0_MODULE + mcpwm_num);
    pspwm_individual_dt_set_frequency(
        mcpwm_num, frequency, lead_red, lead_fed, lag_red, lag_fed);
    pspwm_set_individual_deadtimes(
        mcpwm_num, lead_red, lead_fed, lag_red, lag_fed);
    pspwm_individual_dt_set_ps_duty(mcpwm_num, ps_duty);

    portENTER_CRITICAL(&mcpwm_spinlock);
    // Timer and deadtime units clock prescaler/divider configuration
    // Datasheet 16.1: PWM_CLK_CFG_REG (0x0000)
    MCPWM[mcpwm_num]->clk_cfg.prescale = BASE_CLK_PRESCALE;

    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        MCPWM[mcpwm_num]->timer[timer_i].period.prescale = TIMER_CLK_PRESCALE;
        // Set first bit => update at timer equals zero
        MCPWM[mcpwm_num]->timer[timer_i].period.upmethod = 1;
        // Datasheet 16.3: PWM_TIMER0_CFG1_REG (0x0008) etc.
        MCPWM[mcpwm_num]->timer[timer_i].mode.mode = MCPWM_UP_COUNTER;
        // Update/swap shadow registers at timer equals zero
        // Datasheet 16.16: PWM_GEN0_STMP_CFG_REG (0x003c) etc.
        MCPWM[mcpwm_num]->channel[timer_i].cmpr_cfg.a_upmethod = 1;
        // MCPWM[mcpwm_num]->channel[timer_i].cmpr_cfg.b_upmethod = 1;
        // 2 => Set output high; 1 => set output low
        // Datasheet 16.21: PWM_GEN0_A_REG (0x0050) etc.
        MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_A].utez = 2;
        MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_A].utea = 1;
        // MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_A].dtea = 2;
        // Datasheet 16.21: PWM_GEN0_B_REG (0x0054) etc.
        // MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_B].utez = 1;
        // MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_B].uteb = 2;
        // MCPWM[mcpwm_num]->channel[timer_i].generator[MCPWM_OPR_B].dteb = 1;
    }
    // Datasheet 16.15: PWM_OPERATOR_TIMERSEL_REG (0x0038)
    MCPWM[mcpwm_num]->timer_sel.operator0_sel = 0;
    MCPWM[mcpwm_num]->timer_sel.operator1_sel = 1;
    // MCPWM[mcpwm_num]->timer_sel.operator2_sel = 2;
    // SYNC input coupling setup: Timer 1 input coupled to timer 0 sync output
    // Datasheet 16.14: PWM_TIMER_SYNCI_CFG_REG (0x0034)
    MCPWM[mcpwm_num]->timer_synci_cfg.t0_in_sel = 0; // None
    MCPWM[mcpwm_num]->timer_synci_cfg.t1_in_sel = 1; // timer0 sync out
    // SYNC input and output configuration for both timers
    // Datasheet 16.4: PWM_TIMER0_SYNC_REG (0x000c)
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].sync.in_en = 0; // Off
    // Generate sync output at timer equals zero of first timer
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].sync.out_sel = 2;
    // Second timer is synchronized to first timer
    // Datasheet 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.in_en = 1; // On
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.out_sel = 3; // Off
    // Start continuously running mode
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].mode.start = 2;
    // Start continuously running mode
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].mode.start = 2;
    // Force update on all registers for settings to take effect
    // Datasheet 16.68: PWM_UPDATE_CFG_REG (0x010c)
    MCPWM[mcpwm_num]->update_cfg.global_up_en = 1;
    MCPWM[mcpwm_num]->update_cfg.global_force_up = 1;
    MCPWM[mcpwm_num]->update_cfg.global_force_up = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}


void pspwm_setup_gpios(mcpwm_unit_t mcpwm_num,
                       uint32_t lead_a, uint32_t lead_b,
                       uint32_t lag_a, uint32_t lag_b)
{
    INFO("Initializing GPIOs for PS-PWM");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, lead_a);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, lead_b);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, lag_a);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, lag_b);
    //gpio_pulldown_en(GPIO_SYNC0_IN);   //Enable pull down on SYNC0  signal
}
