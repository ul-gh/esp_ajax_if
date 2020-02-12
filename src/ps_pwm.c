/* Use Espressif ESP32 platform MCPWM hardware module for generating
 * Phase-Shift-PWM waveform on 4 hardware pins.
 *
 * Application in ZVS-PS-PWM, DAB-DCM and LLC power electronics converters.
 * 
 * This depends on the ESP-IDF SDK
 *
 * 2020-01-31 Ulrich Lukas
 */
#include "freertos/FreeRTOS.h"
#include "soc/mcpwm_struct.h"

#include "ps_pwm.h"
#define LOG_LOCAL_LEVEL PS_PWM_LOG_LEVEL
#include "esp_log.h"

#define DBG(...) ESP_LOGD("Module: ps_pwm.c", __VA_ARGS__)
#define INFO(...) ESP_LOGI("Module: ps_pwm.c", __VA_ARGS__)
#define ERROR(...) ESP_LOGE("Module: ps_pwm.c", __VA_ARGS__)


// MMAP IO using direct register access.
// External definition and declaration in mcpwm_struct.h
static mcpwm_dev_t* const MCPWM[2] = {&MCPWM0, &MCPWM1};
// Atomic register access uses spinlock polling via "portENTER_CRITICAL()" macro
static portMUX_TYPE mcpwm_spinlock = portMUX_INITIALIZER_UNLOCKED;
// Setpoint values need to be static because of timing settings
// between frequency, phase and dead-time all depend on each other.
static pspwm_setpoint_t* s_setpoints[2] = {NULL, NULL};

// Timer clock settings are common to both MCPWM stages
static pspwm_clk_conf_t s_clk_conf = {
    .base_clk_prescale = BASE_CLK_PRESCALE_DEFAULT,
    .timer_clk_prescale = TIMER_CLK_PRESCALE_DEFAULT,
    .base_clk = (float) MCPWM_INPUT_CLK / BASE_CLK_PRESCALE_DEFAULT,
    .timer_clk = (float) MCPWM_INPUT_CLK / (
            BASE_CLK_PRESCALE_DEFAULT * TIMER_CLK_PRESCALE_DEFAULT)
};
// Calculated setpoint limits
static pspwm_setpoint_limits_t* s_setpoint_limits[2] = {NULL, NULL};
// Set to true by interrupt handler when OST fault event is triggered
static bool ost_fault_event_occurred[2] = {false, false};

// Not part of external API
static void _pspwm_up_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num);
static void _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num);
static void _pspwm_setup_fault_handler_module(
        mcpwm_unit_t mcpwm_num,
        mcpwm_action_on_pwmxa_t disable_action_lag_leg,
        mcpwm_action_on_pwmxa_t disable_action_lead_leg);
static void IRAM_ATTR pspwm_unit0_isr_handler(void* arg);

/**********************************************************************
 *    FULL-SPEED-MODE, 4x INDIVIDUAL DEAD-TIME, HW-DEAD-TIME-MODULE
 **********************************************************************
 */
