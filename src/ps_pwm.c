/* Use Espressif ESP32 platform MCPWM hardware module for generating
 * Phase-Shift-PWM waveform on 4 hardware pins.
 *
 * Application in ZVS-PS-PWM, DAB-DCM and LLC power electronics converters.
 * 
 * This depends on the ESP-IDF SDK
 *
 * 2019-12-11 Ulrich Lukas
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
static mcpwm_dev_t* const MCPWM[2] = {(mcpwm_dev_t*) &MCPWM0,
                                      (mcpwm_dev_t*) &MCPWM1};
// Atomic register access uses spinlock polling via "portENTER_CRITICAL()" macro
static portMUX_TYPE mcpwm_spinlock = portMUX_INITIALIZER_UNLOCKED;
// Setpoint values need to be static because of timing settings
// between frequency, phase and dead-time all depend on each other.
static setpoint_t* setpoints[2] = {NULL, NULL};

// Timer clock settings are common to both MCPWM stages
static clk_conf_t clk_conf = {
    .base_clk_prescale = 4,
    .timer_clk_prescale = 4,
    .base_clk = (float) MCPWM_INPUT_CLK / BASE_CLK_PRESCALE_DEFAULT,
    .timer_clk = (float) MCPWM_INPUT_CLK / (
            BASE_CLK_PRESCALE_DEFAULT * TIMER_CLK_PRESCALE_DEFAULT)
};

static setpoint_limits_t* s_setpoint_limits[2] = {NULL, NULL};

// Not part of external API
void _pspwm_up_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num);
void _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num);
void _pspwm_setup_fault_handler_module(mcpwm_unit_t mcpwm_num,
                                       mcpwm_action_on_pwmxa_t disable_action_lag_leg,
                                       mcpwm_action_on_pwmxa_t disable_action_lead_leg);

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
                                 mcpwm_action_on_pwmxa_t disable_action_lag_leg,
                                 mcpwm_action_on_pwmxa_t disable_action_lead_leg)
{
    DBG("Call pspwm_up_ctr_mode_init");
    if (mcpwm_num != MCPWM_UNIT_0 && mcpwm_num != MCPWM_UNIT_1) {
        ERROR("mcpwm_num must be MCPWM_UNIT_0 or MCPWM_UNIT_1!");
        return ESP_FAIL;
    }
    if (!setpoints[mcpwm_num]) {
        setpoints[mcpwm_num] = malloc(sizeof(setpoint_t));
        if (!setpoints[mcpwm_num]) {
            ERROR("Malloc failure!");
            return ESP_FAIL;
        }
    }
    if (!s_setpoint_limits[mcpwm_num]) {
        s_setpoint_limits[mcpwm_num] = malloc(sizeof(setpoint_limits_t));
        if (!s_setpoint_limits[mcpwm_num]) {
            ERROR("Malloc failure!");
            return ESP_FAIL;
        }
    }
    s_setpoint_limits[mcpwm_num]->frequency_min = clk_conf.timer_clk / UINT16_MAX;
    s_setpoint_limits[mcpwm_num]->frequency_max = clk_conf.timer_clk / (1 + timer_top_min);
    if (UINT16_MAX/clk_conf.base_clk < 1.0 / frequency) {
        s_setpoint_limits[mcpwm_num]->deadtime_sum_max = UINT16_MAX / clk_conf.base_clk;
    } else {
        s_setpoint_limits[mcpwm_num]->deadtime_sum_max = 1.0 / frequency;
    }
    DBG("frequency_min is now: %g", s_setpoint_limits[mcpwm_num]->frequency_min);
    DBG("frequency_max is now: %g", s_setpoint_limits[mcpwm_num]->frequency_max);
    DBG("deadtime_sum_max is now: %g", s_setpoint_limits[mcpwm_num]->deadtime_sum_max);
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
            || lead_red + lead_fed >= s_setpoint_limits[mcpwm_num]->deadtime_sum_max
            || lag_red + lag_fed >= s_setpoint_limits[mcpwm_num]->deadtime_sum_max) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    setpoints[mcpwm_num]->frequency = frequency;
    setpoints[mcpwm_num]->ps_duty = ps_duty;
    setpoints[mcpwm_num]->lead_red = lead_red;
    setpoints[mcpwm_num]->lead_fed = lead_fed;
    setpoints[mcpwm_num]->lag_red = lag_red;
    setpoints[mcpwm_num]->lag_fed = lag_fed;
    periph_module_enable(PERIPH_PWM0_MODULE + mcpwm_num);
    // Basic setup for PS_PWM in up/down counting mode
    _pspwm_up_ctr_mode_register_base_setup(mcpwm_num);
    // Setup the fault handler module as this is required for disabling the outputs
    _pspwm_setup_fault_handler_module(mcpwm_num,
                                      disable_action_lag_leg,
                                      disable_action_lead_leg);
    // Continue by setting a Fault Event forcing the GPIOs to defined "OFF" state
    if (pspwm_disable_output(mcpwm_num) == ESP_OK
        && mcpwm_gpio_init(mcpwm_num, MCPWM0A, gpio_lead_a) == ESP_OK
        && mcpwm_gpio_init(mcpwm_num, MCPWM0B, gpio_lead_b) == ESP_OK
        && mcpwm_gpio_init(mcpwm_num, MCPWM1A, gpio_lag_a) == ESP_OK
        && mcpwm_gpio_init(mcpwm_num, MCPWM1B, gpio_lag_b) == ESP_OK

        && pspwm_up_ctr_mode_set_frequency(mcpwm_num, frequency) == ESP_OK
        && pspwm_up_ctr_mode_set_deadtimes(
            mcpwm_num, lead_red, lead_fed, lag_red, lag_fed) == ESP_OK
        && pspwm_up_ctr_mode_set_ps_duty(mcpwm_num, ps_duty) == ESP_OK
        ) {
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
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    // PWM hardware must have been initialised first
    assert(setpoints[mcpwm_num] != NULL);
    // This is a 16-Bit timer register, although the API struct uses uint32_t...
    if (frequency <= s_setpoint_limits[mcpwm_num]->frequency_min
        || frequency > s_setpoint_limits[mcpwm_num]->frequency_max) {
            ERROR("Frequency setpoint out of range!");
            return ESP_FAIL;
    }
    uint32_t timer_top = (uint32_t)(clk_conf.timer_clk / frequency) - 1;
    uint32_t cmpr_0_a = (uint32_t)(
        0.5 * (timer_top + clk_conf.timer_clk * (setpoints[mcpwm_num]->lead_red 
                                                 - setpoints[mcpwm_num]->lead_fed)));
    uint32_t cmpr_1_a = (uint32_t)(
        0.5 * (timer_top + clk_conf.timer_clk * (setpoints[mcpwm_num]->lag_red
                                                 - setpoints[mcpwm_num]->lag_fed)));
    // Phase shift value for Timer 1 needs updating when changing frequency.
    // Timer 0 is the reference phase and needs no update.
    uint32_t phase_setval = (uint32_t)(timer_top * setpoints[mcpwm_num]->ps_duty / 2);
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
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    ///// Force update on all registers for settings to take effect /////
    // Toggle triggers a "forced register update" whatever that means..
    //MCPWM[mcpwm_num]->update_cfg.global_force_up = 1;
    //MCPWM[mcpwm_num]->update_cfg.global_force_up = 0;
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
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    // PWM hardware must be initialised first
    assert(setpoints[mcpwm_num] != NULL);
    // PWM base period and duty cycle must be adjusted when changing dead-times
    //uint32_t timer_top = MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].period.period;
    uint32_t timer_top = (uint32_t)(clk_conf.timer_clk / setpoints[mcpwm_num]->frequency) - 1;
    DBG("Limit value for (red + fed) in ns: %f", 1e9*timer_top / clk_conf.base_clk);
    if (lead_red < 0 || lead_fed < 0 || lag_red < 0 || lag_fed < 0
            || lead_red + lead_fed >= s_setpoint_limits[mcpwm_num]->deadtime_sum_max
            || lag_red + lag_fed >= s_setpoint_limits[mcpwm_num]->deadtime_sum_max) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    // Static variables needed when changing PWM frequency or other settings
    // which depend on dead-time values
    setpoints[mcpwm_num]->lead_red = lead_red;
    setpoints[mcpwm_num]->lead_fed = lead_fed;
    setpoints[mcpwm_num]->lag_red = lag_red;
    setpoints[mcpwm_num]->lag_fed = lag_fed;
    uint32_t lead_red_reg = (uint32_t)(lead_red * clk_conf.base_clk);
    uint32_t lead_fed_reg = (uint32_t)(lead_fed * clk_conf.base_clk);
    uint32_t lag_red_reg = (uint32_t)(lag_red * clk_conf.base_clk);
    uint32_t lag_fed_reg = (uint32_t)(lag_fed * clk_conf.base_clk);
    uint32_t cmpr_0_a = (uint32_t)(
        0.5 * (timer_top + clk_conf.timer_clk * (setpoints[mcpwm_num]->lead_red
                                              - setpoints[mcpwm_num]->lead_fed)));
    uint32_t cmpr_1_a = (uint32_t)(
        0.5 * (timer_top + clk_conf.timer_clk * (setpoints[mcpwm_num]->lag_red
                                              - setpoints[mcpwm_num]->lag_fed)));
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.25: PWM_DT0_RED_CFG_REG (0x0060) etc.
    module->channel[MCPWM_TIMER_0].db_red_cfg.red = lead_red_reg;
    // Register 16.24: PWM_DT0_FED_CFG_REG (0x005c) etc.
    module->channel[MCPWM_TIMER_0].db_fed_cfg.fed = lead_fed_reg;
    module->channel[MCPWM_TIMER_1].db_red_cfg.red = lag_red_reg;
    module->channel[MCPWM_TIMER_1].db_fed_cfg.fed = lag_fed_reg;
    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Register 16.23: PWM_DT0_CFG_REG (0x0058) etc.
        module->channel[timer_i].db_cfg.fed_upmethod = 1; // TEZ
        module->channel[timer_i].db_cfg.red_upmethod = 1; // TEZ
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
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_0_a;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_1_a;
    portEXIT_CRITICAL(&mcpwm_spinlock);
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
    portENTER_CRITICAL(&mcpwm_spinlock);
    uint32_t curr_period = MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].period.period;
    uint32_t phase_setval = (uint32_t)(curr_period * ps_duty / 2);
    // Phase shift value is based on timer 0 period setting but intentionally
    // only set for timer 1. Timer 0 is the reference phase.
    // Register 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

void _pspwm_up_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num) {
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Timer and deadtime module clock prescaler/divider configuration
    // Datasheet 16.1: PWM_CLK_CFG_REG (0x0000)
    // Hardware prescales by register value plus one, thus subtracting it here
    MCPWM[mcpwm_num]->clk_cfg.prescale = clk_conf.base_clk_prescale - 1;

    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        // Hardware prescales by register value plus one, thus subtracting it here
        MCPWM[mcpwm_num]->timer[timer_i].period.prescale = clk_conf.timer_clk_prescale - 1;
        // 3 => update at timer equals zero OR at sync...
        MCPWM[mcpwm_num]->timer[timer_i].period.upmethod = 3;
        // Datasheet 16.3: PWM_TIMER0_CFG1_REG (0x0008) etc.
        MCPWM[mcpwm_num]->timer[timer_i].mode.mode = MCPWM_UP_COUNTER;
        // Update/swap shadow registers at timer equals zero
        // Datasheet 16.16: PWM_GEN0_STMP_CFG_REG (0x003c) etc.
        MCPWM[mcpwm_num]->channel[timer_i].cmpr_cfg.a_upmethod = 1;
        MCPWM[mcpwm_num]->channel[timer_i].cmpr_cfg.b_upmethod = 1;
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
    ///// Start continuously running mode /////
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].mode.start = 2;
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].mode.start = 2;
    ///// Force update on all registers for settings to take effect /////
    // Datasheet 16.68: PWM_UPDATE_CFG_REG (0x010c)
    MCPWM[mcpwm_num]->update_cfg.global_up_en = 1;
    // Toggle triggers a "forced register update" whatever that means..
    MCPWM[mcpwm_num]->update_cfg.global_force_up = 1;
    MCPWM[mcpwm_num]->update_cfg.global_force_up = 0;
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
                                      mcpwm_action_on_pwmxa_t disable_action_lag_leg,
                                      mcpwm_action_on_pwmxa_t disable_action_lead_leg)
{
    DBG("Call pspwm_up_down_ctr_mode_init");
    if (mcpwm_num != MCPWM_UNIT_0 && mcpwm_num != MCPWM_UNIT_1) {
        ERROR("mcpwm_num must be MCPWM_UNIT_0 or MCPWM_UNIT_1!");
        return ESP_FAIL;
    }
    if (!setpoints[mcpwm_num]) {
        setpoints[mcpwm_num] = malloc(sizeof(setpoint_t));
        if (!setpoints[mcpwm_num]) {
            ERROR("Malloc failure!");
            return ESP_FAIL;
        }
    }
    if (!s_setpoint_limits[mcpwm_num]) {
        s_setpoint_limits[mcpwm_num] = malloc(sizeof(setpoint_limits_t));
        if (!s_setpoint_limits[mcpwm_num]) {
            ERROR("Malloc failure!");
            return ESP_FAIL;
        }
    }
    s_setpoint_limits[mcpwm_num]->frequency_min = 0.5 * clk_conf.timer_clk / UINT16_MAX;
    s_setpoint_limits[mcpwm_num]->frequency_max = 0.5 * clk_conf.timer_clk / timer_top_min;
    s_setpoint_limits[mcpwm_num]->deadtime_sum_max = 1.0 / frequency;
    DBG("frequency_min is now: %g", s_setpoint_limits[mcpwm_num]->frequency_min);
    DBG("frequency_max is now: %g", s_setpoint_limits[mcpwm_num]->frequency_max);
    DBG("deadtime_sum_max is now: %g", s_setpoint_limits[mcpwm_num]->deadtime_sum_max);
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
            || lead_dt >= 0.5 * s_setpoint_limits[mcpwm_num]->deadtime_sum_max
            || lag_dt  >= 0.5 * s_setpoint_limits[mcpwm_num]->deadtime_sum_max) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    setpoints[mcpwm_num]->frequency = frequency;
    setpoints[mcpwm_num]->ps_duty = ps_duty;
    setpoints[mcpwm_num]->lead_red = lead_dt;
    setpoints[mcpwm_num]->lead_fed = lead_dt; // Set but unused because identical
    setpoints[mcpwm_num]->lag_red = lag_dt;
    setpoints[mcpwm_num]->lag_fed = lag_dt; // Set but unused because identical
    periph_module_enable(PERIPH_PWM0_MODULE + mcpwm_num);
    // Basic setup for PS_PWM in up/down counting mode
    _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_num);
    // Setup the fault handler module as this is required for disabling the outputs
    _pspwm_setup_fault_handler_module(mcpwm_num,
                                      disable_action_lag_leg,
                                      disable_action_lead_leg);
    // Continue by setting a Fault Event forcing the GPIOs to defined "OFF" state
    if (pspwm_disable_output(mcpwm_num) == ESP_OK
        && mcpwm_gpio_init(mcpwm_num, MCPWM0A, gpio_lead_a) == ESP_OK
        && mcpwm_gpio_init(mcpwm_num, MCPWM0B, gpio_lead_b) == ESP_OK
        && mcpwm_gpio_init(mcpwm_num, MCPWM1A, gpio_lag_a) == ESP_OK
        && mcpwm_gpio_init(mcpwm_num, MCPWM1B, gpio_lag_b) == ESP_OK
        // In up_down_ctr_mode, this also sets the dead time; there should
        // be no need to call pspwm_up_down_ctr_mode_set_deadtimes() again.
        && pspwm_up_down_ctr_mode_set_frequency(mcpwm_num, frequency) == ESP_OK
        && pspwm_up_down_ctr_mode_set_ps_duty(mcpwm_num, ps_duty) == ESP_OK
        ) {
        DBG("pspwm_up_ctr_mode_init OK!");
        return ESP_OK;
    } else {
        ERROR("pspwm_up_ctr_mode_init failed!");
        return ESP_FAIL;
    }
}

esp_err_t pspwm_up_down_ctr_mode_set_frequency(mcpwm_unit_t mcpwm_num,
                                               const float frequency)
{
    DBG("Call pspwm_up_down_ctr_mode_set_frequency");
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    // PWM hardware must have been initialised first
    assert(setpoints[mcpwm_num] != NULL);
    // This is a 16-Bit timer register, although the API struct uses uint32_t...
    if (frequency <= s_setpoint_limits[mcpwm_num]->frequency_min
        || frequency > s_setpoint_limits[mcpwm_num]->frequency_max) {
            ERROR("Frequency setpoint out of range!");
            return ESP_FAIL;
    }
    uint32_t timer_top = (uint32_t)(0.5 * clk_conf.timer_clk / frequency);
    uint32_t cmpr_lead_a = (uint32_t)(
        0.5 * clk_conf.timer_clk * setpoints[mcpwm_num]->lead_red);
    uint32_t cmpr_lag_a = (uint32_t)(
        0.5 * clk_conf.timer_clk * setpoints[mcpwm_num]->lag_red);
    uint32_t cmpr_lead_b = timer_top - cmpr_lead_a;
    uint32_t cmpr_lag_b = timer_top - cmpr_lag_a;
    // Phase shift value for Timer 1 needs updating when changing frequency.
    // Timer 0 is the reference phase and needs no update.
    uint32_t phase_setval = (uint32_t)(timer_top * setpoints[mcpwm_num]->ps_duty / 2);
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
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Timer TOP is now: %d", timer_top);
    DBG("cmpr_0_a register value: %d", cmpr_lead_a);
    DBG("cmpr_0_a register value: %d", cmpr_lead_b);
    DBG("cmpr_1_a register value: %d", cmpr_lag_a);
    DBG("cmpr_1_a register value: %d", cmpr_lag_b);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

esp_err_t pspwm_up_down_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                               const float lead_dt, const float lag_dt)
{
    DBG("Call pspwm_up_down_ctr_mode_set_deadtimes()");
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    // PWM hardware must be initialised first
    assert(setpoints[mcpwm_num] != NULL);
    // PWM base period and duty cycle must be adjusted when changing dead-times
    uint32_t timer_top = module->timer[MCPWM_TIMER_0].period.period;
    if (lead_dt < 0 || lag_dt < 0
            || lead_dt >= 0.5 * s_setpoint_limits[mcpwm_num]->deadtime_sum_max
            || lag_dt  >= 0.5 * s_setpoint_limits[mcpwm_num]->deadtime_sum_max) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    setpoints[mcpwm_num]->lead_red = lead_dt;
    setpoints[mcpwm_num]->lag_red = lag_dt;
    uint32_t cmpr_lead_a = (uint32_t)(0.5 * clk_conf.timer_clk * lead_dt);
    uint32_t cmpr_lead_b = timer_top - cmpr_lead_a;
    uint32_t cmpr_lag_a = (uint32_t)(0.5 * clk_conf.timer_clk * lag_dt);
    uint32_t cmpr_lag_b = timer_top - cmpr_lag_a;
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
    DBG("Timer compare registers set to: %d, %d, %d, %d",
        cmpr_lead_a, cmpr_lag_a, cmpr_lead_b, cmpr_lag_b);
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
    portENTER_CRITICAL(&mcpwm_spinlock);
    uint32_t curr_period = MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].period.period;
    uint32_t phase_setval = (uint32_t)(curr_period * ps_duty);
    // Phase shift value is based on timer 0 period setting but intentionally
    // only set for timer 1. Timer 0 is the reference phase.
    // Register 16.8: PWM_TIMER1_SYNC_REG (0x001c)
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.timer_phase = phase_setval;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Phase register set to: %d", phase_setval);
    return ESP_OK;
}

void _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num) {
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Timer and deadtime module clock prescaler/divider configuration
    // Datasheet 16.1: PWM_CLK_CFG_REG (0x0000)
    // Hardware prescales by register value plus one, thus subtracting it here
    MCPWM[mcpwm_num]->clk_cfg.prescale = clk_conf.base_clk_prescale - 1;

    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        // Hardware prescales by register value plus one, thus subtracting it here
        MCPWM[mcpwm_num]->timer[timer_i].period.prescale = clk_conf.timer_clk_prescale - 1;
        // 3 => update at timer equals zero OR at sync
        MCPWM[mcpwm_num]->timer[timer_i].period.upmethod = 3;
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
    ///// Start continuously running mode /////
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].mode.start = 2;
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].mode.start = 2;
    ///// Force update on all registers for settings to take effect /////
    // Datasheet 16.68: PWM_UPDATE_CFG_REG (0x010c)
    MCPWM[mcpwm_num]->update_cfg.global_up_en = 1;
    // Toggle triggers a "forced register update" whatever that means..
    MCPWM[mcpwm_num]->update_cfg.global_force_up = 1;
    MCPWM[mcpwm_num]->update_cfg.global_force_up = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
}

/* Fault Handler ("Trip-Zone") input configuration.
 * Set up one-shot (stay-off) mode for fault handler module FH0.
 * This is used for software-forced output disabling.
 */
