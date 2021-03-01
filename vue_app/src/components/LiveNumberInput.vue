<template>
  <div class="live_number_input" :class="{editing: editing}">
    <input
      type="number"
      :name="content_name"
      :value="value_displayed"
      :min="value_min"
      :max="value_max"
      :step="10 ** (-digits)"
      @input="on_input"
      @blur="on_blur"
      @change="on_change"
      :disabled="disabled"
    />
  </div>
</template>

<script>
export default {
  name: "live_number_input",
  data() {
    return {
      value_edit: 0.0,
      editing: false,
    };
  },
  props: {
    content_name: String,
    value_feedback: Number,
    value_min: Number,
    value_max: Number,
    digits: Number,
    timeout: Number,
    disabled: Boolean
  },
  computed: {
    value_displayed() {
      if (this.editing || this.$attrs.editing) {
          return this.value_edit;
      } else {
          return this.value_feedback;
      }
    },
  },
  watch: {},
  methods: {
    on_input(event) {
      this.value_edit = Number(event.target.value);
      // Set editing state of a number or text input box, prevent view updates
      // from happening
      this.editing = true;
      setTimeout(() => this.editing = false, this.timeout);
    },
    // Submit number input, we emit an event with name and value
    on_change(event) {
      this.$emit("value_changed", content_name, Number(event.target.value));
    },
    on_blur() {
        this.editing = false;
    },
  },
  emits: ["value_changed"]
};
</script>

<!-- Add "scoped" attribute to limit CSS to this component only -->
<style scoped>
.live_number_input {
  background-color: #3f87a6;
}
.live_number_input.editing {
  background-color: #ffa8a8;
}
</style>