esp_err_t pspwm_up_ctr_mode_init(mcpwm_unit_t mcpwm_num,
                                 const int gpio_lead_a, const int gpio_lead_b,
                                 const int gpio_lag_a, const int gpio_lag_b,
                                 const float frequency,
                                 const float ps_duty,
                                 const float lead_red, const float lead_fed,
                                 const float lag_red, const float lag_fed,
                                 const bool output_enabled,
                                 mcpwm_action_on_pwmxa_t disable_action_lag_leg,
                                 mcpwm_action_on_pwmxa_t disable_action_lead_leg)
{
    DBG("Call pspwm_up_ctr_mode_init");
    if (mcpwm_num != MCPWM_UNIT_0 && mcpwm_num != MCPWM_UNIT_1) {
        ERROR("mcpwm_num must be MCPWM_UNIT_0 or MCPWM_UNIT_1!");
        return ESP_FAIL;
    }
    if (!s_setpoints[mcpwm_num]) {
        s_setpoints[mcpwm_num] = malloc(sizeof(pspwm_setpoint_t));
        if (!s_setpoints[mcpwm_num]) {
            ERROR("Malloc failure!");
            return ESP_FAIL;
        }
    }
    if (!s_setpoint_limits[mcpwm_num]) {
        s_setpoint_limits[mcpwm_num] = malloc(sizeof(pspwm_setpoint_limits_t));
        if (!s_setpoint_limits[mcpwm_num]) {
            ERROR("Malloc failure!");
            return ESP_FAIL;
        }
    }
    s_setpoint_limits[mcpwm_num]->frequency_min = s_clk_conf.timer_clk / UINT16_MAX;
    s_setpoint_limits[mcpwm_num]->frequency_max = s_clk_conf.timer_clk / period_min;
    if (UINT16_MAX/s_clk_conf.base_clk < 1.0 / frequency) {
        s_setpoint_limits[mcpwm_num]->dt_sum_max = UINT16_MAX / s_clk_conf.base_clk;
    } else {
        s_setpoint_limits[mcpwm_num]->dt_sum_max = 1.0 / frequency;
    }
    DBG("frequency_min is now: %g", s_setpoint_limits[mcpwm_num]->frequency_min);
    DBG("frequency_max is now: %g", s_setpoint_limits[mcpwm_num]->frequency_max);
    DBG("dt_sum_max is now: %g", s_setpoint_limits[mcpwm_num]->dt_sum_max);
    // This is a 16-Bit timer register, although the API struct uses uint32_t...
    if (frequency <= s_setpoint_limits[mcpwm_num]->frequency_min
        || frequency > s_setpoint_limits[mcpwm_num]->frequency_max) {
            ERROR("Frequency setpoint out of range!");
            return ESP_FAIL;
    }
    if (ps_duty < 0 || ps_duty > 1) {
        ERROR("Invalid setpoint value for ps_duty");
        return ESP_FAIL;
    }
    if (lead_red < 0 || lead_fed < 0 || lag_red < 0 || lag_fed < 0
            || lead_red + lead_fed >= s_setpoint_limits[mcpwm_num]->dt_sum_max
            || lag_red + lag_fed >= s_setpoint_limits[mcpwm_num]->dt_sum_max) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    s_setpoints[mcpwm_num]->frequency = frequency;
    s_setpoints[mcpwm_num]->ps_duty = ps_duty;
    s_setpoints[mcpwm_num]->lead_red = lead_red;
    s_setpoints[mcpwm_num]->lead_fed = lead_fed;
    s_setpoints[mcpwm_num]->lag_red = lag_red;
    s_setpoints[mcpwm_num]->lag_fed = lag_fed;
    s_setpoints[mcpwm_num]->output_enabled = output_enabled;
    periph_module_enable(PERIPH_PWM0_MODULE + mcpwm_num);
    // Basic setup for PS_PWM in up/down counting mode
    _pspwm_up_ctr_mode_register_base_setup(mcpwm_num);
    // Setup the fault handler module as this is required for disabling the outputs
    _pspwm_setup_fault_handler_module(mcpwm_num,
                                      disable_action_lag_leg,
                                      disable_action_lead_leg);
    // Continue by setting a Fault Event forcing the GPIOs to defined "OFF" state
    bool is_ok = pspwm_disable_output(mcpwm_num) == ESP_OK;
    is_ok &= mcpwm_gpio_init(mcpwm_num, MCPWM0A, gpio_lead_a) == ESP_OK;
    is_ok &= mcpwm_gpio_init(mcpwm_num, MCPWM0B, gpio_lead_b) == ESP_OK;
    is_ok &= mcpwm_gpio_init(mcpwm_num, MCPWM1A, gpio_lag_a) == ESP_OK;
    is_ok &= mcpwm_gpio_init(mcpwm_num, MCPWM1B, gpio_lag_b) == ESP_OK;
    is_ok &= pspwm_up_ctr_mode_set_frequency(mcpwm_num, frequency) == ESP_OK;
    is_ok &= pspwm_up_ctr_mode_set_deadtimes(
            mcpwm_num, lead_red, lead_fed, lag_red, lag_fed) == ESP_OK;
    is_ok &= pspwm_up_ctr_mode_set_ps_duty(mcpwm_num, ps_duty) == ESP_OK;
    if (output_enabled) {
        is_ok &= pspwm_resync_enable_output(mcpwm_num) == ESP_OK;
    }
    if (is_ok) {
        DBG("pspwm_up_ctr_mode_init OK!");
        return ESP_OK;
    } else {
        ERROR("pspwm_up_ctr_mode_init failed!");
        return ESP_FAIL;
    }
}

