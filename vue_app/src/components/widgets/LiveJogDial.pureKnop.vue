<template>
  <div class="live_jog_dial" id="unique_id_abc">
  </div>
</template>

<script>
//import JogDial from "./JogDial.js";
import pureKnob from "./pureknob.js";

export default {
  name: "LiveJogDial",
  setup() {
    return {
      timeout_timer_id: undefined,
    };
  },
  data() {
    return {
      value_displayed: 0.0,
      editing: false,
    };
  },
  props: {
    change_action: String,
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
    on_input(event) {
      // Set editing state of a number or text input box, prevent view updates
      // from happening
      this.editing = true;
      this.$emit("action_triggered", this.change_action, Number(event.target.value));
      clearTimeout(this.timeout_timer_id);
      this.timeout_timer_id = setTimeout(() => this.editing = false, 1000*this.timeout_s);
    },
    // Submit value, we emit an event with name and value
    on_change(_) {
      setTimeout(() => this.editing = false, 1.1*this.roundtrip_ms);
    },
  },
  emits: ["action_triggered"],
  mounted() {
    const knob = pureKnob.createKnob(300, 300);

    // Set properties.
    knob.setProperty('angleStart', 0 * Math.PI);
    knob.setProperty('angleEnd', 2 * Math.PI);
    knob.setProperty('colorFG', '#88ff88');
    knob.setProperty('trackWidth', 0.4);
    knob.setProperty('valMin', 0);
    knob.setProperty('valMax', 100);

    // Set initial value.
    knob.setValue(50);

    const node = knob.node();
    this.$el.appendChild(node);

/*
    const options = {
        debug: true,
        wheelSize: "90%",
        knobSize: "50%",
        minDegree: null,
        maxDegree: null,
        touchMode: "knob",
    };
    const dial = JogDial(this.$el, options);
    console.log(dial);
*/
  },
};
</script>

<style scoped>
* {
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
}
#unique_id_abc {
    width: 500px;
    height: 500px;
}

#unique_id_abc_knob {
    /*This is your knob style*/
}
</style>