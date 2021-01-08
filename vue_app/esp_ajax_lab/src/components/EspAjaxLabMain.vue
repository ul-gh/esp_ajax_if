<template>
  <div class="esp_ajax_lab">
    <!--Responsive grid layout for main content-->
    <ul class="grid_container main_grid">
      <li class="grid_item lr_center">
        <table>
          <thead>
            <tr>
              <th colspan="3">Operation Status</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Network Connection</td>
              <td>Power Output State</td>
              <td>HW Error Shutdown</td>
            </tr>
            <tr>
              <td>
                <div class="flex-centered state_vw" id="connection_vw"
                    :active="set_if(!disabled)">
                  <span style="white-space: pre">
                    {{ disabled ? "No Connection\nto Hardware!" : "OK" }}
                  </span>
                </div>
              </td>
              <td>
                <div class="flex-centered state_vw" id="power_pwm_vw"
                    :disabled="set_if(disabled)"
                    :active="set_if(state.power_active)"
                >
                  <span style="white-space: pre">
                    {{ state.power_active ? "Power PWM ON" : "Power PWM OFF" }}
                  </span>
                </div>
              </td>
              <td>
                <div class="flex-centered state_vw ajax_btn" id="shutdown_vw"
                    name="clear_shutdown" value="true"
                    :disabled="set_if(disabled)"
                    :fault_occurred="set_if(state.fault_occurred)"
                    :fault_present="set_if(state.fault_present)">
                  <span style="white-space: pre" v-if="!disabled">
                    {{ state.fault_present
                       ? "HW OC FAULT\nClick here to Reset!"
                       : "" }}
                    {{ state.fault_occurred && !state.fault_present
                       ? "HW OC fault latched!\nClick here to Reset!"
                       : "" }}
                    {{ !state.fault_occurred && !state.fault_present
                       ? "State: Normal"
                       : "" }}
                  </span>
                  {{ disabled ? "Unknown.." : "" }}
                </div>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item lr_center">
        <table>
          <thead>
            <tr>
              <th colspan="3">Temperatures and Fan</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Heatsink 1 Temp</td>
              <td>Heatsink 2 Temp</td>
              <td>Fan Override ON</td>
            </tr>
            <tr>
              <td>
                <div class="flex-centered number_vw"
                    :disabled="set_if(disabled)">
                  {{ state.aux_temp.toFixed(1) + " °C" }}
                </div>
              </td>
              <td>
                <div class="flex-centered number_vw"
                    :disabled="set_if(disabled)">
                  {{ state.heatsink_temp.toFixed(1) + " °C" }}
                </div>
              </td>
              <td>
                <label class="toggle-switchy">
                  <input type="checkbox"
                      name="set_fan_override"
                      @change="submit_btn"
                      :disabled="disabled"/>
                  <span class="toggle"><span class="switch"></span></span>
                </label>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item lr_center">
        <table>
          <thead>
            <tr>
              <th colspan="2">Dead Time Settings</th>
              <th colspan="1">All Settings</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Lead (ZVS) DT /ns</td>
              <td>Lag (ZCS) DT /ns</td>
              <td>Save All Settings</td>
            </tr>
            <tr>
              <td>
                <input type="number"
                    name="set_lead_dt" value="125"
                    min="6" max="1000"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
              <td>
                <input type="number"
                    name="set_lag_dt" value="125"
                    min="6" max="1000"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
              <td>
                <button class="ajax_btn" id="btn_save_settings"
                    name="save_settings" value="true"
                    @change="submit_btn"
                    :disabled="disabled">
                  Save!
                </button>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item lr_center">
        <table>
          <thead>
            <tr>
              <th colspan="3">Limit Values</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Current Limit /A</td>
              <td>Frequency MIN /kHz</td>
              <td>Frequency MAX /kHz</td>
            </tr>
            <tr>
              <td>
                <input type="number"
                    name="set_current_limit" value="8"
                    min="0" max="100"
                    step="1"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
              <td>
                <input type="number"
                    name="set_frequency_min" value="100"
                    min="5" max="2000"
                    step="1"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
              <td>
                <input type="number"
                    name="set_frequency_max" value="300"
                    min="5" max="2000"
                    step="1"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item lr_center">
        <table>
          <thead>
            <tr>
              <th colspan="2">Operation Settings</th>
            </tr>
          </thead>
          <tbody>
            <tr class="alternating_bg">
              <td>Frequency /kHz:</td>
              <td>
                <input type="number"
                    name="set_frequency" value="100"
                    min="5" max="1000"
                    step="1"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
            </tr>
            <tr class="alternating_bg">
              <td colspan="2">
                <input type="range"
                    name="set_frequency" value="100"
                    min="5" max="1000"
                    step="1"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
            </tr>

            <tr class="alternating_bg">
              <td>Duty Cycle /%:</td>
              <td>
                <input type="number"
                    name="set_duty" value="45.0"
                    min="0" max="100"
                    step="0.1"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
            </tr>
            <tr class="alternating_bg">
              <td colspan="2">
                <input type="range"
                    name="set_duty" value="45.0"
                    min="0" max="100"
                    step="0.1"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item lr_center">
        <table>
          <thead>
            <tr>
              <th colspan="3">Power Output Switches</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Power PWM</td>
              <td>REF/CAL Load</td>
              <td>DUT Output</td>
            </tr>
            <tr>
              <td>
                <span style="white-space: nowrap">
                  <button class="ajax_btn"
                      id="btn_pwm_on"
                      name="set_power_pwm_active" value="true"
                      :disabled="disabled">
                    ON
                  </button>
                  <button class="ajax_btn"
                      id="btn_pwm_off"
                      name="set_power_pwm_active" value="false"
                      :disabled="disabled">
                    OFF
                  </button>
                </span>
              </td>
              <td>
                <label class="toggle-switchy ajax_btn" data-size="lg">
                  <input type="checkbox"
                      name="set_relay_ref_active"
                      :disabled="disabled"/>
                  <span class="toggle"><span class="switch"></span></span>
                </label>
              </td>
              <td>
                <label class="toggle-switchy ajax_btn" data-size="lg">
                  <input type="checkbox"
                      name="set_relay_dut_active"
                      :disabled="disabled"/>
                  <span class="toggle"><span class="switch"></span></span>
                </label>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item lr_center">
        <table>
          <thead>
            <tr>
              <th colspan="2">Output Interval Timer</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Pulse Length /sec</td>
              <td>Trigger One-Shot</td>
            </tr>
            <tr>
              <td>
                <input type="number"
                    id="oneshot_len_vw"
                    name="set_oneshot_len" value="0.0"
                    min="0" max="1800"
                    step="0.001"
                    @change="submit_number"
                    :disabled="disabled"/>
              </td>
              <td>
                <button class="ajax_btn"
                    id="btn_trigger_oneshot"
                    name="trigger_oneshot" value="true"
                    :disabled="disabled">
                  TRIGGER!
                </button>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <!--End responsive grid layout-->
    </ul>

    <p>
      <a href="update.html">Perform OTA Firmware Upload</a>
    </p>
    <p>2020-11-18 Ulrich Lukas</p>
  </div>