esp_err_t pspwm_up_ctr_mode_set_frequency(mcpwm_unit_t mcpwm_num, 
                                          const float frequency)
{
    DBG("Call pspwm_up_ctr_mode_set_frequency");
    // PWM hardware must have been initialised first
    pspwm_setpoint_t* setpoints = s_setpoints[mcpwm_num];
    assert(setpoints != NULL);
    // This is a 16-Bit timer register, although the API struct uses uint32_t...
    if (frequency <= s_setpoint_limits[mcpwm_num]->frequency_min
        || frequency > s_setpoint_limits[mcpwm_num]->frequency_max) {
            ERROR("Frequency setpoint out of range!");
            return ESP_FAIL;
    }
    // Set global state
    setpoints->frequency = frequency;
    float half_period = 0.5 * s_clk_conf.timer_clk / frequency;
    uint32_t timer_top = (uint32_t)(2 * half_period) - 1;
    uint32_t cmpr_0_a = (uint32_t)(
        half_period
        + 0.5 * (s_clk_conf.timer_clk * (setpoints->lead_red 
                                         - setpoints->lead_fed)));
    uint32_t cmpr_1_a = (uint32_t)(
        half_period
        + 0.5 * (s_clk_conf.timer_clk * (setpoints->lag_red
                                         - setpoints->lag_fed)));
    // Phase shift value for Timer 1 needs updating when changing frequency.
    // Timer 0 is the reference phase and needs no update.
    uint32_t phase_setval = (uint32_t)(
        half_period * setpoints->ps_duty);
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_0_a;
    // Register 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
    module->timer[MCPWM_TIMER_0].period.period = timer_top;
    // Same for timer 1
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_1_a;
    module->timer[MCPWM_TIMER_1].period.period = timer_top;
    // Phase shift value is based on timer 0 period setting but intentionally
    // only set for timer 1. Timer 0 is the reference phase.
    // Register 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    module->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Timer TOP is now: %d", timer_top);
    DBG("cmpr_0_a register value: %d", cmpr_0_a);
    DBG("cmpr_1_a register value: %d", cmpr_1_a);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

esp_err_t pspwm_up_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                          const float lead_red, const float lead_fed,
                                          const float lag_red, const float lag_fed)
{
    DBG("Call pspwm_up_ctr_mode_set_deadtimes()");
    // PWM hardware must be initialised first
    pspwm_setpoint_t* setpoints = s_setpoints[mcpwm_num];
    assert(setpoints != NULL);
    // PWM base period and duty cycle must be adjusted when changing dead-times
    if (lead_red < 0 || lead_fed < 0 || lag_red < 0 || lag_fed < 0
            || lead_red + lead_fed >= s_setpoint_limits[mcpwm_num]->dt_sum_max
            || lag_red + lag_fed >= s_setpoint_limits[mcpwm_num]->dt_sum_max) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    // Static variables needed when changing PWM frequency or other settings
    // which depend on dead-time values
    setpoints->lead_red = lead_red;
    setpoints->lead_fed = lead_fed;
    setpoints->lag_red = lag_red;
    setpoints->lag_fed = lag_fed;
    uint32_t lead_red_reg = (uint32_t)(lead_red * s_clk_conf.base_clk);
    uint32_t lead_fed_reg = (uint32_t)(lead_fed * s_clk_conf.base_clk);
    uint32_t lag_red_reg = (uint32_t)(lag_red * s_clk_conf.base_clk);
    uint32_t lag_fed_reg = (uint32_t)(lag_fed * s_clk_conf.base_clk);
    float half_period = 0.5 * s_clk_conf.timer_clk / setpoints->frequency;
    //float half_period = 0.5 * (module->timer[MCPWM_TIMER_0].period.period + 1);
    DBG("Limit value for (red + fed) in ns: %f",
        (1e9*2*half_period - 1) / s_clk_conf.base_clk);
    uint32_t cmpr_0_a = (uint32_t)(
        half_period
        + 0.5 * (s_clk_conf.timer_clk * (setpoints->lead_red
                                         - setpoints->lead_fed)));
    uint32_t cmpr_1_a = (uint32_t)(
        half_period
        + 0.5 * (s_clk_conf.timer_clk * (setpoints->lag_red
                                         - setpoints->lag_fed)));
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.25: PWM_DT0_RED_CFG_REG (0x0060) etc.
    module->channel[MCPWM_TIMER_0].db_red_cfg.red = lead_red_reg;
    // Register 16.24: PWM_DT0_FED_CFG_REG (0x005c) etc.
    module->channel[MCPWM_TIMER_0].db_fed_cfg.fed = lead_fed_reg;
    module->channel[MCPWM_TIMER_1].db_red_cfg.red = lag_red_reg;
    module->channel[MCPWM_TIMER_1].db_fed_cfg.fed = lag_fed_reg;
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_0_a;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_1_a;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("cmpr_0_a register value: %d", cmpr_0_a);
    DBG("cmpr_1_a register value: %d", cmpr_1_a);
    DBG("Dead time registers for LEAD set to: %d (rising edge), %d (falling edge)",
        lead_red_reg, lead_fed_reg);
    DBG("Dead time registers for LAG set to: %d (rising edge), %d (falling edge)",
        lag_red_reg, lag_fed_reg);

    return ESP_OK;
}

