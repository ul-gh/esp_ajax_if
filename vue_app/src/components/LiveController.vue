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
                    :active="set_if(state.power_pwm_active)"
                >
                  <span style="white-space: pre">
                    {{ state.power_pwm_active ? "Power PWM ON" : "Power PWM OFF" }}
                  </span>
                </div>
              </td>
              <td>
                <LiveErrorIndicatorBtn
                  change_action="clear_shutdown"
                  :value_feedback="state.hw_oc_fault_present"
                  ok_text="State: Normal"
                  :error_text="'HW OC FAULT\nClick here to Reset!'"
                  :disabled="disabled"
                  @action_triggered="submit_nv"
                />
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
                <LiveToggleSwitch
                  change_action="set_fan_override"
                  :value_feedback="state.fan_override"
                  :disabled="disabled"
                  @action_triggered="submit_nv"
                />
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
                <LiveNumberInput
                    change_action="set_lead_dt"
                    :value_feedback="state.lead_dt"
                    :min="6" :max="5000"
                    :digits="0"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
              </td>
              <td>
                <LiveNumberInput
                    change_action="set_lag_dt"
                    :value_feedback="state.lag_dt"
                    :min="6" :max="5000"
                    :digits="0"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
              </td>
              <td>
                <button class="ajax_btn" id="btn_save_settings"
                    name="save_settings"
                    value="true"
                    :disabled="disabled"
                    @click="submit_btn"
                >
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
                <LiveNumberInput
                    change_action="set_current_limit"
                    :value_feedback="state.current_limit"
                    :min="0" :max="100"
                    :digits="0"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
              </td>
              <td>
                <LiveNumberInput
                    change_action="set_frequency_min"
                    :value_feedback="state.frequency_min"
                    :min="0.00" :max="5000.00"
                    :digits="2"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
              </td>
              <td>
                <LiveNumberInput
                    change_action="set_frequency_max"
                    :value_feedback="state.frequency_max"
                    :min="0.00" :max="5000.00"
                    :digits="2"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
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
                <LiveNumberInput
                    change_action="set_frequency"
                    :value_feedback="state.frequency"
                    :min="state.frequency_min" :max="state.frequency_max"
                    :digits="2"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
              </td>
            </tr>
            <tr class="alternating_bg">
              <td colspan="2">
                <LiveRangeInput
                    change_action="set_frequency"
                    :value_feedback="state.frequency"
                    :min="state.frequency_min" :max="state.frequency_max"
                    :digits="2"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
              </td>
            </tr>

            <tr class="alternating_bg">
              <td>Duty Cycle /%:</td>
              <td>
                <LiveNumberInput
                    change_action="set_duty"
                    :value_feedback="state.duty"
                    :min="0" :max="100"
                    :digits="1"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
              </td>
            </tr>
            <tr class="alternating_bg">
              <td colspan="2">
                <LiveRangeInput
                    change_action="set_duty"
                    :value_feedback="state.duty"
                    :min="0" :max="100"
                    :digits="1"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
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
                      name="set_power_pwm_active"
                      value="true"
                      :disabled="disabled"
                      @click="submit_btn"
                  >
                    ON
                  </button>
                  <button class="ajax_btn"
                      id="btn_pwm_off"
                      name="set_power_pwm_active"
                      value="false"
                      :disabled="disabled"
                      @click="submit_btn"
                  >
                    OFF
                  </button>
                </span>
              </td>
              <td>
                <LiveToggleSwitch
                  data-size="lg"
                  change_action="set_relay_ref_active"
                  :value_feedback="state.relay_ref_active"
                  :disabled="disabled"
                  @action_triggered="submit_nv"
                />
              </td>
              <td>
                <LiveToggleSwitch
                  data-size="lg"
                  change_action="set_relay_dut_active"
                  :value_feedback="state.relay_dut_active"
                  :disabled="disabled"
                  @action_triggered="submit_nv"
                />
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
                <LiveNumberInput
                    change_action="set_oneshot_len"
                    :value_feedback="state.oneshot_len"
                    :min="0"
                    :max="1800"
                    :digits="3"
                    :disabled="disabled"
                    @action_triggered="submit_nv"
                />
              </td>
              <td>
                <button class="ajax_btn"
                    id="btn_trigger_oneshot"
                    name="trigger_oneshot"
                    value="true"
                    :disabled="disabled"
                    @click="submit_btn"
                >
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
    <p>2021-02-27 Ulrich Lukas</p>
  </div>
</template>

<script>
import LiveErrorIndicatorBtn from './LiveErrorIndicatorBtn.vue'
import LiveToggleSwitch from './LiveToggleSwitch.vue';
import LiveNumberInput from './LiveNumberInput.vue';
import LiveRangeInput from './LiveRangeInput.vue';

export default {
  name: "LiveController",
  components: {
      LiveErrorIndicatorBtn,
      LiveToggleSwitch,
      LiveNumberInput,
      LiveRangeInput,
  },
  data() {
    return {};
  },
  props: {
    state: Object,
    disabled: Boolean,
  },
  methods: {
    // Helper function for setting custom boolean attributes on any HTML element.
    // These are defined to be true when present and false when removed from the DOM.
    set_if(is_true) {
      return is_true ? "" : undefined;
    },
    // Push buttons can have a name and value
    submit_btn(event) {
      console.log(event);
      this.$emit("submit_cmd", event.target.name, event.target.value);
    },
    // Submit name=value pair
    submit_nv(name, value) {
        console.log("submitting " + name + "=" + value);
        this.$emit("submit_cmd", name, value);
    },
  },
  emits: ["submit_cmd"],
};
</script>


<style>
</style>
