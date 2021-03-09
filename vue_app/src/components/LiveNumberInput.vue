<template>
  <div class="live_number_input">
    <input
      type="number"
      :class="{editing: editing}"
      :name="cmd_name"
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
      value_displayed: 0.0,
    };
  },
  props: {
    cmd_name: String,
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
    on_input(_) {
      // Set editing state of a number or text input box, prevent view updates
      // from happening
      this.editing = true;
      clearTimeout(timeout_timer_id);
      timeout_timer_id = setTimeout(() => this.editing = false, 1000*this.timeout_s);
    },
    // Submit value, we emit an event with name and value
    on_change(event) {
      this.$emit("value_changed", this.cmd_name, Number(event.target.value));
      this.on_blur();
    },
    on_blur() {
      setTimeout(() => this.editing = false, 1.1*this.roundtrip_ms);
    },
  },
  emits: ["value_changed"]
};
</script>

<style scoped>
input.editing {
  background-color: #ffa8a8;
}
</style>