esp_err_t pspwm_up_ctr_mode_set_ps_duty(mcpwm_unit_t mcpwm_num, const float ps_duty)
{
    DBG("Call pspwm_up_ctr_mode_set_ps_duty");
    if (ps_duty < 0 || ps_duty > 1) {
        ERROR("Invalid setpoint value for ps_duty");
        return ESP_FAIL;
    }
    // Set global state
    pspwm_setpoint_t* setpoints = s_setpoints[mcpwm_num];
    assert(setpoints != NULL);
    setpoints->ps_duty = ps_duty;
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // For up-counting mode, output waveform period is actually timer TOP + 1
    uint32_t curr_period = module->timer[MCPWM_TIMER_0].period.period + 1;
    uint32_t phase_setval = (uint32_t)(curr_period * ps_duty * 0.5);
    // Phase shift value is based on timer 0 period setting but intentionally
    // only set for timer 1. Timer 0 is the reference phase.
    // Register 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    module->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

void _pspwm_up_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num) {
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Timer and deadtime module clock prescaler/divider configuration
    // Datasheet 16.1: PWM_CLK_CFG_REG (0x0000)
    // Hardware prescales by register value plus one, thus subtracting it here
    module->clk_cfg.prescale = s_clk_conf.base_clk_prescale - 1;

    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        // Hardware prescales by register value plus one, thus subtracting it here
        module->timer[timer_i].period.prescale = s_clk_conf.timer_clk_prescale - 1;
        // Datasheet 16.3: PWM_TIMER0_CFG1_REG (0x0008) etc.
        module->timer[timer_i].mode.mode = MCPWM_UP_COUNTER;
        // 2 => Set output high; 1 => set output low
        // Datasheet 16.21: PWM_GEN0_A_REG (0x0050) etc.
        module->channel[timer_i].generator[MCPWM_OPR_A].utez = 2;
        module->channel[timer_i].generator[MCPWM_OPR_A].utea = 1;
        // module->channel[timer_i].generator[MCPWM_OPR_A].dtea = 2;
        // Datasheet 16.21: PWM_GEN0_B_REG (0x0054) etc.
        // module->channel[timer_i].generator[MCPWM_OPR_B].utez = 1;
        // module->channel[timer_i].generator[MCPWM_OPR_B].uteb = 2;
        // module->channel[timer_i].generator[MCPWM_OPR_B].dteb = 1;
        /* Dead-Band Generator Set-Up
         */
        // Register 16.23: PWM_DT0_CFG_REG (0x0058) etc.
        module->channel[timer_i].db_cfg.clk_sel = 0; // MCPWM_BASE_CLK (PWM_clk)
        module->channel[timer_i].db_cfg.b_outbypass = 0; //S0
        module->channel[timer_i].db_cfg.a_outbypass = 0; //S1
        module->channel[timer_i].db_cfg.red_outinvert = 0; //S2
        module->channel[timer_i].db_cfg.fed_outinvert = 1; //S3
        // module->channel[timer_i].db_cfg.red_insel = 0; //S4
        // module->channel[timer_i].db_cfg.fed_insel = 0; //S5
        // module->channel[timer_i].db_cfg.a_outswap = 0; //S6
        // module->channel[timer_i].db_cfg.b_outswap = 0; //S7
        // module->channel[timer_i].db_cfg.deb_mode = 0;  //S8
    }
    // Update/swap shadow registers at timer equals zero for timer0,
    // update at sync for timer1.
    // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
    module->timer[MCPWM_TIMER_0].period.upmethod = 1; // TEZ
    module->timer[MCPWM_TIMER_1].period.upmethod = 2; // Literal 2 correct: At sync
    // Datasheet 16.16: PWM_GEN0_STMP_CFG_REG (0x003c) etc.
    module->channel[MCPWM_TIMER_0].cmpr_cfg.a_upmethod = 1; // TEZ
    module->channel[MCPWM_TIMER_0].cmpr_cfg.b_upmethod = 1; // TEZ
    module->channel[MCPWM_TIMER_1].cmpr_cfg.a_upmethod = 1ul<<2; // At sync
    module->channel[MCPWM_TIMER_1].cmpr_cfg.b_upmethod = 1ul<<2; // At sync
    // Register 16.23: PWM_DT0_CFG_REG (0x0058) etc.
    module->channel[MCPWM_TIMER_0].db_cfg.fed_upmethod = 1; // TEZ
    module->channel[MCPWM_TIMER_1].db_cfg.red_upmethod = 1ul<<2; // At sync
    // Datasheet 16.15: PWM_OPERATOR_TIMERSEL_REG (0x0038)
    module->timer_sel.operator0_sel = 0;
    module->timer_sel.operator1_sel = 1;
    // module->timer_sel.operator2_sel = 2;
    // SYNC input coupling setup: Timer 1 input coupled to timer 0 sync output
    // Datasheet 16.14: PWM_TIMER_SYNCI_CFG_REG (0x0034)
    module->timer_synci_cfg.t0_in_sel = 0; // None
    module->timer_synci_cfg.t1_in_sel = 1; // timer0 sync out
    // SYNC input and output configuration for both timers
    // Datasheet 16.4: PWM_TIMER0_SYNC_REG (0x000c)
    module->timer[MCPWM_TIMER_0].sync.in_en = 0; // Off
    // Generate sync output at timer equals zero of first timer
    module->timer[MCPWM_TIMER_0].sync.out_sel = 2;
    // Second timer is synchronized to first timer
    // Datasheet 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    module->timer[MCPWM_TIMER_1].sync.in_en = 1; // On
    module->timer[MCPWM_TIMER_1].sync.out_sel = 3; // Off
    ///// Start continuously running mode /////
    module->timer[MCPWM_TIMER_0].mode.start = 2;
    module->timer[MCPWM_TIMER_1].mode.start = 2;
    ///// Force update on all registers for settings to take effect /////
    // Datasheet 16.68: PWM_UPDATE_CFG_REG (0x010c)
    module->update_cfg.global_up_en = 1;
    // Toggle triggers a "forced register update" whatever that means..
    module->update_cfg.global_force_up = 1;
    module->update_cfg.global_force_up = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
}


/*****************************************************************
 * TIMER UP/DOWN-COUNTING MODE; DOES NOT USE HW-DEAD-TIME-MODULE *
 *****************************************************************
 */
