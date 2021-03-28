<!-- LiveColorIndicatorBtn - Colored background indicator button
 * 
 * On click, the "action_triggered" event is emitted with
 * name property and the boolean value "true".
 * 
 * In addition to the input feature, this control acts as a live view of its
 * reactive "value_feedback" property by switching of text and background color.
 *
 * 2021-03-26 Ulrich Lukas
 * License: GPL v.3
-->
<template>
    <div
        :style="{backgroundColor}"
        @click="on_click"
    >
        <span style="white-space: pre">
            {{ disabled ? disabled_text :
                value_feedback ? active_text :
                    inactive_text }}
        </span>
    </div>
</template>

<script>
export default {
  name: "LiveColorIndicatorBtn",
  props: {
    // Name of the emitted "action_triggered" event
    change_action: String,
    // Displayed reactive property
    value_feedback: {default: false, type: Boolean},
    // Text and background color representing a boolean "true" feedback value
    active_text: {default: "Active", type: String},
    active_color: {default: "rgb(250, 84, 84)", type: String}, // light red
    inactive_text: {default: "Inactive", type: String},
    inactive_color: {default: "rgb(139, 255, 143)", type: String}, // light green
    // Input is blocked and no events are emitted when disabled
    disabled: {default: false, type: Boolean},
    disabled_text: {default: "Unknown..", type: String},
    disabled_color: {default: "rgb(211, 211, 211)", type: String}, // grey
  },
  computed: {
      backgroundColor() {
          return this.disabled ? this.disabled_color :
              this.value_feedback ? this.active_color :
                  this.inactive_color;
      },
  },
  methods: {
    on_click(_) {
      if (this.disabled) {
        return;
      }
      this.$emit("action_triggered", this.change_action, "true");
    },
  },
  emits: ["action_triggered"]
};
</script>

<style scoped>
div {
    height: 100%;
    display: flex;
    flex-flow: column nowrap;
    align-items: center;
    justify-content: center;
}
</style>