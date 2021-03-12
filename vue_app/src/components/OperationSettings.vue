<!-- ESP Ajax Lab Operation Specific Settings Component
-->
<template>
  <div class="operation_settings">
    <!--Responsive grid layout for main content-->
    <ul class="grid_container main_grid">
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
      <!--End responsive grid layout-->
    </ul>
  </div>
</template>

<script>
import LiveColorIndicatorBtn from './widgets/LiveColorIndicatorBtn.vue'
import LiveToggleSwitch from './widgets/LiveToggleSwitch.vue';
import LiveNumberInput from './widgets/LiveNumberInput.vue';


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