esp_err_t pspwm_up_down_ctr_mode_init(mcpwm_unit_t mcpwm_num,
                                      const int gpio_lead_a, const int gpio_lead_b,
                                      const int gpio_lag_a, const int gpio_lag_b,
                                      const float frequency,
                                      const float ps_duty,
                                      const float lead_dt, const float lag_dt,
                                      const bool output_enabled,
                                      mcpwm_action_on_pwmxa_t disable_action_lag_leg,
                                      mcpwm_action_on_pwmxa_t disable_action_lead_leg)
{
    DBG("Call pspwm_up_down_ctr_mode_init");
    if (mcpwm_num != MCPWM_UNIT_0 && mcpwm_num != MCPWM_UNIT_1) {
        ERROR("mcpwm_num must be MCPWM_UNIT_0 or MCPWM_UNIT_1!");
        return ESP_FAIL;
    }
    if (!s_setpoints[mcpwm_num]) {
        s_setpoints[mcpwm_num] = malloc(sizeof(pspwm_setpoint_t));
        if (!s_setpoints[mcpwm_num]) {
            ERROR("Malloc failure!");
            return ESP_FAIL;
        }
    }
    if (!s_setpoint_limits[mcpwm_num]) {
        s_setpoint_limits[mcpwm_num] = malloc(sizeof(pspwm_setpoint_limits_t));
        if (!s_setpoint_limits[mcpwm_num]) {
            ERROR("Malloc failure!");
            return ESP_FAIL;
        }
    }
    s_setpoint_limits[mcpwm_num]->frequency_min = 0.5 * s_clk_conf.timer_clk / UINT16_MAX;
    s_setpoint_limits[mcpwm_num]->frequency_max = 0.5 * s_clk_conf.timer_clk / period_min;
    s_setpoint_limits[mcpwm_num]->dt_sum_max = 1.0 / frequency;
    DBG("frequency_min is now: %g", s_setpoint_limits[mcpwm_num]->frequency_min);
    DBG("frequency_max is now: %g", s_setpoint_limits[mcpwm_num]->frequency_max);
    DBG("dt_sum_max is now: %g", s_setpoint_limits[mcpwm_num]->dt_sum_max);
    // This is a 16-Bit timer register, although the API struct uses uint32_t...
    if (frequency <= s_setpoint_limits[mcpwm_num]->frequency_min
        || frequency > s_setpoint_limits[mcpwm_num]->frequency_max) {
            ERROR("Frequency setpoint out of range!");
            return ESP_FAIL;
    }
    if (ps_duty < 0 || ps_duty > 1) {
        ERROR("Invalid setpoint value for ps_duty");
        return ESP_FAIL;
    }
    if (lead_dt < 0 || lag_dt < 0
            || lead_dt >= 0.5 * s_setpoint_limits[mcpwm_num]->dt_sum_max
            || lag_dt  >= 0.5 * s_setpoint_limits[mcpwm_num]->dt_sum_max) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    s_setpoints[mcpwm_num]->frequency = frequency;
    s_setpoints[mcpwm_num]->ps_duty = ps_duty;
    s_setpoints[mcpwm_num]->lead_red = lead_dt;
    s_setpoints[mcpwm_num]->lead_fed = lead_dt; // Set but unused because identical
    s_setpoints[mcpwm_num]->lag_red = lag_dt;
    s_setpoints[mcpwm_num]->lag_fed = lag_dt; // Set but unused because identical
    s_setpoints[mcpwm_num]->output_enabled = output_enabled;
    periph_module_enable(PERIPH_PWM0_MODULE + mcpwm_num);
    // Basic setup for PS_PWM in up/down counting mode
    _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_num);
    // Setup the fault handler module as this is required for disabling the outputs
    _pspwm_setup_fault_handler_module(mcpwm_num,
                                      disable_action_lag_leg,
                                      disable_action_lead_leg);
    // Continue by setting a Fault Event forcing the GPIOs to defined "OFF" state
    bool is_ok = pspwm_disable_output(mcpwm_num) == ESP_OK;
    is_ok &= mcpwm_gpio_init(mcpwm_num, MCPWM0A, gpio_lead_a) == ESP_OK;
    is_ok &= mcpwm_gpio_init(mcpwm_num, MCPWM0B, gpio_lead_b) == ESP_OK;
    is_ok &= mcpwm_gpio_init(mcpwm_num, MCPWM1A, gpio_lag_a) == ESP_OK;
    is_ok &= mcpwm_gpio_init(mcpwm_num, MCPWM1B, gpio_lag_b) == ESP_OK;
    // In up_down_ctr_mode, this also sets the dead time; there should
    // be no need to call pspwm_up_down_ctr_mode_set_deadtimes() again.
    is_ok &= pspwm_up_down_ctr_mode_set_frequency(mcpwm_num, frequency) == ESP_OK;
    is_ok &= pspwm_up_down_ctr_mode_set_ps_duty(mcpwm_num, ps_duty) == ESP_OK;
    if (output_enabled) {
        is_ok &= pspwm_resync_enable_output(mcpwm_num) == ESP_OK;
    }
    if (is_ok) {
        DBG("pspwm_up_down_ctr_mode_init OK!");
        return ESP_OK;
    } else {
        ERROR("pspwm_up_down_ctr_mode_init failed!");
        return ESP_FAIL;
    }
}

