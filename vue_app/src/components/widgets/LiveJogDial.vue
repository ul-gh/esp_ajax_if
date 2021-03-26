<!-- LiveJogDial - Numeric input similar to HTML "range" input,
 *                 using a Multi-Turn "JogDial" style knob.
 * 
 * A changed value is emitted in the form of a named "action_triggered" event
 * useful for Vuex state updates, async requests or web socket transmission.
 * 
 * In addition to the input feature, this control acts as a live view of
 * its reactive "value_feedback" property value using a bar graph; the
 * knob rotation angle is always kept in sync.
 * 
 * Automatic switchover between manual editing mode and visual state
 * representation, including a timeout timer, allows smooth manual control
 * even in the presence of asynchronous/simultaneous updates of the displayed
 * property value.
 *
 * 2021-03-26 Ulrich Lukas
 * License: GPL v.3
-->
<template>
  <div class="live_jog_dial">
    <div class="jog_dial" @mousemove="on_dial_mousemove">
    </div>
    <div class="bar_graph">
        <div class="bar" :style="{width: bar_width_fmt}"></div>
    </div>
  </div>
</template>

<script>
// https://github.com/ul-gh/JogDial.js: Fork of https://github.com/ohsiwon/JogDial.js
// setting HTML element class instead of id attribute for multiple instances
import JogDial from "../../js_modules/JogDial.js/jogDial.js";

export default {
  name: "LiveJogDial",
  setup() {
    return {
      timeout_timer_id: undefined,
      jog_dial: undefined,
    };
  },
  data() {
    return {
      value: 0.0,
      editing: false,
      bar_width_fmt: "0%", // CSS attribute value, set by set_value()
      events_inhibited: true, // Prevent events when setting value programmatically
    };
  },
  props: {
    // Name of the emitted "action_triggered" event
    change_action: String,
    // Displayed reactive property
    value_feedback: Number,
    min: Number,
    max: Number,
    // Decimal digits precision for output value rounding and bar graph step size
    digits: {default: undefined, type: Number},
    // Number of turns of the dial control
    n_turns: {default: 5, type: Number},
    // Timeout for automatic termination of manual editing mode at standstill.
    // This should be slightly larger than the maximum expected request
    // round-trip cycle time to have a most up-to-date value display.
    timeout_ms: {default: 750, type: Number},
    // Input is blocked and no events are emitted when disabled
    disabled: {default: false, type: Boolean}
  },
  watch: {
    value_feedback: function(val) {
      this.set_value(val);
    },
    min: function() {
      this.set_value(this.value);
    },
    max: function() {
      this.set_value(this.value);
    },
    n_turns: function(val) {
      if (!this.jog_dial) {
        return;
      }
      this.jog_dial.opt.maxDegree = val * 360;
      this.set_value(this.value);
    },
  },
  methods: {
    set_value(val) {
      if (!this.jog_dial) {
        return;
      }
      val = val < this.min ? this.min : val > this.max ? this.max : val;
      this.value = this.digits === undefined ? val : Number(val.toFixed(this.digits));
      const scale_factor = (this.value - this.min) / (this.max - this.min);
      this.bar_width_fmt = `${(100 * scale_factor).toFixed(0)}%`;
      // When the wheel is not currently being turned manually, also set its angle value
      if (!this.editing) {
        // When calling the angle setter method, the library still emits the
        // "mousemove" event, so we have to prevent an infinite callback loop here..
        this.events_inhibited = true;
        this.jog_dial.angle(scale_factor * this.n_turns * 360);
        this.events_inhibited = false;
      }

    },
    leave_edit_mode() {
        this.editing = false;
        this.set_value(this.value);
    },
    on_dial_mousemove(e) {
      if (e.target.rotation === undefined || this.events_inhibited) {
          return;
      }
      // Underlying control has no disable function, turning wheel back to fixed value
      if (this.disabled) {
          this.set_value(this.value);
          return;
      }
      // Prevent view updates from happening while control is being operated
      this.editing = true;
      const scale_factor = e.target.rotation / (360 * this.n_turns);
      //this.bar_width_fmt = `${(100 * scale_factor).toFixed(0)}%`;
      let val = this.min + scale_factor * (this.max - this.min);
      val = this.digits === undefined ? val : Number(val.toFixed(this.digits));
      this.$emit("action_triggered", this.change_action, val);
      clearTimeout(this.timeout_timer_id_id);
      // Vue auto-binds "this" instance to methods, no need for arrow function etc..
      this.timeout_timer_id_id = setTimeout(this.leave_edit_mode, this.timeout_ms);
    },
  },
  emits: ["action_triggered"],
  mounted() {
    console.log(this.foo);
    const init_options = {
        debug: false,
        wheelSize: "200px",
        knobSize: "70px",
        minDegree: 0,
        maxDegree: this.n_turns * 360,
        touchMode: "knob",
    };
    this.jog_dial = JogDial(this.$el.firstElementChild, init_options);
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

:deep(.jog_dial) {
  overflow: hidden;
  position: relative;
  width: 260px;
  height: 260px;
  margin: 20px auto;
  background: url('./wheel_sa.png');
  background-repeat: none;
}
:deep(.jog_dial_knob) {
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