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

/* Dead time settings for both MCPWM hardware modules are defined as lead and
 * lag bridge-leg low-side output rising and falling edge dead-times in seconds
 */
typedef struct {
    // Lead leg, dead time for rising edge (up_ctr_mode)
    // or both edges (up_down_ctr_mode)
    float lead_red;
    // Falling edge dead time for up_ctr_mode, not defined for up_down_ctr_mode
    float lead_fed;
    // All the same for lagging leg
    float lag_red;
    float lag_fed;
} dt_conf_t;

// MMAP IO using direct register access.
// External definition and declaration in mcpwm_struct.h
static mcpwm_dev_t * const MCPWM[2] = {(mcpwm_dev_t*) &MCPWM0,
                                       (mcpwm_dev_t*) &MCPWM1};
// Atomic register access uses spinlock polling via "portENTER_CRITICAL()" macro
static portMUX_TYPE mcpwm_spinlock = portMUX_INITIALIZER_UNLOCKED;
// Dead time settings need to be shared between calls because
// frequency and dead-time settings depend on each other
static dt_conf_t *deadtimes[2] = {NULL, NULL};

static setpoint_limits_t *s_setpoint_limits[2] = {NULL, NULL};

// Not part of external API
void _pspwm_up_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num);
void _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num);


/**********************************************************************
 *    FULL-SPEED-MODE, 4x INDIVIDUAL DEAD-TIME, HW-DEAD-TIME-MODULE
 **********************************************************************
 */
esp_err_t pspwm_up_ctr_mode_init(
        mcpwm_unit_t mcpwm_num,
        int gpio_lead_a, int gpio_lead_b, int gpio_lag_a, int gpio_lag_b,
        float frequency,
        float ps_duty,
        float lead_red, float lead_fed, float lag_red, float lag_fed)
{
    DBG("Call pspwm_up_ctr_mode_init");
    if (mcpwm_num != MCPWM_UNIT_0 && mcpwm_num != MCPWM_UNIT_1) {
        ERROR("mcpwm_num must be MCPWM_UNIT_0 or MCPWM_UNIT_1!");
        return ESP_FAIL;
    }
    if (!deadtimes[mcpwm_num]) {
        deadtimes[mcpwm_num] = malloc(sizeof(dt_conf_t));
    }
    if (!s_setpoint_limits[mcpwm_num]) {
        s_setpoint_limits[mcpwm_num] = malloc(sizeof(setpoint_limits_t));
    }
    deadtimes[mcpwm_num]->lead_red = lead_red;
    deadtimes[mcpwm_num]->lead_fed = lead_fed;
    deadtimes[mcpwm_num]->lag_red = lag_red;
    deadtimes[mcpwm_num]->lag_fed = lag_fed;
    periph_module_enable(PERIPH_PWM0_MODULE + mcpwm_num);
    // Basic setup for PS_PWM in up/down counting mode
    _pspwm_up_ctr_mode_register_base_setup(mcpwm_num);
    // Setting a Fault Event forcing the GPIOs to pre-defined state
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
                                          float frequency)
{
    DBG("Call pspwm_up_ctr_mode_set_frequency");
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    // PWM hardware must have been initialised first
    assert(deadtimes[mcpwm_num] != NULL);
    // This is a 16-Bit timer register, although the API struct uses uint32_t...
    if (frequency <= (float)MCPWM_TIMER_CLK / UINT16_MAX) {
        ERROR("Frequency setpoint out of range!");
        return ESP_FAIL;
    }
    uint32_t timer_top = (uint32_t)(MCPWM_TIMER_CLK / frequency);
    uint32_t cmpr_0_a = (uint32_t)(
        0.5 * (timer_top + MCPWM_TIMER_CLK * (deadtimes[mcpwm_num]->lead_red 
                                              - deadtimes[mcpwm_num]->lead_fed)));
    uint32_t cmpr_1_a = (uint32_t)(
        0.5 * (timer_top + MCPWM_TIMER_CLK * (deadtimes[mcpwm_num]->lag_red
                                              - deadtimes[mcpwm_num]->lag_fed)));
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
    module->timer[MCPWM_TIMER_0].period.period = timer_top;
    module->timer[MCPWM_TIMER_1].period.period = timer_top;
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_0_a;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_1_a;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Timer TOP is now: %d", timer_top);
    DBG("cmpr_0_a register value: %d", cmpr_0_a);
    DBG("cmpr_1_a register value: %d", cmpr_1_a);
    return ESP_OK;
}