esp_err_t pspwm_up_down_ctr_mode_set_frequency(mcpwm_unit_t mcpwm_num,
                                               const float frequency)
{
    DBG("Call pspwm_up_down_ctr_mode_set_frequency");
    // PWM hardware must have been initialised first
    pspwm_setpoint_t* setpoints = s_setpoints[mcpwm_num];
    assert(setpoints != NULL);
    // This is a 16-Bit timer register, although the API struct uses uint32_t...
    if (frequency <= s_setpoint_limits[mcpwm_num]->frequency_min
        || frequency > s_setpoint_limits[mcpwm_num]->frequency_max) {
            ERROR("Frequency setpoint out of range!");
            return ESP_FAIL;
    }
    // Set global state
    setpoints->frequency = frequency;
    float half_period = 0.5 * s_clk_conf.timer_clk / frequency;
    uint32_t timer_top = (uint32_t)half_period;
    uint32_t cmpr_lead_a = (uint32_t)(
        0.5 * (half_period - s_clk_conf.timer_clk * setpoints->lead_red));
    uint32_t cmpr_lag_a = (uint32_t)(
        0.5 * (half_period - s_clk_conf.timer_clk * setpoints->lag_red));
    uint32_t cmpr_lead_b = timer_top - cmpr_lead_a;
    uint32_t cmpr_lag_b = timer_top - cmpr_lag_a;
    // Phase shift value for Timer 1 needs updating when changing frequency.
    // Timer 0 is the reference phase and needs no update.
    uint32_t phase_setval = (uint32_t)(half_period * setpoints->ps_duty);
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_lead_a;
    // Register 16.18: PWM_GEN0_TSTMP_B_REG (0x0044) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_lead_b;
    // Register 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
    module->timer[MCPWM_TIMER_0].period.period = timer_top;
    // Same for timer 1
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_lag_a;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_lag_b;
    module->timer[MCPWM_TIMER_1].period.period = timer_top;
    // Phase shift value is based on timer 0 period setting but intentionally
    // only set for timer 1. Timer 0 is the reference phase.
    // Register 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    module->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Timer TOP is now: %d", timer_top);
    DBG("cmpr_0_a register value: %d", cmpr_lead_a);
    DBG("cmpr_0_b register value: %d", cmpr_lead_b);
    DBG("cmpr_1_a register value: %d", cmpr_lag_a);
    DBG("cmpr_1_b register value: %d", cmpr_lag_b);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

esp_err_t pspwm_up_down_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                               const float lead_dt, const float lag_dt)
{
    DBG("Call pspwm_up_down_ctr_mode_set_deadtimes()");
    // PWM hardware must be initialised first
    pspwm_setpoint_t* setpoints = s_setpoints[mcpwm_num];
    assert(setpoints != NULL);
    if (lead_dt < 0 || lag_dt < 0
            || lead_dt >= 0.5 * s_setpoint_limits[mcpwm_num]->dt_sum_max
            || lag_dt  >= 0.5 * s_setpoint_limits[mcpwm_num]->dt_sum_max) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    setpoints->lead_red = lead_dt;
    setpoints->lag_red = lag_dt;
    // PWM base period and duty cycle must be adjusted when changing dead-times
    // uint32_t timer_top = module->timer[MCPWM_TIMER_0].period.period;
    float half_period = 0.5 * s_clk_conf.timer_clk / setpoints->frequency;
    uint32_t timer_top = (uint32_t)half_period;
    uint32_t cmpr_lead_a = (uint32_t)(
        0.5 * (half_period - s_clk_conf.timer_clk * setpoints->lead_red));
    uint32_t cmpr_lag_a = (uint32_t)(
        0.5 * (half_period - s_clk_conf.timer_clk * setpoints->lag_red));
    uint32_t cmpr_lead_b = timer_top - cmpr_lead_a;
    uint32_t cmpr_lag_b = timer_top - cmpr_lag_a;
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_lead_a;
    // Register 16.18: PWM_GEN0_TSTMP_B_REG (0x0044) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_lead_b;
    // Same for timer 1
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_lag_a;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_lag_b;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("cmpr_0_a register value: %d", cmpr_lead_a);
    DBG("cmpr_0_b register value: %d", cmpr_lead_b);
    DBG("cmpr_1_a register value: %d", cmpr_lag_a);
    DBG("cmpr_1_b register value: %d", cmpr_lag_b);
    return ESP_OK;
}

esp_err_t pspwm_up_down_ctr_mode_set_ps_duty(mcpwm_unit_t mcpwm_num,
                                             const float ps_duty)
{
    DBG("Call pspwm_up_down_ctr_mode_set_ps_duty");
    if (ps_duty < 0 || ps_duty > 1) {
        ERROR("Invalid setpoint value for ps_duty");
        return ESP_FAIL;
    }
    // Set global state
    pspwm_setpoint_t* setpoints = s_setpoints[mcpwm_num];
    assert(setpoints != NULL);
    setpoints->ps_duty = ps_duty;
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    uint32_t timer_top = module->timer[MCPWM_TIMER_0].period.period;
    uint32_t phase_setval = (uint32_t)(timer_top * ps_duty);
    // Phase shift value is based on timer 0 period setting but intentionally
    // only set for timer 1. Timer 0 is the reference phase.
    // Register 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    module->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

void _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num) {
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Timer and deadtime module clock prescaler/divider configuration
    // Datasheet 16.1: PWM_CLK_CFG_REG (0x0000)
    // Hardware prescales by register value plus one, thus subtracting it here
    module->clk_cfg.prescale = s_clk_conf.base_clk_prescale - 1;

    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        // Hardware prescales by register value plus one, thus subtracting it here
        module->timer[timer_i].period.prescale = s_clk_conf.timer_clk_prescale - 1;
        // Datasheet 16.3: PWM_TIMER0_CFG1_REG (0x0008) etc.
        module->timer[timer_i].mode.mode = MCPWM_UP_DOWN_COUNTER;
        // 2 => Set output high; 1 => set output low
        // Datasheet 16.21: PWM_GEN0_A_REG (0x0050) etc.
        //module->channel[timer_i].generator[MCPWM_OPR_A].utez = 2;
        module->channel[timer_i].generator[MCPWM_OPR_A].utea = 1;
        module->channel[timer_i].generator[MCPWM_OPR_A].dtea = 2;
        // Datasheet 16.21: PWM_GEN0_B_REG (0x0054) etc.
        //module->channel[timer_i].generator[MCPWM_OPR_B].utez = 1;
        module->channel[timer_i].generator[MCPWM_OPR_B].uteb = 2;
        module->channel[timer_i].generator[MCPWM_OPR_B].dteb = 1;
    }
    // Update/swap shadow registers at timer equals zero for timer0,
    // update at sync for timer1.
    // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
    module->timer[MCPWM_TIMER_0].period.upmethod = 1; // TEZ
    module->timer[MCPWM_TIMER_1].period.upmethod = 2; // Literal 2 correct: At sync
    // Datasheet 16.16: PWM_GEN0_STMP_CFG_REG (0x003c) etc.
    module->channel[MCPWM_TIMER_0].cmpr_cfg.a_upmethod = 1; // TEZ
    module->channel[MCPWM_TIMER_0].cmpr_cfg.b_upmethod = 1; // TEZ
    module->channel[MCPWM_TIMER_1].cmpr_cfg.a_upmethod = 1ul<<2; // At sync
    module->channel[MCPWM_TIMER_1].cmpr_cfg.b_upmethod = 1ul<<2; // At sync
    // Datasheet 16.15: PWM_OPERATOR_TIMERSEL_REG (0x0038)
    module->timer_sel.operator0_sel = 0;
    module->timer_sel.operator1_sel = 1;
    // module->timer_sel.operator2_sel = 2;
    // SYNC input coupling setup: Timer 1 input coupled to timer 0 sync output
    // Datasheet 16.14: PWM_TIMER_SYNCI_CFG_REG (0x0034)
    module->timer_synci_cfg.t0_in_sel = 0; // None
    module->timer_synci_cfg.t1_in_sel = 1; // timer0 sync out
    // SYNC input and output configuration for both timers
    // Datasheet 16.4: PWM_TIMER0_SYNC_REG (0x000c)
    module->timer[MCPWM_TIMER_0].sync.in_en = 0; // Off
    // Generate sync output at timer equals zero of first timer
    module->timer[MCPWM_TIMER_0].sync.out_sel = 2;
    // Second timer is synchronized to first timer
    // Datasheet 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    module->timer[MCPWM_TIMER_1].sync.in_en = 1; // On
    module->timer[MCPWM_TIMER_1].sync.out_sel = 3; // Off
    ///// Start continuously running mode /////
    module->timer[MCPWM_TIMER_0].mode.start = 2;
    module->timer[MCPWM_TIMER_1].mode.start = 2;
    ///// Force update on all registers for settings to take effect /////
    // Datasheet 16.68: PWM_UPDATE_CFG_REG (0x010c)
    module->update_cfg.global_up_en = 1;
    // Toggle triggers a "forced register update" whatever that means..
    module->update_cfg.global_force_up = 1;
    module->update_cfg.global_force_up = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
}