void _pspwm_setup_fault_handler_module(mcpwm_unit_t mcpwm_num,
                                       mcpwm_action_on_pwmxa_t disable_action_lag_leg,
                                       mcpwm_action_on_pwmxa_t disable_action_lead_leg) {
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Datasheet 16.27: PWM_FH0_CFG0_REG (0x0068)
    // Enable sw-forced one-shot tripzone action
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg0.sw_ost = 1;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg0.sw_ost = 1;
    // Uncomment to enable sw-forced cycle-by-cycle tripzone action
    //MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.sw_cbc = 1;
    // Enable hardware-forced (event f0) one-shot tripzone action
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg0.f0_ost = 1;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg0.f0_ost = 1;
    // Uncomment to enable hardware (event f0) cycle-by-cycle tripzone action
    //MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.f0_cbc = 1;
    // Configure the kind of action (pull up / pull down) for the lag bridge leg:
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg0.a_ost_d = disable_action_lag_leg;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg0.a_ost_u = disable_action_lag_leg;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg0.b_ost_d = disable_action_lag_leg;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg0.b_ost_u = disable_action_lag_leg;
    // Lead leg might have a different configuration, e.g. stay at last output level
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg0.a_ost_d = disable_action_lead_leg;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg0.a_ost_u = disable_action_lead_leg;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg0.b_ost_d = disable_action_lead_leg;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg0.b_ost_u = disable_action_lead_leg;
    portEXIT_CRITICAL(&mcpwm_spinlock);
}

