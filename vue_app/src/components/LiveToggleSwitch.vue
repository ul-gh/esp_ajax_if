<template>
    <label class="live_toggle_switch toggle-switchy">
        <input
          type="checkbox"
          :name="name_prop"
          :checked="value_feedback"
          @change="on_change"
          :disabled="disabled"/>
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
    name_prop: String,
    value_feedback: {default: false, type: Boolean},
    disabled: {default: false, type: Boolean}
  },
  methods: {
    // Submit current state, we emit an event with name and value
    on_change(event) {
      this.$emit("value_changed", this.name_prop, event.target.checked);
    },
  },
  emits: ["value_changed"]
};
</script>

<style scoped src="./toggle-switchy.css">
</style>