#ifndef APP_HW_DRV_HPP__
#define APP_HW_DRV_HPP__


class AuxHwDrv
{
public:
    // GPIO config
    static constexpr int gpio_relay_ref{};
    static constexpr int gpio_relay_dut{};
    static constexpr int gpio_fan{};
    static constexpr int gpio_curr_limit_reference_pwm{17};
    static constexpr int gpio_overcurrent_reset{16};
    // Setpoints are public in order to be read-accessed by PsPwmAppHwControl
    float current_limit;
    bool relay_ref_active;
    bool relay_dut_active;
    bool fan_active;

    AuxHwDrv(float current_limit, bool relay_ref_active,
             bool relay_dut_active, bool fan_active);

    void set_current_limit(float value);
    void set_relay_ref_active(bool state);
    void set_relay_dut_active(bool state);
    void set_fan_active(bool state);

private:

};

#endif