</template>

<script>
export default {
  name: "EspAjaxLabMain",
  data() {
    return {};
  },
  props: {
    remote_state: Object,
    view_updates_inhibited: Boolean,
    disabled: Boolean,
  },
  computed: {
    state() {
      const rs = this.remote_state;
      return {
        frequency_min_hw: Number(rs.frequency_min_hw),
        frequency_max_hw: Number(rs.frequency_max_hw),
        dt_sum_max_hw: Number(rs.dt_sum_max_hw),
        // Runtime user setpoint limits for output frequency
        frequency_min: Number(rs.frequency_min),
        frequency_max: Number(rs.frequency_max),
        // Operational setpoints for PSPWM module
        frequency: Number(rs.frequency),
        duty: Number(rs.duty),
        lead_dt: Number(rs.lead_dt),
        lag_dt: Number(rs.lag_dt),
        power_active: Boolean(rs.power_pwm_active),
        // Settings for auxiliary HW control module
        current_limit: Number(rs.current_limit),
        relay_ref_active: Boolean(rs.relay_ref_active),
        relay_dut_active: Boolean(rs.relay_dut_active),
        // Temperatures and fan
        heatsink_temp: Number(rs.heatsink_temp),
        aux_temp: Number(rs.aux_temp),
        fan_active: Boolean(rs.fan_active),
        fan_override: Boolean(rs.fan_override),
        // Clock divider settings
        base_div: Number(rs.base_div),
        timer_div: Number(rs.timer_div),
        // Gate driver supply and disable signals
        drv_supply_active: Boolean(rs.drv_supply_active),
        drv_disabled: Boolean(rs.drv_disabled),
        // True when hardware OC shutdown condition is present
        fault_present: Boolean(rs.hw_oc_fault_present),
        // Hardware Fault Shutdown Status is latched using this flag
        fault_occurred: Boolean(rs.hw_oc_fault_occurred),
        // Length of the power output one-shot timer pulse
        oneshot_len: Number(rs.oneshot_len),
      };
    },
  },
  methods: {
    // Helper function for setting custom boolean attributes on any HTML element.
    // These are defined to be true when present and false when removed from the DOM.
    set_if(is_true) {
      return is_true ? "" : undefined;
    },
    // Submit number input, we emit an event with name and value
    submit_number(event) {
      this.$emit("submit_cmd",
                 event.target.name,
                 Number(event.target.value));
    },
    // When clicking on a button, we want to toggle its state, so requested value is inverse
    submit_btn(event) {
      this.$emit("submit_cmd",
                 event.target.name,
                 !event.target.value);
    },
  },
  emits: ["submit_cmd"],
};
</script>

<!-- Add "scoped" attribute to limit CSS to this component only -->
<style scoped src="./main_app.css">
</style>
<style scoped src="./toggle-switchy.css">
</style>
