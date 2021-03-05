<template>
  <div class="live_range_input">
    <input
      type="range"
      :class="{editing: editing}"
      :name="name_prop"
      :value="value_displayed"
      :min="min.toFixed(digits)"
      :max="max.toFixed(digits)"
      :step="10 ** (-digits)"
      @input="on_input"
      @change="on_change"
      :disabled="disabled"
    />
  </div>
</template>

<script>
export default {
  name: "LiveRangeInput",
  data() {
    return {
      value_edit: 0.0,
      editing: false,
    };
  },
  props: {
    name_prop: String,
    value_feedback: Number,
    min: Number,
    max: Number,
    digits: {default: 0, type: Number},
    timeout_s: {default: 7, type: Number},
    // Specify request round-trip or periodic update time to prevent flicker on input
    roundtrip_ms: {default: 750, type: Number},
    disabled: {default: false, type: Boolean}
  },
  computed: {
    value_displayed() {
      if (this.editing || this.$attrs.editing) {
          return this.value_edit.toFixed(this.digits);
      } else {
          return this.value_feedback.toFixed(this.digits);
      }
    },
  },
  methods: {
    on_input(event) {
      this.value_edit = Number(event.target.value);
      // Set editing state of a number or text input box, prevent view updates
      // from happening
      this.editing = true;
      this.$emit("value_changed", this.name_prop, Number(event.target.value));
      setTimeout(() => this.editing = false, 1000*this.timeout_s);
    },
    // Submit value, we emit an event with name and value
    on_change(event) {
      setTimeout(() => this.editing = false, 1.1*this.roundtrip_ms);
    },
  },
  emits: ["value_changed"]
};
</script>

<style scoped>
</style>