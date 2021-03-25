<!-- ESP Ajax Lab Main Hardware Controller Component
-->
<template>
  <div class="live_controller">
    <!--Responsive grid layout for main content-->
    <ul class="grid_container main_grid">
      <li class="grid_item">
        <table>
          <thead>
            <tr>
              <th colspan="3">Hardware Status Display</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <th>Network Connection</th>
              <th>Power Output State</th>
              <th>HW Error Shutdown</th>
            </tr>
            <tr>
              <td>
                <LiveColorIndicatorBtn
                  inactive_text="OK"
                  :disabled_text="'No Connection\nto Hardware!'"
                  disabled_color="rgb(250, 84, 84)"
                  :disabled="disabled"
                />
              </td>
              <td>
                <LiveColorIndicatorBtn
                  :value_feedback="state.power_pwm_active"
                  inactive_text="Power PWM OFF"
                  active_text="Power PWM ON"
                  active_color="red"
                  :disabled="disabled"
                />
              </td>
              <td>
                <LiveColorIndicatorBtn
                  change_action="clear_shutdown"
                  :value_feedback="Boolean(state.hw_error)"
                  inactive_text="State: Normal"
                  :active_text="`${state.hw_error}\nClick here to Reset!`"
                  :disabled="disabled"
                  @action_triggered="dispatch_nv"
                />
              </td>
            </tr>
            <!-- Temperatures and FAN -->
            <tr>
              <th>Heatsink 1 Temp</th>
              <th>Heatsink 2 Temp</th>
              <th>Fan Override ON</th>
            </tr>
            <tr>
              <td>
                <div class="flex-stacked-calign"
                    :disabled="set_if(disabled)">
                  {{ state.temp_1.toFixed(1) + "&nbsp;°C" }}
                </div>
              </td>
              <td>
                <div class="flex-stacked-calign"
                    :disabled="set_if(disabled)">
                  {{ state.temp_2.toFixed(1) + "&nbsp;°C" }}
                </div>
              </td>
              <td>
                <LiveToggleSwitch
                  change_action="set_fan_override"
                  :value_feedback="state.fan_override"
                  :disabled="disabled"
                  @action_triggered="dispatch_nv"
                />
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item">
        <table>
          <thead>
            <tr>
              <th colspan="2">Operation Settings</th>
            </tr>
          </thead>
          <tbody>
            <tr class="alternating_bg">
              <th>Frequency [kHz]:</th>
              <td>
                <span class="flex-centered-row">
                    <span class="flex-stacked-calign">
                        <span>Min:</span>
                        {{state.frequency_min.toFixed(2)}}
                    </span>
                    <LiveNumberInput
                        change_action="set_frequency"
                        :value_feedback="state.frequency"
                        :min="state.frequency_min" :max="state.frequency_max"
                        :digits="2"
                        :disabled="disabled"
                        @action_triggered="dispatch_nv"
                    />
                    <span class="flex-stacked-calign">
                        <span>Max:</span>
                        {{state.frequency_max.toFixed(2)}}
                    </span>
                    <label>
                        &nbsp;&nbsp;
                        <input
                            type="radio"
                            name="f_range_dial"
                            :value="false"
                            v-model="f_dial_active"
                        >
                        Slider
                    </label>
                    <label>
                        <input
                            type="radio"
                            name="f_range_dial"
                            :value="true"
                            v-model="f_dial_active"
                        >
                        Dial
                    </label>
                </span>
              </td>
            </tr>
            <tr class="alternating_bg">
              <td colspan="2">
                <keep-alive>
                  <LiveRangeInput
                      v-if="!f_dial_active"
                      change_action="set_frequency"
                      :value_feedback="state.frequency"
                      :min="state.frequency_min" :max="state.frequency_max"
                      :digits="2"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <LiveJogDial
                      v-else
                      change_action="set_frequency"
                      :value_feedback="state.frequency"
                      :min="state.frequency_min" :max="state.frequency_max"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                </keep-alive>
              </td>
            </tr>

            <tr class="alternating_bg">
              <th>Duty Cycle [%]:</th>
              <td>
                <span class="flex-centered-row">
                    <span class="flex-stacked-calign">
                        <span>Min:</span>
                        {{state.duty_min.toFixed(1)}}
                    </span>
                    <LiveNumberInput
                        change_action="set_duty"
                        :value_feedback="state.duty"
                        :min="state.duty_min" :max="state.duty_max"
                        :digits="1"
                        :disabled="disabled"
                        @action_triggered="dispatch_nv"
                    />
                    <span class="flex-stacked-calign">
                        <span>Max:</span>
                        {{state.duty_max.toFixed(1)}}
                    </span>
                    <label>
                        &nbsp;&nbsp;
                        <input
                            type="radio"
                            name="d_range_dial"
                            :value="false"
                            v-model="d_dial_active"
                        >
                        Slider
                    </label>
                    <label>
                        <input
                            type="radio"
                            name="d_range_dial"
                            :value="true"
                            v-model="d_dial_active"
                        >
                        Dial
                    </label>
                </span>
              </td>
            </tr>
            <tr class="alternating_bg">
              <td colspan="2">
                <keep-alive>
                  <LiveRangeInput
                      v-if="!d_dial_active"
                      change_action="set_duty"
                      :value_feedback="state.duty"
                      :min="state.duty_min" :max="state.duty_max"
                      :digits="1"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <LiveJogDial
                      v-else
                      change_action="set_duty"
                      :value_feedback="state.duty"
                      :min="state.duty_min" :max="state.duty_max"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                </keep-alive>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item">
        <table>
          <thead>
            <tr>
              <th colspan="3">Power Output Switches</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <th>Power PWM</th>
              <th>REF/CAL Load</th>
              <th>DUT Output</th>
            </tr>
            <tr>
              <td>
                <span style="white-space: nowrap">
                  <button
                      id="btn_pwm_on"
                      name="set_power_pwm_active"
                      value="true"
                      :disabled="disabled"
                      @click="dispatch_btn"
                  >
                    ON
                  </button>
                  <button
                      id="btn_pwm_off"
                      name="set_power_pwm_active"
                      value="false"
                      :disabled="disabled"
                      @click="dispatch_btn"
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
                  @action_triggered="dispatch_nv"
                />
              </td>
              <td>
                <LiveToggleSwitch
                  data-size="lg"
                  change_action="set_relay_dut_active"
                  :value_feedback="state.relay_dut_active"
                  :disabled="disabled"
                  @action_triggered="dispatch_nv"
                />
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item">
        <table>
          <thead>
            <tr>
              <th colspan="2">Output Interval Timer</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <th>Pulse Length [s]</th>
              <th>Trigger One-Shot</th>
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
                    @action_triggered="dispatch_nv"
                />
              </td>
              <td>
                <button
                    id="btn_trigger_oneshot"
                    name="trigger_oneshot"
                    value="true"
                    :disabled="disabled"
                    @click="dispatch_btn"
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
  </div>