/* Fault Handler ("Trip-Zone") input configuration.
 * Set up one-shot (stay-off) mode for fault handler module FH0.
 * This is used for software-forced output disabling.
 */
static void _pspwm_setup_fault_handler_module(
        mcpwm_unit_t mcpwm_num,
        mcpwm_action_on_pwmxa_t disable_action_lag_leg,
        mcpwm_action_on_pwmxa_t disable_action_lead_leg) {
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Datasheet 16.27: PWM_FH0_CFG0_REG (0x0068)
    // Enable sw-forced one-shot tripzone action
    module->channel[MCPWM_TIMER_0].tz_cfg0.sw_ost = 1;
    module->channel[MCPWM_TIMER_1].tz_cfg0.sw_ost = 1;
    // Uncomment to enable sw-forced cycle-by-cycle tripzone action
    //module->channel[timer_i].tz_cfg0.sw_cbc = 1;
    // Enable hardware-forced (event f0) one-shot tripzone action
    module->channel[MCPWM_TIMER_0].tz_cfg0.f0_ost = 1;
    module->channel[MCPWM_TIMER_1].tz_cfg0.f0_ost = 1;
    // Uncomment to enable hardware (event f0) cycle-by-cycle tripzone action
    //module->channel[timer_i].tz_cfg0.f0_cbc = 1;
    // Configure the kind of action (pull up / pull down) for the lag bridge leg:
    module->channel[MCPWM_TIMER_1].tz_cfg0.a_ost_d = disable_action_lag_leg;
    module->channel[MCPWM_TIMER_1].tz_cfg0.a_ost_u = disable_action_lag_leg;
    module->channel[MCPWM_TIMER_1].tz_cfg0.b_ost_d = disable_action_lag_leg;
    module->channel[MCPWM_TIMER_1].tz_cfg0.b_ost_u = disable_action_lag_leg;
    // Lead leg might have a different configuration, e.g. stay at last output level
    module->channel[MCPWM_TIMER_0].tz_cfg0.a_ost_d = disable_action_lead_leg;
    module->channel[MCPWM_TIMER_0].tz_cfg0.a_ost_u = disable_action_lead_leg;
    module->channel[MCPWM_TIMER_0].tz_cfg0.b_ost_d = disable_action_lead_leg;
    module->channel[MCPWM_TIMER_0].tz_cfg0.b_ost_u = disable_action_lead_leg;
    portEXIT_CRITICAL(&mcpwm_spinlock);
}

/*****************************************************************
 *                         COMMON SETUP                          *
 *****************************************************************
 */
bool pspwm_get_hw_fault_shutdown_status(mcpwm_unit_t mcpwm_num) {
    //bool status;
    //portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.58: PWM_FAULT_DETECT_REG (0x00e4)
    //status = (bool)MCPWM[mcpwm_num]->fault_detect.event_f0;
    // Register 16.29: PWM_FH0_STATUS_REG (0x0070)
    //status = (bool)MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_status.ost_on;
    //portEXIT_CRITICAL(&mcpwm_spinlock);
    return ost_fault_event_occurred[mcpwm_num];
}

esp_err_t pspwm_disable_output(mcpwm_unit_t mcpwm_num)
{
    DBG("Disabling output!");
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Toggle triggers the fault event
    // Register 16.28: PWM_FH0_CFG1_REG (0x006c)
    module->channel[MCPWM_TIMER_0].tz_cfg1.force_ost = 1;
    module->channel[MCPWM_TIMER_0].tz_cfg1.force_ost = 0;
    module->channel[MCPWM_TIMER_1].tz_cfg1.force_ost = 1;
    module->channel[MCPWM_TIMER_1].tz_cfg1.force_ost = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    // Update global state
    pspwm_setpoint_t* setpoints = s_setpoints[mcpwm_num];
    assert(setpoints != NULL);
    setpoints->output_enabled = false;
    return ESP_OK;
}

