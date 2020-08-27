#ifndef APP_HW_DRV_HPP__
#define APP_HW_DRV_HPP__


class AuxHwDrv
{
public:
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