esp_err_t pspwm_up_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                          float lead_red, float lead_fed,
                                          float lag_red, float lag_fed)
{
    DBG("Call pspwm_up_ctr_mode_set_deadtimes()");
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    // PWM hardware must be initialised first
    assert(deadtimes[mcpwm_num] != NULL);
    // PWM base period and duty cycle must be adjusted when changing dead-times
    uint32_t timer_top = MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].period.period;
    DBG("Limit value for (red + fed) in ns: %f", 1e9*timer_top / MCPWM_TIMER_CLK);
    if (lead_red < 0 || lead_fed < 0 || lag_red < 0 || lag_fed < 0
            || MCPWM_TIMER_CLK * (lead_red + lead_fed) >= timer_top
            || MCPWM_TIMER_CLK * (lag_red + lag_fed) >= timer_top) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    // Static variables needed when changing PWM frequency or other settings
    // which depend on dead-time values
    deadtimes[mcpwm_num]->lead_red = lead_red;
    deadtimes[mcpwm_num]->lead_fed = lead_fed;
    deadtimes[mcpwm_num]->lag_red = lag_red;
    deadtimes[mcpwm_num]->lag_fed = lag_fed;
    uint32_t lead_red_reg = (uint32_t)(lead_red * MCPWM_BASE_CLK);
    uint32_t lead_fed_reg = (uint32_t)(lead_fed * MCPWM_BASE_CLK);
    uint32_t lag_red_reg = (uint32_t)(lag_red * MCPWM_BASE_CLK);
    uint32_t lag_fed_reg = (uint32_t)(lag_fed * MCPWM_BASE_CLK);
    uint32_t cmpr_0_a = (uint32_t)(
        0.5 * (timer_top + MCPWM_TIMER_CLK * (deadtimes[mcpwm_num]->lead_red
                                              - deadtimes[mcpwm_num]->lead_fed)));
    uint32_t cmpr_1_a = (uint32_t)(
        0.5 * (timer_top + MCPWM_TIMER_CLK * (deadtimes[mcpwm_num]->lag_red
                                              - deadtimes[mcpwm_num]->lag_fed)));
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
        module->channel[timer_i].db_cfg.clk_sel = 0; // MCPWM_BASE_CLK
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

esp_err_t pspwm_up_ctr_mode_set_ps_duty(mcpwm_unit_t mcpwm_num, float ps_duty)
{
    DBG("Call pspwm_up_ctr_mode_set_ps_duty");
    if (ps_duty < 0 || ps_duty > 1) {
        ERROR("Invalid setpoint value for ps_duty");
        return ESP_FAIL;
    }
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

void _pspwm_up_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num) {
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Timer and deadtime module clock prescaler/divider configuration
    // Datasheet 16.1: PWM_CLK_CFG_REG (0x0000)
    // Hardware prescales by register value plus one, thus subtracting it here
    MCPWM[mcpwm_num]->clk_cfg.prescale = BASE_CLK_PRESCALE - 1;

    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        // Hardware prescales by register value plus one, thus subtracting it here
        MCPWM[mcpwm_num]->timer[timer_i].period.prescale = TIMER_CLK_PRESCALE - 1;
        // Set first bit => update at timer equals zero
        MCPWM[mcpwm_num]->timer[timer_i].period.upmethod = 1;
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
        /* Fault Handler ("Trip-Zone") input configuration.
         * Set up one-shot (stay-off) mode for fault handler 0.
         * This is used for software-forced output disabling.
         */
        // Datasheet 16.27: PWM_FH0_CFG0_REG (0x0068)
        // Enable sw-forced one-shot tripzone action
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.sw_ost = 1;
        // Uncomment to enable sw-forced cycle-by-cycle tripzone action
        //MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.sw_cbc = 1;
        // Enable hardware-forced (event f0) one-shot tripzone action
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.f0_ost = 1;
        // Uncomment to enable hardware (event f0) cycle-by-cycle tripzone action
        //MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.f0_cbc = 1;
        // Configure the kind of action (pull up / pull down)
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.a_ost_d = TRIPZONE_ACTION_PWMxA;
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.a_ost_u = TRIPZONE_ACTION_PWMxA;
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.b_ost_d = TRIPZONE_ACTION_PWMxB;
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.b_ost_u = TRIPZONE_ACTION_PWMxB;
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
    // Toggle triggers a "forced register update" whatever that means..
    MCPWM[mcpwm_num]->update_cfg.global_force_up ^= 1;
    portEXIT_CRITICAL(&mcpwm_spinlock);
}


