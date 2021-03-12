<template>
    <label class="live_toggle_switch toggle-switchy">
        <input
          type="checkbox"
          :name="change_action"
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
    change_action: String,
    value_feedback: {default: false, type: Boolean},
    disabled: {default: false, type: Boolean}
  },
  methods: {
    // Submit current state, we emit an event with name and value
    on_change(event) {
      this.$emit("action_triggered", this.change_action, event.target.checked);
    },
  },
  emits: ["action_triggered"]
};
</script>

<style scoped src="./toggle-switchy.css">
</style>