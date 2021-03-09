<template>
    <label class="live_toggle_switch toggle-switchy">
        <input
          type="checkbox"
          :name="cmd_name"
          :checked="value_feedback"
          :disabled="disabled"
          @change="on_change"
        />
        <span class="toggle">
          <span class="switch">
          </span>
        </span>
    </label>
</template>

<script>
export default {
  name: "LiveToggleSwitch",
  data() {
      return {};
  },
  props: {
    cmd_name: String,
    value_feedback: {default: false, type: Boolean},
    disabled: {default: false, type: Boolean}
  },
  methods: {
    // Submit current state, we emit an event with name and value
    on_change(event) {
      this.$emit("value_changed", this.cmd_name, event.target.checked);
    },
  },
  emits: ["value_changed"]
};
</script>

<style scoped src="./toggle-switchy.css">
</style>