esp_err_t pspwm_resync_enable_output(mcpwm_unit_t mcpwm_num)
{
    DBG("Enabling output!");
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Toggle triggers the sync.
    module->timer[MCPWM_TIMER_0].sync.sync_sw = 1;
    module->timer[MCPWM_TIMER_0].sync.sync_sw = 0;
    module->timer[MCPWM_TIMER_1].sync.sync_sw = 1;
    module->timer[MCPWM_TIMER_1].sync.sync_sw = 0;
    // Toggle clears the fault event. XOR is somehow not reliable here.
    // Register 16.28: PWM_FH0_CFG1_REG (0x006c)
    module->channel[MCPWM_TIMER_0].tz_cfg1.clr_ost = 1;
    module->channel[MCPWM_TIMER_0].tz_cfg1.clr_ost = 0;
    module->channel[MCPWM_TIMER_1].tz_cfg1.clr_ost = 1;
    module->channel[MCPWM_TIMER_1].tz_cfg1.clr_ost = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    // Update global state
    pspwm_setpoint_t* setpoints = s_setpoints[mcpwm_num];
    assert(setpoints != NULL);
    setpoints->output_enabled = true;
    return ESP_OK;
}

esp_err_t pspwm_enable_hw_fault_shutdown(mcpwm_unit_t mcpwm_num,
                                         const int gpio_fault_shutdown,
                                         mcpwm_fault_input_level_t fault_pin_active_level) {
    DBG("Enabling hardware fault shutdown on GPIO: %d", gpio_fault_shutdown);
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    ///// Enable fault F0 generation from hardware pin /////
    // Datasheet 16.58: PWM_FAULT_DETECT_REG (0x00e4)
    module->fault_detect.f0_en = 1;
    // Set GPIO polarity for activation of trip event
    module->fault_detect.f0_pole = fault_pin_active_level;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return mcpwm_gpio_init(mcpwm_num, MCPWM_FAULT_0, gpio_fault_shutdown);
}

esp_err_t pspwm_disable_hw_fault_shutdown(mcpwm_unit_t mcpwm_num,
                                          const int gpio_fault_shutdown) {
    DBG("Resetting GPIO to default state: %d", gpio_fault_shutdown);
    mcpwm_dev_t* const module = MCPWM[mcpwm_num];
    portENTER_CRITICAL(&mcpwm_spinlock);
    ///// Enable fault F0 generation from hardware pin /////
    // Datasheet 16.58: PWM_FAULT_DETECT_REG (0x00e4)
    module->fault_detect.f0_en = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    // Resets pin to default state, i.e. pull-up enabled etc.
    return gpio_reset_pin(gpio_fault_shutdown);
}

esp_err_t pspwm_get_setpoint_ptr(mcpwm_unit_t mcpwm_num,
                                 pspwm_setpoint_t** setpoints) {
    if (!s_setpoints[mcpwm_num]) {
        ERROR("ERROR: The PMW unit must be initialised first!");
        return ESP_FAIL;
    }
    *setpoints = s_setpoints[mcpwm_num];
    return ESP_OK;
}

esp_err_t pspwm_get_setpoint_limits_ptr(mcpwm_unit_t mcpwm_num,
                                        pspwm_setpoint_limits_t** setpoint_limits) {
    if (!s_setpoint_limits[mcpwm_num]) {
        ERROR("ERROR: The PMW unit must be initialised first!");
        return ESP_FAIL;
    }
    *setpoint_limits = s_setpoint_limits[mcpwm_num];
    return ESP_OK;
}

esp_err_t pspwm_get_clk_conf_ptr(mcpwm_unit_t mcpwm_num,
                                 pspwm_clk_conf_t** clk_conf) {
    *clk_conf = &s_clk_conf;
    return ESP_OK;
}

esp_err_t pspwm_enable_interrupts(mcpwm_unit_t mcpwm_num,
                                  uint32_t mcpwm_interrupt_enable_mask) {
    portENTER_CRITICAL(&mcpwm_spinlock);
    MCPWM[mcpwm_num]->int_ena.val = mcpwm_interrupt_enable_mask;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return mcpwm_isr_register(
        mcpwm_num, pspwm_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);
}

/* Interrupt handler called on activation of either MCPWM_UNIT stage interrupts,
 * e.g. on hardware fault "tripzone" input trigger.
 * 
 * This sets a global flag when the One-Shot-Action Fault Handler is triggered
 * because the OST status from registers is seemingly broken e.g. the following
 * registers have the OST status only /while/ the hardware pin is active:
 * // Register 16.58: PWM_FAULT_DETECT_REG (0x00e4)
 * //status = (bool)MCPWM[mcpwm_num]->fault_detect.event_f0;
 * // Register 16.29: PWM_FH0_STATUS_REG (0x0070)
 * //status = (bool)MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_status.ost_on;
 */
static void IRAM_ATTR pspwm_isr_handler(void* arg) {
    uint32_t interrupt_status;
    // Get state for MCPWM unit 0 and set flag
    interrupt_status = MCPWM[MCPWM_UNIT_0]->int_st.val;
    ost_fault_event_occurred[0] = (bool)(interrupt_status & PSPWM_INT_FAULT0_INT);
    MCPWM[MCPWM_UNIT_0]->int_clr.val = interrupt_status;
    // Get state for MCPWM unit 1 and set flag
    interrupt_status = MCPWM[MCPWM_UNIT_1]->int_st.val;
    ost_fault_event_occurred[1] = (bool)(interrupt_status & PSPWM_INT_FAULT0_INT);
    MCPWM[MCPWM_UNIT_1]->int_clr.val = interrupt_status;
}