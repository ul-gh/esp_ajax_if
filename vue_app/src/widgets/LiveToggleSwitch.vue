<!-- LiveToggleSwitch - Boolean input similar to HTML "checkbox" input,
 *                      using a toggle-switch style (see toggle-switchy.css)
 * 
 * A changed value is emitted in the form of a named "action_triggered" event
 * useful for Vuex state updates, async requests or web socket transmission.
 * 
 * In addition to the input feature, this control acts as a live view of
 * its reactive "value_feedback" property value.
 *
 * 2021-03-26 Ulrich Lukas
 * License: GPL v.3
-->
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
    // Name of the emitted "action_triggered" event
    change_action: String,
    // Displayed reactive property
    value_feedback: {default: false, type: Boolean},
    // Input is blocked and no events are emitted when disabled
    disabled: {default: false, type: Boolean}
  },
  methods: {
    // Submit current state, we emit an event with name and value
    on_change(e) {
      this.$emit("action_triggered", this.change_action, e.target.checked);
    },
  },
  emits: ["action_triggered"]
};
</script>

<style scoped src="./toggle-switchy.css">
</style>