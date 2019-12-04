/* Use Espressif ESP32 platform MCPWM hardware module for output of
 * Phase-Shift-PWM waveform on 4 hardware pins.
 * 
 * Application in ZVS-PS-PWM, DAB-DCM and LLC power electronics converters.
 * 
 * This depends on the ESP-IDF SDK
 *
 * 2019-10-28 Ulrich Lukas
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

#define TRIPZONE_ACTION_PWMxA MCPWM_FORCE_MCPWMXA_LOW
#define TRIPZONE_ACTION_PWMxB MCPWM_FORCE_MCPWMXB_LOW

#ifdef __cplusplus
extern "C" {
#endif

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
        float lag_red, float lag_fed);

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
        mcpwm_unit_t mcpwm_num, float frequency, float leg_duty);

/* Set PS-PWM phase shift based on the current period time setting
 * i.e. state of the PWM hardware "period" register.
 * 
 * The phase-shift value is valid for the individual deadtime setting.
 *
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_individual_dt_set_ps_duty(mcpwm_unit_t mcpwm_num, float ps_duty);

/* Set PS-PWM phase shift based on the current period time setting
 * i.e. state of the PWM hardware "period" register.
 *
 * The phase-shift value is valid for the symmetric dead-time setting
 * 
 * Parameters:
 * int mcpwm_num: PWM unit number ([0|1]),
 * float ps_duty: Duty cycle of the rectified waveform (0..1)
 */
esp_err_t pspwm_symmetric_dt_set_ps_duty(mcpwm_unit_t mcpwm_num, float ps_duty);

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
        float lag_red, float lag_fed);

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
        mcpwm_unit_t mcpwm_num, float frequency, float ps_duty, float leg_duty);

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
        float lead_red, float lead_fed, float lag_red, float lag_fed);

void pspwm_setup_gpios(mcpwm_unit_t mcpwm_num,
                       uint32_t lead_a, uint32_t lead_b,
                       uint32_t lag_a, uint32_t lag_b);

#ifdef __cplusplus
}
#endif

#endif  /* _PS_PWM_H__ */