</template>

<script>
import LiveColorIndicatorBtn from './widgets/LiveColorIndicatorBtn.vue'
import LiveToggleSwitch from './widgets/LiveToggleSwitch.vue';
import LiveNumberInput from './widgets/LiveNumberInput.vue';
import LiveRangeInput from './widgets/LiveRangeInput.vue';
import LiveJogDial from './widgets/LiveJogDial.vue';

export default {
  name: "LiveController",
  components: {
      LiveColorIndicatorBtn,
      LiveToggleSwitch,
      LiveNumberInput,
      LiveRangeInput,
      LiveJogDial,
  },
  data() {
    return {
        f_dial_active: false,
        d_dial_active: false,
    };
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
    dispatch_btn(event) {
      this.$emit("action", event.target.name, event.target.value);
    },
    // Submit name=value pair
    dispatch_nv(name, value) {
        this.$emit("action", name, value);
    },
  },
  emits: ["action"],
};
</script>


<style scoped>
    #btn_pwm_on {
        display: inline-block;
        font-size: 1.3rem;
        padding: 0.8rem 1.3rem;
        color: white;
        background-color: #4CAF50; /* Green */
    }

    #btn_pwm_off {
        display: inline-block;
        font-size: 1.3rem;
        padding: 0.8rem 1.3rem;
        color: white;
        background-color: red;
    }

    #btn_trigger_oneshot {
        display: inline-block;
        font-size: 1.3rem;
        padding: 0.8rem 1.3rem;
        color: white;
        background-color: orange;
    }
</style>