/*****************************************************************
 * TIMER UP/DOWN-COUNTING MODE; DOES NOT USE HW-DEAD-TIME-MODULE *
 *****************************************************************
 */
esp_err_t pspwm_up_down_ctr_mode_init(
        mcpwm_unit_t mcpwm_num,
        int gpio_lead_a, int gpio_lead_b, int gpio_lag_a, int gpio_lag_b,
        float frequency,
        float ps_duty,
        float lead_dt, float lag_dt)
{
    DBG("Call pspwm_up_ctr_mode_init");
    if (mcpwm_num != MCPWM_UNIT_0 && mcpwm_num != MCPWM_UNIT_1) {
        ERROR("mcpwm_num must be MCPWM_UNIT_0 or MCPWM_UNIT_1!");
        return ESP_FAIL;
    }
    if (!deadtimes[mcpwm_num]) {
        deadtimes[mcpwm_num] = malloc(sizeof(dt_conf_t));
    }
    if (!s_setpoint_limits[mcpwm_num]) {
        s_setpoint_limits[mcpwm_num] = malloc(sizeof(setpoint_limits_t));
    }
    deadtimes[mcpwm_num]->lead_red = lead_dt;
    deadtimes[mcpwm_num]->lag_red = lag_dt;
    periph_module_enable(PERIPH_PWM0_MODULE + mcpwm_num);
    // Basic setup for PS_PWM in up/down counting mode
    _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_num);
    // Setting a Fault Event forcing the GPIOs to pre-defined state
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
                                               float frequency)
{
    DBG("Call pspwm_up_down_ctr_mode_set_frequency");
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    // PWM hardware must have been initialised first
    assert(deadtimes[mcpwm_num] != NULL);
    // This is a 16-Bit timer register, although the API struct uses uint32_t...
    if (frequency <= (float)MCPWM_TIMER_CLK / UINT16_MAX) {
        ERROR("Frequency setpoint out of range!");
        return ESP_FAIL;
    }
    uint32_t timer_top = (uint32_t)(MCPWM_TIMER_CLK / frequency);
    uint32_t cmpr_a_lead = (uint32_t)(
        MCPWM_TIMER_CLK * deadtimes[mcpwm_num]->lead_red / 2);
    uint32_t cmpr_a_lag = (uint32_t)(
        MCPWM_TIMER_CLK * deadtimes[mcpwm_num]->lag_red / 2);
    uint32_t cmpr_b_lead = timer_top - cmpr_a_lead;
    uint32_t cmpr_b_lag = timer_top - cmpr_a_lag;
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_a_lead;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_a_lag;
    // Register 16.18: PWM_GEN0_TSTMP_B_REG (0x0044) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_b_lead;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_b_lag;
    // Register 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
    module->timer[MCPWM_TIMER_0].period.period = timer_top;
    module->timer[MCPWM_TIMER_1].period.period = timer_top;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Timer TOP is now: %d", timer_top);
    return ESP_OK;
}

esp_err_t pspwm_up_down_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                               float lead_dt, float lag_dt)
{
    DBG("Call pspwm_up_down_ctr_mode_set_deadtimes()");
    mcpwm_dev_t *module = MCPWM[mcpwm_num];
    // PWM hardware must be initialised first
    assert(deadtimes[mcpwm_num] != NULL);
    // PWM base period and duty cycle must be adjusted when changing dead-times
    uint32_t timer_top = module->timer[MCPWM_TIMER_0].period.period;
    if (lead_dt < 0 || lag_dt < 0
            || MCPWM_TIMER_CLK * lead_dt >= timer_top
            || MCPWM_TIMER_CLK * lag_dt  >= timer_top) {
        ERROR("Dead time setpoint out of range");
        return ESP_FAIL;
    }
    deadtimes[mcpwm_num]->lead_red = lead_dt;
    deadtimes[mcpwm_num]->lag_red = lag_dt;
    uint32_t cmpr_a_lead = (uint32_t)(MCPWM_TIMER_CLK * lead_dt/2);
    uint32_t cmpr_b_lead = timer_top - cmpr_a_lead;
    uint32_t cmpr_a_lag = (uint32_t)(MCPWM_TIMER_CLK * lag_dt/2);
    uint32_t cmpr_b_lag = timer_top - cmpr_a_lag;
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Register 16.17: PWM_GEN0_TSTMP_A_REG (0x0040) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_a_lead;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_A].cmpr_val = cmpr_a_lag;
    // Register 16.18: PWM_GEN0_TSTMP_B_REG (0x0044) etc.
    // also for GEN1 with different register offset
    module->channel[MCPWM_TIMER_0].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_b_lead;
    module->channel[MCPWM_TIMER_1].cmpr_value[MCPWM_OPR_B].cmpr_val = cmpr_b_lag;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    DBG("Timer compare registers set to: %d, %d, %d, %d",
        cmpr_a_lead, cmpr_a_lag, cmpr_b_lead, cmpr_b_lag);
    return ESP_OK;
}

