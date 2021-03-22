<template>
  <div class="live_jog_dial">
      <input class="p5" type="range" min="0" max="100"
       data-width="100" 
       data-height="100" 
       data-angleOffset="220"
       data-angleRange="280"
      >

  </div>
</template>

<script>
//import JogDial from "./JogDial.js";
//import pureKnob from "./pureknob.js";
import * as JimKnopf from "./knob.js";

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
    let Ui = JimKnopf.Ui;

    Ui.P5=function(){};
    Ui.P5.prototype=Object.create(Ui.prototype);
    Ui.P5.prototype.createElement=function(){
        Ui.prototype.createElement.apply(this,arguments);
        this.addComponent(new Ui.Pointer({
            type:'Arc',size:30,
            outerRadius:this.width/2.2,
            innerRadius:this.width/2.2-this.width/6,
            angleoffset:this.options.angleoffset
        }));
        this.addComponent(new Ui.Text());
        var circle=new Ui.El.Circle(this.width/2.1,this.width/2,this.height/2);
        this.el.node.appendChild(circle.node);
        this.el.node.setAttribute("class","p5");};

    new JimKnopf.Knob(this.$el.firstElementChild, new Ui.P5());
  },
};
</script>

<style scoped>
/** {
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
}*/
#unique_id_abc {
    width: 200px;
    height: 200px;
}

#unique_id_abc_knob {
    /*This is your knob style*/
}

::v-deep svg circle{stroke:#57c7b6;fill: none;stroke: black;stroke-width:2;}
::v-deep svg text{font-size:40px;fill:#57c7b6;font-weight:300}
::v-deep svg path{fill:#57c7b6}


</style>