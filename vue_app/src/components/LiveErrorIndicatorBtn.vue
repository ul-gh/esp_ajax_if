<template>
    <div
        :fault_present="value_feedback ? '' : undefined"
        :disabled="disabled ? '' : undefined"
        @click="on_click"
    >
        <span style="white-space: pre">
            {{ disabled ? "Unknown.." : value_feedback ? error_text : ok_text }}
        </span>
    </div>
</template>

<script>
export default {
  name: "LiveErrorIndicatorBtn",
  data() {
      return {};
  },
  props: {
    change_action: String,
    value_feedback: {default: false, type: Boolean},
    ok_text: String,
    error_text: String,
    disabled: {default: false, type: Boolean}
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
    background-color: rgb(139, 255, 143);
}
div[fault_present]{
    background-color: rgb(250, 84, 84);
}
div[disabled] {
    background-color: rgb(211, 211, 211);
}
</style>