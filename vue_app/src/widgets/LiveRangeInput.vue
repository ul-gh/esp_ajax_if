<!-- LiveRangeInput - Numeric input which is a styled HTML "range" input
 * 
 * A changed value is emitted in the form of a named "action_triggered" event
 * useful for Vuex state updates, async requests or web socket transmission.
 * 
 * In addition to the input feature, this control acts as a live view of
 * its reactive "value_feedback" property value by the current slider position.
 * 
 * Automatic switchover between manual editing mode and visual state
 * representation allows smooth manual control even in the presence
 * of simultaneously incoming updates of the "value_feedback" property.
 *
 * 2021-03-26 Ulrich Lukas
 * License: GPL v.3
-->
<template>
  <div class="live_range_input">
    <input
      type="range"
      :class="{editing: editing}"
      :name="change_action"
      v-model="value_displayed"
      :min="min.toFixed(digits)"
      :max="max.toFixed(digits)"
      :step="10 ** (-digits)"
      :disabled="disabled"
      @input="on_input"
      @change="on_change"
    />
  </div>
</template>

<script>
export default {
  name: "LiveRangeInput",
  setup() {
    return {};
  },
  data() {
    return {
      value_displayed: this.value_feedback.toFixed(this.digits),
      editing: false,
    };
  },
  props: {
    // Name of the emitted "action_triggered" event
    change_action: String,
    // Displayed reactive property
    value_feedback: Number,
    min: Number,
    max: Number,
    // Decimal digits precision for output value rounding and slider step size
    digits: {default: 0, type: Number},
    // Input is blocked and no events are emitted when disabled
    disabled: {default: false, type: Boolean},
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
    leave_edit_mode() {
      this.editing = false;
      this.value_displayed = this.value_feedback.toFixed(this.digits);
    },
    on_input(e) {
      // Set editing state of input, prevent view updates from happening
      this.editing = true;
      this.$emit("action_triggered", this.change_action, Number(e.target.value));
    },
    on_change(_) {
      this.leave_edit_mode();
    },
    start_timeout(t_ms) {
      clearTimeout(this.timeout_timer_id);
      this.timeout_timer_id = setTimeout(() => this.leave_edit_mode(), t_ms);
    },
  },
  emits: ["action_triggered"]
};
</script>

<style scoped>
/* Range input styling, see: http://danielstern.ca/range.css/#/ */
input[type=range] {
    width: 100%;
    margin: 13.8px 0;
    background-color: transparent;
    -webkit-appearance: none;
}
input[type=range]:focus {
    outline: none;
}
input[type=range]::-webkit-slider-runnable-track {
    background: #3f87a6;
    border: 0.2px solid #010101;
    border-radius: 1.3px;
    width: 100%;
    height: 8.4px;
    cursor: pointer;
}
input[type=range]::-webkit-slider-thumb {
    margin-top: -14px;
    width: 36px;
    height: 36px;
    background: #82c5ff;
    border: 1px solid #000000;
    border-radius: 6px;
    cursor: pointer;
    -webkit-appearance: none;
}
input[type=range]:focus::-webkit-slider-runnable-track {
    background: #4593b5;
}
input[type=range]::-moz-range-track {
    background: #3f87a6;
    border: 0.2px solid #010101;
    border-radius: 1.3px;
    width: 100%;
    height: 8.4px;
    cursor: pointer;
}
input[type=range]::-moz-range-thumb {
    width: 36px;
    height: 36px;
    background: #82c5ff;
    border: 1px solid #000000;
    border-radius: 6px;
    cursor: pointer;
}
input[type=range]::-ms-track {
    background: transparent;
    border-color: transparent;
    border-width: 14.8px 0;
    color: transparent;
    width: 100%;
    height: 8.4px;
    cursor: pointer;
}
input[type=range]::-ms-fill-lower {
    background: #397b97;
    border: 0.2px solid #010101;
    border-radius: 2.6px;
}
input[type=range]::-ms-fill-upper {
    background: #3f87a6;
    border: 0.2px solid #010101;
    border-radius: 2.6px;
}
input[type=range]::-ms-thumb {
    width: 36px;
    height: 36px;
    background: #82c5ff;
    border: 1px solid #000000;
    border-radius: 6px;
    cursor: pointer;
    margin-top: 0px;
    /*Needed to keep the Edge thumb centred*/
}
input[type=range]:focus::-ms-fill-lower {
    background: #3f87a6;
}
input[type=range]:focus::-ms-fill-upper {
    background: #4593b5;
}
/*TODO: Use one of the selectors from https://stackoverflow.com/a/20541859/7077589 and figure out
how to remove the virtical space around the range input in IE*/
@supports (-ms-ime-align:auto) {
    /* Pre-Chromium Edge only styles, selector taken from hhttps://stackoverflow.com/a/32202953/7077589 */
    input[type=range] {
        margin: 0;
        /*Edge starts the margin from the thumb, not the track as other browsers do*/
    }
}
</style>