/*****************************************************************
 *                         COMMON SETUP                          *
 *****************************************************************
 */
esp_err_t pspwm_disable_output(mcpwm_unit_t mcpwm_num)
{
    DBG("Disabling output!");
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Toggle triggers the fault event
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg1.force_ost = 1;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg1.force_ost = 0;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg1.force_ost = 1;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg1.force_ost = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}

esp_err_t pspwm_resync_enable_output(mcpwm_unit_t mcpwm_num)
{
    DBG("Enabling output!");
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Toggle triggers the sync.
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].sync.sync_sw = 1;
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].sync.sync_sw = 0;
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.sync_sw = 1;
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.sync_sw = 0;
    // Toggle clears the fault event. XOR is somehow not reliable here.
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg1.clr_ost = 1;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg1.clr_ost = 0;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg1.clr_ost = 1;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg1.clr_ost = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}

esp_err_t pspwm_enable_hw_fault_shutdown(mcpwm_unit_t mcpwm_num,
                                         const int gpio_fault_shutdown,
                                         mcpwm_fault_input_level_t fault_pin_active_level) {
    DBG("Enabling hardware fault shutdown on GPIO: %d", gpio_fault_shutdown);
    portENTER_CRITICAL(&mcpwm_spinlock);
    ///// Enable fault F0 generation from hardware pin /////
    // Datasheet 16.58: PWM_FAULT_DETECT_REG (0x00e4)
    MCPWM[mcpwm_num]->fault_detect.f0_en = 1;
    // Set GPIO polarity for activation of trip event
    MCPWM[mcpwm_num]->fault_detect.f0_pole = fault_pin_active_level;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return mcpwm_gpio_init(mcpwm_num, MCPWM_FAULT_0, gpio_fault_shutdown);
}

esp_err_t pspwm_disable_hw_fault_shutdown(mcpwm_unit_t mcpwm_num,
                                          const int gpio_fault_shutdown) {
    DBG("Resetting GPIO to default state: %d", gpio_fault_shutdown);
    portENTER_CRITICAL(&mcpwm_spinlock);
    ///// Enable fault F0 generation from hardware pin /////
    // Datasheet 16.58: PWM_FAULT_DETECT_REG (0x00e4)
    MCPWM[mcpwm_num]->fault_detect.f0_en = 0;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    // Resets pin to default state, i.e. pull-up enabled etc.
    return gpio_reset_pin(gpio_fault_shutdown);
}

esp_err_t pspwm_get_setpoint_limits(mcpwm_unit_t mcpwm_num,
                                    setpoint_limits_t** setpoint_limits) {
    if (!setpoint_limits[mcpwm_num]) {
        ERROR("ERROR: The PMW unit must be initialised first!");
        return ESP_FAIL;
    }
    *setpoint_limits = s_setpoint_limits[mcpwm_num];
    return ESP_OK;
}