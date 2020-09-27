#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "driver/periph_ctrl.h"
// ADC internal acquisition registers
#include <soc/sens_reg.h>
#include <soc/sens_struct.h>

// Hardware timer clock divider
#define TIMER_DIVIDER 16
// TIMER_BASE_CLK = APB_CLK_FREQ = 80 MHz for ESP32
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)
#define INTERVAL_SEC (0.01)

#define ADC_SAMPLES_COUNT 1000
int16_t abuf[ADC_SAMPLES_COUNT];
int16_t abufPos = 0;

static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED; 

inline uint32_t IRAM_ATTR local_adc1_read(const uint32_t channel) {
    uint32_t adc_value;
    SENS.sar_meas_start1.sar1_en_pad = (1 << channel);
    while (SENS.sar_slave_addr1.meas_status != 0) {};
    SENS.sar_meas_start1.meas1_start_sar = 0;
    SENS.sar_meas_start1.meas1_start_sar = 1;
    while (SENS.sar_meas_start1.meas1_done_sar == 0) {};
    adc_value = SENS.sar_meas_start1.meas1_data_sar;
    return adc_value;
}

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);

  abuf[abufPos] = local_adc1_read(ADC1_CHANNEL_0);
  
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup_and_run_dsp_backend()
{
    xTaskCreate(complexHandler, "Handler Task", 8192, NULL, 1, &complexHandlerTask);

    timer_idx_t timer_idx = TIMER_0;
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true,
    }; // default clock source is APB
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, INTERVAL_SEC * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
                       (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}