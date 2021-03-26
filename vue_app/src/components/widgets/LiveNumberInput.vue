<!-- LiveNumberInput - Numeric input also acting as a reactive state display
 * 
 * A changed value is emitted in the form of a named "action_triggered" event
 * useful for Vuex state updates, async requests or web socket transmission.
 * 
 * In addition to the input feature, this control acts as a live view of
 * its reactive "value_feedback" property.
 * 
 * Automatic switchover between manual editing mode and visual state
 * representation, including a timeout timer, allows smooth manual control
 * even in the presence of asynchronous/simultaneous updates of the displayed
 * property value.
 *
 * 2021-03-26 Ulrich Lukas
 * License: GPL v.3
-->
<template>
  <div class="live_number_input">
    <input
      type="number"
      :class="{editing: editing}"
      :name="change_action"
      v-model="value_displayed"
      :min="min.toFixed(digits)"
      :max="max.toFixed(digits)"
      :step="10 ** (-digits)"
      :disabled="disabled"
      @input="on_input"
      @blur="on_blur"
      @change="on_change"
    />
  </div>
</template>

<script>
export default {
  name: "LiveNumberInput",
  setup() {
    return {
      timeout_timer_id: undefined,
    };
  },
  data() {
    return {
      editing: false,
      value_displayed: this.value_feedback.toFixed(this.digits),
    };
  },
  props: {
    // Name of the emitted "action_triggered" event
    change_action: String,
    // Displayed reactive property
    value_feedback: Number,
    min: Number,
    max: Number,
    // Decimal digits precision for value rounding
    digits: {default: 0, type: Number},
    // Timeout for automatic termination of manual editing mode at standstill.
    // This should be slightly larger than the maximum expected request
    // round-trip cycle time to have a most up-to-date value display.
    timeout_ms: {default: 750, type: Number},
    // Longer timeout useful when value is being edited by keyboard/cursor
    edit_timeout_ms: {default: 7000, type: Number},
    // Input is blocked and no events are emitted when disabled
    disabled: {default: false, type: Boolean},
  },
  watch: {
    value_feedback: function(val) {
      if (this.editing) {
        return;
      }
      this.value_displayed = val.toFixed(this.digits);
    }
  },
  methods: {
    leave_edit_mode() {
      this.editing = false;
      this.value_displayed = this.value_feedback.toFixed(this.digits);
    },
    on_input(_) {
      // Set editing state of a number or text input box, prevent view updates
      // from happening
      this.editing = true;
      this.start_timeout(this.edit_timeout_ms);
    },
    on_change(e) {
      this.$emit("action_triggered", this.change_action, Number(e.target.value));
      this.start_timeout(this.timeout_ms);
    },
    on_blur(_) {
      this.start_timeout(this.timeout_ms);
    },
    // Also called on blur event of the original input
    start_timeout(t_ms) {
      clearTimeout(this.timeout_timer_id);
      this.timeout_timer_id = setTimeout(() => this.leave_edit_mode(), t_ms);
    },
  },
  emits: ["action_triggered"]
};
</script>

<style scoped>
input {
    width: 6rem;
}
input.editing {
  background-color: #ffa8a8;
}
</style>