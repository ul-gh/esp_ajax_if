/** @file ps_pwm.h
 * Use Espressif ESP32 platform MCPWM hardware module for generating
 * Phase-Shift-PWM waveform on 4 hardware pins.
 * @note Application in power electronics, e.g.
 *       ZVS-PS-PWM, DAB-DCM and LLC converters.
 * 
 * @note This depends on the ESP-IDF SDK
 *
 * 2019-12-11 Ulrich Lukas
 */
#ifndef __PS_PWM_H__
#define __PS_PWM_H__

#include "driver/mcpwm.h"

// Set log level to ESP_LOG_INFO for production!
#define PS_PWM_LOG_LEVEL ESP_LOG_DEBUG

//#define GPIO_SYNC0_IN   2   //Set GPIO 02 as SYNC0

// Unscaled input clock frequency
#define MCPWM_INPUT_CLK 160000000 // 160 MHz
// Hardware prescaler factor for input clock.
// Dead time generators are configured to run on this scaled clock signal
#define BASE_CLK_PRESCALE 4
#define MCPWM_BASE_CLK (MCPWM_INPUT_CLK / BASE_CLK_PRESCALE)
// Hardware prescaler factor for timer operator sub-modules
#define TIMER_CLK_PRESCALE 4
#define MCPWM_TIMER_CLK (MCPWM_BASE_CLK / TIMER_CLK_PRESCALE)

// Define here if the output pins shall be forced low or high
// or else when a fault condition is triggered
#define TRIPZONE_ACTION_PWMxA MCPWM_FORCE_MCPWMXA_LOW
#define TRIPZONE_ACTION_PWMxB MCPWM_FORCE_MCPWMXB_LOW

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************//**
 *    FULL-SPEED-MODE, 4x INDIVIDUAL DEAD-TIME, HW-DEAD-TIME-MODULE
 ************************************************************************
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
 * @param mcpwm_num: PWM unit number ([0|1]),
 * @param gpio_lead_a: GPIO number leading leg low_side
 * @param gpio_lead_b: GPIO number leading leg high_side
 * @param gpio_lag_a: GPIO number lagging leg low_side
 * @param gpio_lag_b: GPIO number lagging leg high_side
 * @param frequency: Frequency of the non-rectified waveform in Hz,
 * @param ps_duty: Duty cycle of the rectified waveform (0..1)
 * @param lead_red: dead time value for rising edge, leading leg
 * @param lead_fed: dead time value for falling edge, leading leg
 * @param lag_red: dead time value for rising edge, lagging leg
 * @param lag_fed: dead time value for falling edge, lagging leg 
 */
esp_err_t pspwm_up_ctr_mode_init(
        mcpwm_unit_t mcpwm_num,
        int gpio_lead_a, int gpio_lead_b, int gpio_lag_a, int gpio_lag_b,
        float frequency,
        float ps_duty,
        float lead_red, float lead_fed, float lag_red, float lag_fed);

/** Set frequency when running PS-PWM generator in up-counting mode
 * @note
 * This does not alter prescaler settings.
 * 
 * @param mcpwm_num: PWM unit number ([0|1]),
 * @param frequency: Frequency of the non-rectified waveform in Hz,
 */
esp_err_t pspwm_up_ctr_mode_set_frequency(mcpwm_unit_t mcpwm_num,
                                          float frequency);

/** Set deadtime values individually for leading leg rising and
 * falling edge as well as for lagging leg rising and falling edge
 * for all four PWM outputs.
 * 
 * @param  mcpwm_num: PWM unit number ([0|1]),
 * @param  lead_red: dead time value for rising edge, leading leg
 * @param  lead_fed: dead time value for falling edge, leading leg
 * @param  lag_red: dead time value for rising edge, lagging leg
 * @param  lag_fed: dead time value for falling edge, lagging leg
 */
esp_err_t pspwm_up_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                          float lead_red, float lead_fed,
                                          float lag_red, float lag_fed);