esp_err_t pspwm_up_down_ctr_mode_set_ps_duty(mcpwm_unit_t mcpwm_num,
                                             float ps_duty)
{
    DBG("Call pspwm_up_down_ctr_mode_set_ps_duty");
    if (ps_duty < 0 || ps_duty > 1) {
        ERROR("Invalid setpoint value for ps_duty");
        return ESP_FAIL;
    }
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

void _pspwm_up_down_ctr_mode_register_base_setup(mcpwm_unit_t mcpwm_num) {
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Timer and deadtime module clock prescaler/divider configuration
    // Datasheet 16.1: PWM_CLK_CFG_REG (0x0000)
    // Hardware prescales by register value plus one, thus subtracting it here
    MCPWM[mcpwm_num]->clk_cfg.prescale = BASE_CLK_PRESCALE - 1;

    for (int timer_i=0; timer_i < 2; ++timer_i){
        // Datasheet 16.2: PWM_TIMER0_CFG0_REG (0x0004) etc.
        // Hardware prescales by register value plus one, thus subtracting it here
        MCPWM[mcpwm_num]->timer[timer_i].period.prescale = TIMER_CLK_PRESCALE - 1;
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
        /* Fault Handler ("Trip-Zone") input configuration.
         * Set up one-shot (stay-off) mode for fault handler 0
         * This is used for software-forced output disabling.
         */
        // Datasheet 16.27: PWM_FH0_CFG0_REG (0x0068)
        // Enable sw-forced one-shot tripzone action
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.sw_ost = 1;
        // Uncomment to enable sw-forced cycle-by-cycle tripzone action
        //MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.sw_cbc = 1;
        // Enable hardware-forced (event f0) one-shot tripzone action
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.f0_ost = 1;
        // Uncomment to enable hardware (event f0) cycle-by-cycle tripzone action
        //MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.f0_cbc = 1;
        // Configure the kind of action (pull up / pull down)
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.a_ost_d = TRIPZONE_ACTION_PWMxA;
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.a_ost_u = TRIPZONE_ACTION_PWMxA;
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.b_ost_d = TRIPZONE_ACTION_PWMxB;
        MCPWM[mcpwm_num]->channel[timer_i].tz_cfg0.b_ost_u = TRIPZONE_ACTION_PWMxB;
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
    // Toggle triggers a "forced register update" whatever that means..
    MCPWM[mcpwm_num]->update_cfg.global_force_up ^= 1;
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
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg1.force_ost ^= 1;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg1.force_ost ^= 1;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}

esp_err_t pspwm_resync_enable_output(mcpwm_unit_t mcpwm_num)
{
    DBG("Enabling output!");
    portENTER_CRITICAL(&mcpwm_spinlock);
    // Toggle triggers the sync
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_0].sync.sync_sw ^= 1;
    MCPWM[mcpwm_num]->timer[MCPWM_TIMER_1].sync.sync_sw ^= 1;
    // Toggle clears the fault event
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_0].tz_cfg1.clr_ost ^= 1;
    MCPWM[mcpwm_num]->channel[MCPWM_TIMER_1].tz_cfg1.clr_ost ^= 1;
    portEXIT_CRITICAL(&mcpwm_spinlock);
    return ESP_OK;
}

esp_err_t pspwm_get_setpoint_limits(mcpwm_unit_t mcpwm_num,
                                    setpoint_limits_t** setpoint_limits) {
    if (!setpoint_limits[mcpwm_num]) {
        ERROR("ERROR: The PMW unit must be initialised first!");
        return ESP_FAIL;
    }
    *setpoint_limits = s_setpoint_limits;
    return ESP_OK;
}