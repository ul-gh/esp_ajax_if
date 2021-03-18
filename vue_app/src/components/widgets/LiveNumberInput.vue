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
let timeout_timer_id = undefined;

export default {
  name: "LiveNumberInput",
  data() {
    return {
      editing: false,
      value_displayed: this.value_feedback.toFixed(this.digits),
    };
  },
  props: {
    change_action: String,
    value_feedback: Number,
    min: Number,
    max: Number,
    digits: {default: 0, type: Number},
    timeout_s: {default: 7, type: Number},
    // Specify request round-trip or periodic update time to prevent flicker on input
    roundtrip_ms: {default: 750, type: Number},
    disabled: {default: false, type: Boolean}
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
      clearTimeout(timeout_timer_id);
      timeout_timer_id = setTimeout(() => this.leave_edit_mode(), 1000*this.timeout_s);
    },
    // Submit value, we emit an event with name and value
    on_change(event) {
      this.$emit("action_triggered", this.change_action, Number(event.target.value));
      this.on_blur();
    },
    on_blur() {
      setTimeout(() => this.leave_edit_mode(), 1.1*this.roundtrip_ms);
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