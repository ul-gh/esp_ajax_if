/* Use Espressif ESP32 platform MCPWM hardware module for output of
 * Phase-Shift-PWM waveform on 4 hardware pins.
 * 
 * Application in ZVS-PS-PWM, DAB-DCM and LLC power electronics converters.
 * 
 * This depends on the ESP-IDF SDK
 *
 * 2019-12-04 Ulrich Lukas
 */
#ifndef _PS_PWM_H__
#define _PS_PWM_H__

#include "driver/mcpwm.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG // Set to ESP_LOG_INFO for production!
#include "esp_log.h"
#define DBG(...) ESP_LOGD("Module: ps_pwm", __VA_ARGS__)
#define INFO(...) ESP_LOGI("Module: ps_pwm", __VA_ARGS__)
#define ERROR(...) ESP_LOGE("Module: ps_pwm", __VA_ARGS__)

//#define GPIO_SYNC0_IN   2   //Set GPIO 02 as SYNC0

#define MCPWM_BASE_CLK 160000000 // 160 MHz
#define BASE_CLK_PRESCALE 3 // Hardware adds 1 to this value
#define TIMER_CLK_PRESCALE 3 // Hardware adds 1 to this value
#define MCPWM_CLK (MCPWM_BASE_CLK/(BASE_CLK_PRESCALE + 1))

// Define here if the output pins shall be forced low or high
// or else when a fault condition is triggered
#define TRIPZONE_ACTION_PWMxA MCPWM_FORCE_MCPWMXA_LOW
#define TRIPZONE_ACTION_PWMxB MCPWM_FORCE_MCPWMXB_LOW

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************
 * FULL-SPEED-MODE, 4x INDIVIDUAL DEAD-TIME, HW-DEAD-TIME-MODULE *
 *****************************************************************
 *
 * Initialize ESP32 MCPWM hardware module for output of
 * Phase-Shift-PWM waveform on 4 hardware pins.
 * 
 * This runs the PS-PWM generator module in up-counting mode,
 * allowing 4x individual dead-time values for rising and falling
 * edges for all four PWM outputs.
 * 
 * To achieve this, this uses the hardware dead-band generator while
 * also calculating and setting complementary timing values for
 * the PWM operator compare registers.
 * 
 * Combined output waveform of the phase-shifted full-bridge
 * is DC-free symmetric nevertheless.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * int gpio_lead_a, int gpio_lead_b, int gpio_lag_a, int gpio_lag_b:
 *      GPIOs used as outputs
 * float frequency: Frequency of the non-rectified waveform in Hz,
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 * float lead_red: dead time value for rising edge, leading leg
 * float lead_fed: dead time value for falling edge, leading leg
 * float lag_red: dead time value for rising edge, lagging leg
 * float lag_fed: dead time value for falling edge, lagging leg 
 */
esp_err_t pspwm_up_ctr_mode_init(
        mcpwm_unit_t mcpwm_num,
        int gpio_lead_a, int gpio_lead_b, int gpio_lag_a, int gpio_lag_b,
        float frequency,
        float ps_duty,
        float lead_red, float lead_fed, float lag_red, float lag_fed);

/* Set frequency when running PS-PWM generator in up-counting mode
 * 
 * This does not alter prescaler settings.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float frequency: Frequency of the non-rectified waveform in Hz,
 */
esp_err_t pspwm_up_ctr_mode_set_frequency(mcpwm_unit_t mcpwm_num,
                                          float frequency);

/* Set deadtime values individually for leading leg rising and
 * falling edge as well as for lagging leg rising and falling edge
 * for all four PWM outputs.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float lead_red: dead time value for rising edge, leading leg
 * float lead_fed: dead time value for falling edge, leading leg
 * float lag_red: dead time value for rising edge, lagging leg
 * float lag_fed: dead time value for falling edge, lagging leg
 */
esp_err_t pspwm_up_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                          float lead_red, float lead_fed,
                                          float lag_red, float lag_fed);

/* Set PS-PWM phase shift based on the current period time setting
 * i.e. state of the PWM hardware "period" register.
 * 
 * The phase-shift value is valid for the individual deadtime setting.
 *
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_up_ctr_mode_set_ps_duty(mcpwm_unit_t mcpwm_num, float ps_duty);



/*****************************************************************
 * TIMER UP/DOWN-COUNTING MODE; DOES NOT USE HW-DEAD-TIME-MODULE *
 *****************************************************************
 *
 * Initialize ESP32 MCPWM hardware module for output of a
 * Phase-Shift-PWM waveform on 4 hardware pins.
 * 
 * Individual dead-times are configured for both half-bridge PWM outputs.
 * 
 * This PWM generation mode does not use the dead-band generator hardware.
 * Instead, the dead-time for each two outputs of a half-bridge is configured
 * by running the main timers in up-down-counting mode and using both compare
 * registers for each timer to generate two symmetric outputs.
 * 
 * Because of the up/down-counting mode, maximum output frequency is half of
 * the value which is possible with using the hardware deas-band generator.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * int gpio_lead_a, int gpio_lead_b, int gpio_lag_a, int gpio_lag_b:
 *      GPIOs used as outputs
 * float frequency: Frequency of the non-rectified waveform in Hz,
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 * float lead_dt: leading bridge-leg dead-time in sec (0..),
 * float lag_dt: lagging bridge-leg dead-time in sec (0..)
 */
esp_err_t pspwm_up_down_ctr_mode_init(
        mcpwm_unit_t mcpwm_num,
        int gpio_lead_a, int gpio_lead_b, int gpio_lag_a, int gpio_lag_b,
        float frequency,
        float ps_duty,
        float lead_dt, float lag_dt);

/* Set frequency (and update dead-time values) for all four output
 * signals of the phase-shift-PWM when using the timer
 * in up/down counting mode.
 * 
 * Because of the up/down-counting mode, maximum output frequency is half of
 * the value which is possible with using the hardware deas-band generator.
 * 
 * This does not alter prescaler settings.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float frequency: Frequency of the non-rectified waveform in Hz,
 */
esp_err_t pspwm_up_down_ctr_mode_set_frequency(mcpwm_unit_t mcpwm_num,
                                               float frequency);

/* Set dead-time values for all four output signals
 * of the phase-shift-PWM when using the timer
 * in up/down counting mode.
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float lead_dt: leading bridge-leg dead-time in sec (0..),
 * float lag_dt: lagging bridge-leg dead-time in sec (0..)
 */
esp_err_t pspwm_up_down_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                               float lead_dt, float lag_dt);

/* Set PS-PWM phase shift based on the current period time setting
 * i.e. state of the PWM hardware "period" register.
 *
 * The phase-shift value is valid for the symmetric dead-time setting
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_up_down_ctr_mode_set_ps_duty(mcpwm_unit_t mcpwm_num,
                                             float ps_duty);



/*****************************************************************
 *                         COMMON SETUP                          *
 *****************************************************************/

/* Disable PWM output immediately by software-triggering the one-shot
 * fault input of the "trip-zone" fault handler module.
 * 
 * This sets the PWM output pins to predefined levels TRIPZONE_ACTION_PWMxA
 * and TRIPZONE_ACTION_PWMxB, which can be configured in the header file.
 */
esp_err_t pspwm_disable_output(mcpwm_unit_t mcpwm_num);

/* (Re-)enable PWM output by clearing fault handler one-shot trigger
 * after software-triggering a re-sync to the initial phase setpoint.
 */
esp_err_t pspwm_resync_enable_output(mcpwm_unit_t mcpwm_num);


#ifdef __cplusplus
}
#endif

#endif  /* _PS_PWM_H__ */