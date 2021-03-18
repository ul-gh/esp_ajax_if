<template>
  <div class="live_jog_dial">
    <div class="dial" @mousemove="on_dial_mousemove">
    </div>
    <div class="bar_graph">
        <div class="bar" :style="{width: bar_width_fmt}"></div>
    </div>
  </div>
</template>

<script>
import JogDial from "../../js_modules/JogDial.js/jogDial.js";

let timeout_timer_id = undefined;

export default {
  name: "LiveJogDial",
  data() {
    return {
      value_displayed: 0.0,
      editing: false,
      bar_width_fmt: "0%", // Bar graph bar width as a string
      events_inhibited: false,
    };
  },
  props: {
    change_action: String,
    value_feedback: Number,
    min: Number,
    max: Number,
    digits: {default: 0, type: Number},
    n_turns: {default: 5, type: Number},
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
      this.set_value(val);
    },
    n_turns: function(val) {
        this.jog_dial.opt.maxDegree = val * 360;
    },
  },
  methods: {
    set_value(val) {
      val = val < this.min ? this.min : val > this.max ? this.max : val;
      this.value_displayed = val.toFixed(this.digits);
      let scale_factor = (Number(this.value_displayed) - this.min) / (this.max - this.min);
      this.bar_width_fmt = `${(100 * scale_factor).toFixed(0)}%`;
      this.events_inhibited = true;
      this.jog_dial.angle(scale_factor * this.n_turns * 360);
      this.events_inhibited = false;
    },
    leave_edit_mode() {
        this.editing = false;
        this.set_value(this.value_feedback);
    },
    on_dial_mousemove(e) {
      if (this.events_inhibited || e.target.rotation == undefined) {
          return;
      }
      // Set editing state of input, prevent view updates from happening
      this.editing = true;
      let scale_factor = e.target.rotation / (360 * this.n_turns);
      this.bar_width_fmt = `${(100 * scale_factor).toFixed(0)}%`;
      this.value_displayed = (
          this.min + scale_factor * (this.max - this.min)
          ).toFixed(this.digits);
      this.$emit("action_triggered", this.change_action, Number(this.value_displayed));
      clearTimeout(timeout_timer_id);
      timeout_timer_id = setTimeout(() => this.leave_edit_mode(), 1.1*this.roundtrip_ms);
    },
  },
  emits: ["action_triggered"],
  mounted() {
    const init_options = {
        debug: false,
        wheelSize: "200px",
        knobSize: "70px",
        minDegree: 0,
        maxDegree: this.n_turns * 360,
        touchMode: "knob",
    };
    const dial_el = document.getElementsByClassName("dial")[0];
    this.jog_dial = JogDial(dial_el, init_options);
    this.set_value(this.value_feedback);
  },
};
</script>

<style scoped>
:deep(.live_jog_dial) {
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
}

:deep(.dial) {
  overflow: hidden;
  position: relative;
  width: 260px;
  height: 260px;
  margin: 20px auto;
  background: url('./wheel_sa.png');
  background-repeat: none;
}
:deep(.dial_knob) {
  background: url('./knob.png');
}

:deep(.bar_graph) {
  width: 200px;
  height: 10px;
  margin: 20px auto 30px;
  background: #999;
  overflow: hidden;
  -webkit-border-radius: 5px;
  -moz-border-radius: 5px;
  -ms-border-radius: 5px;
  -o-border-radius: 5px;
  border-radius: 5px;
}
:deep(.bar_graph .bar) {
  position: relative;
  width: 0;
  height: 100%;
  background: #80e93a;
}
</style>