/** Set PS-PWM phase shift between lead and lag leg output pairs
 * 
 * This is expressed in units of one where 1.0 means 180° phase-shift
 * for the output pairs, giving maximum duty cycle after rectification.
 * @note The value is calculated based on current value of the
 *       PWM timer "period" register.
 *
 * @param  mcpwm_num: PWM unit number ([0|1]),
 * @param  ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_up_ctr_mode_set_ps_duty(mcpwm_unit_t mcpwm_num, float ps_duty);



/*************************************************************//**
 * TIMER UP/DOWN-COUNTING MODE; DOES NOT USE HW-DEAD-TIME-MODULE
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
 * the value which is possible when using the hardware dead-band generator.
 * 
 * @param mcpwm_num: PWM unit number ([0|1]),
 * @param gpio_lead_a: GPIO number leading leg low_side
 * @param gpio_lead_b: GPIO number leading leg high_side
 * @param gpio_lag_a: GPIO number lagging leg low_side
 * @param gpio_lag_b: GPIO number lagging leg high_side
 * @param frequency: Frequency of the non-rectified waveform in Hz,
 * @param ps_duty: Duty cycle of the rectified waveform (0..1)
 * @param lead_dt: leading bridge-leg dead-time in sec (0..),
 * @param lag_dt: lagging bridge-leg dead-time in sec (0..)
 */
esp_err_t pspwm_up_down_ctr_mode_init(
        mcpwm_unit_t mcpwm_num,
        int gpio_lead_a, int gpio_lead_b, int gpio_lag_a, int gpio_lag_b,
        float frequency,
        float ps_duty,
        float lead_dt, float lag_dt);

/** Set frequency (and update dead-time values) for all four output
 * signals of the phase-shift-PWM when using the timer
 * in up/down counting mode.
 * 
 * Because of the up/down-counting mode, maximum output frequency is half of
 * the value which is possible when using the hardware dead-band generator.
 * 
 * @note
 * This does not alter prescaler settings.
 * 
 * @param mcpwm_num: PWM unit number ([0|1]),
 * @param frequency: Frequency of the non-rectified waveform in Hz,
 */
esp_err_t pspwm_up_down_ctr_mode_set_frequency(mcpwm_unit_t mcpwm_num,
                                               float frequency);

/** Set dead-time values for all four output signals
 * of the phase-shift-PWM when using the timer
 * in up/down counting mode.
 * 
 * @param mcpwm_num: PWM unit number ([0|1]),
 * @param lead_dt: leading bridge-leg dead-time in sec (0..),
 * @param lag_dt: lagging bridge-leg dead-time in sec (0..)
 */
esp_err_t pspwm_up_down_ctr_mode_set_deadtimes(mcpwm_unit_t mcpwm_num,
                                               float lead_dt, float lag_dt);

/** Set PS-PWM phase shift based on the current period time setting
 * i.e. state of the PWM hardware "period" register.
 *
 * The phase-shift value is valid for the symmetric dead-time setting
 * 
 * @param mcpwm_num: PWM unit number ([0|1]),
 * @param ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_up_down_ctr_mode_set_ps_duty(mcpwm_unit_t mcpwm_num,
                                             float ps_duty);



/*****************************************************************
 *                         COMMON SETUP
 *****************************************************************/

/** Disable PWM output immediately by software-triggering the one-shot
 * fault input of the "trip-zone" fault handler module.
 * 
 * This sets the PWM output pins to predefined levels TRIPZONE_ACTION_PWMxA
 * and TRIPZONE_ACTION_PWMxB, which can be configured in the header file.
 * 
 * @param mcpwm_num: PWM unit number ([0|1]),
 */
esp_err_t pspwm_disable_output(mcpwm_unit_t mcpwm_num);

/** (Re-)enable PWM output by clearing fault handler one-shot trigger
 * after software-triggering a re-sync to the initial phase setpoint.
 * 
 * @param mcpwm_num: PWM unit number ([0|1]),
 */
esp_err_t pspwm_resync_enable_output(mcpwm_unit_t mcpwm_num);


#ifdef __cplusplus
}
#endif

#endif  /* __PS_PWM_H__ */