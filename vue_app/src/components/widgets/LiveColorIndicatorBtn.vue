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
    change_action: String,
    value_feedback: {default: false, type: Boolean},
    active_text: {default: "Active", type: String},
    active_color: {default: "rgb(250, 84, 84)", type: String}, // light red
    inactive_text: {default: "Inactive", type: String},
    inactive_color: {default: "rgb(139, 255, 143)", type: String}, // light green
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
    // Submit command with "true" as a string
    on_click() {
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