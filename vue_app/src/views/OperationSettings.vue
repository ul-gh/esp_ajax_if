<!-- ESP Ajax Lab Operation Specific Settings Component
-->
<template>
  <div class="operation_settings">
    <!--Responsive grid layout for main content-->
    <ul class="grid_container main_grid">
      <li class="grid_item">
        <table>
          <thead>
            <tr>
              <th colspan="2">Limit Values</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <th>Overtemperature Limit<br>Heat Sink 1 [°C]</th>
              <th>Overtemperature Limit<br>Heat Sink 2 [°C]</th>
            </tr>
            <tr>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_temp_1_limit"
                      :value_feedback="state.temp_1_limit"
                      :min="0" :max="150"
                      :digits="0"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    HW Limit:<br>Suggested: 50&nbsp;°C<br>(Sensor: 150&nbsp;°C..)
                  </span>
                </span>
              </td>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_temp_2_limit"
                      :value_feedback="state.temp_2_limit"
                      :min="0" :max="150"
                      :digits="0"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    HW Limit:<br>Suggested: 50&nbsp;°C<br>(Sensor: 150&nbsp;°C..)
                  </span>
                </span>
              </td>
            </tr>

            <tr>
              <th>Frequency Min [kHz]</th>
              <th>Frequency Max [kHz]</th>
            </tr>
            <tr>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_frequency_min"
                      :value_feedback="state.frequency_min"
                      :min="state.frequency_min_hw" :max="state.frequency_max_hw"
                      :digits="2"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    HW Limit:<br>
                    {{state.frequency_min_hw.toFixed(2)}}&nbsp;kHz
                  </span>
                </span>
              </td>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_frequency_max"
                      :value_feedback="state.frequency_max"
                      :min="state.frequency_min_hw" :max="state.frequency_max_hw"
                      :digits="2"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    HW Limit:<br>
                    {{state.frequency_max_hw.toFixed(2)}}&nbsp;kHz
                  </span>
                </span>
              </td>
            </tr>

            <tr>
              <th>Duty Min [%]</th>
              <th>Duty Max [%]</th>
            </tr>
            <tr>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_duty_min"
                      :value_feedback="state.duty_min"
                      :min="0.0" :max="state.duty_max_hw"
                      :digits="1"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    HW Limit:<br>
                    0.0&nbsp;%
                  </span>
                </span>
              </td>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_duty_max"
                      :value_feedback="state.duty_max"
                      :min="0.0" :max="state.duty_max_hw"
                      :digits="1"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    HW Limit:<br>
                    {{state.duty_max_hw.toFixed(1)}}&nbsp;%
                  </span>
                </span>
              </td>
            </tr>

            <tr>
              <th>Current Limit [A]</th>
              <th></th>
            </tr>
            <tr>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_current_limit"
                      :value_feedback="state.current_limit"
                      :min="0" :max="100"
                      :digits="0"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    HW Limit:<br>
                    100&nbsp;A
                  </span>
                </span>
              </td>
              <td>
                <span class="flex-centered-row">
                </span>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item">
        <table>
          <thead>
            <tr>
              <th colspan="3">Dead Time Settings</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <th>Lead (ZVS) DT [ns]</th>
              <th>Lag (ZCS) DT [ns]</th>
              <th>Hardware Limit [ns]</th>
            </tr>
            <tr>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_lead_dt"
                      :value_feedback="state.lead_dt"
                      :min="6" :max="lead_dt_max_hw"
                      :digits="0"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    Min:<br>6&nbsp;ns
                  </span>
                </span>
              </td>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_lag_dt"
                      :value_feedback="state.lag_dt"
                      :min="6" :max="lag_dt_max_hw"
                      :digits="0"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    Min:<br>6&nbsp;ns
                  </span>
                </span>
              </td>
              <td>
                <span class="flex-stacked-calign">
                  <span>Lead + Lag Sum Max:</span>
                  {{disabled ? "unknown.." : state.dt_sum_max_hw.toFixed(0)}}&nbsp;ns
                </span>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item">
        <table>
          <thead>
            <tr>
              <th colspan="2">Setpoint Throttling (Soft-Start Feature)</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <th>Frequency Rate of Change [kHz/s]</th>
              <th>Duty Rate of Change [%/s]</th>
            </tr>
            <tr>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_frequency_changerate"
                      :value_feedback="state.frequency_changerate"
                      :min="0.01" :max="1000"
                      :digits="2"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    Min:<br>0.01&nbsp;kHz/s
                  </span>
                  <span class="flex-stacked-calign">
                    Max:<br>1000&nbsp;kHz/s
                  </span>
                </span>
              </td>
              <td>
                <span class="flex-centered-row">
                  <LiveNumberInput
                      change_action="set_duty_changerate"
                      :value_feedback="state.duty_changerate"
                      :min="0.1" :max="5000"
                      :digits="1"
                      :disabled="disabled"
                      @action_triggered="dispatch_nv"
                  />
                  <span class="flex-stacked-calign">
                    Min:<br>0.1&nbsp;%/s
                  </span>
                  <span class="flex-stacked-calign">
                    Max:<br>5000&nbsp;%/s
                  </span>
                </span>
              </td>
            </tr>
          </tbody>
        </table>
      </li>

      <li class="grid_item">
        <table>
          <thead>
            <tr>
              <th colspan="3">All Settings - Persistent Storage</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <th>Save all settings &nbsp;&nbsp;(State will be restored on next system start)</th>
            </tr>
            <tr>
              <td>
                <button
                    id="btn_save_settings"
                    name="save_settings"
                    value="true"
                    :disabled="disabled"
                    @click="dispatch_btn"
                >
                  Save!
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
import LiveColorIndicatorBtn from '@/widgets/LiveColorIndicatorBtn.vue'
import LiveToggleSwitch from '@/widgets/LiveToggleSwitch.vue';
import LiveNumberInput from '@/widgets/LiveNumberInput.vue';


export default {
  name: "OperationSettings",
  components: {
    LiveColorIndicatorBtn,
    LiveToggleSwitch,
    LiveNumberInput,
  },
  data() {
    return {};
  },
  props: {
    state: Object,
    disabled: Boolean,
  },
  computed: {
    lag_dt_max_hw() {
      return this.state.dt_sum_max_hw - this.state.lead_dt;
    },
    lead_dt_max_hw() {
      return this.state.dt_sum_max_hw - this.state.lag_dt;
    },
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


<style>
</style>
