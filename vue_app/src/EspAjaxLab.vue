<template>
  <img alt="Application Logo" style="height: 2rem" src="./assets/logo.svg">

  <EspAjaxLabMain ref="eal"
      :state="state"
      :disabled="disabled"
      @submit_cmd="submit_cmd"/>
</template>

<script>
import { reactive } from 'vue';
import EspAjaxLabMain from './components/EspAjaxLabMain.vue';
import { AsyncRequestGenerator,
         ServerSentEventHandler,
         AppWatchdog,
        } from './async_requests_sse.js';

let view_state_store = reactive({
  debug: false,
  state: {
      frequency_min_hw: Number(),
      frequency_max_hw: Number(),
      dt_sum_max_hw: Number(),
      // Runtime user setpoint limits for output frequency
      frequency_min: 90.00,
      frequency_max: 110.00,
      // Operational setpoints for PSPWM module
      frequency: 100.00,
      duty: 0.0,
      lead_dt: 300,
      lag_dt: 300,
      power_pwm_active: Boolean(),
      // Settings for auxiliary HW control module
      current_limit: Number(),
      relay_ref_active: Boolean(),
      relay_dut_active: Boolean(),
      // Temperatures and fan
      heatsink_temp: Number(),
      aux_temp: Number(),
      fan_active: Boolean(),
      fan_override: Boolean(),
      // Clock divider settings
      base_div: Number(),
      timer_div: Number(),
      // Gate driver supply and disable signals
      drv_supply_active: Boolean(),
      drv_disabled: Boolean(),
      // True when hardware OC shutdown condition is present
      hw_oc_fault_present: Boolean(),
      // Hardware Fault Shutdown Status is latched using this flag
      hw_oc_fault_occurred: Boolean(),
      // Length of the power output one-shot timer pulse
      oneshot_len: Number(),
    },
  // Called by server-sent event handler
  update_state (new_state) {
    if (this.debug) {
      console.log('update_state called with', new_state);
    }
      for (let key in this.state) {
          this.state[key] = new_state[key];
      }
      // Probably unsafe as it adds arbitrary new attributes from unsafe JSON
      //Object.assign(this.state, new_state);
  },
});


export default {
  name: 'EspAjaxLab',
  components: {
    EspAjaxLabMain,
  },
  data() {
    return {
      disabled: true,
      store: view_state_store,
      state: view_state_store.state,
      };
  },
  methods: {
    submit_cmd(name, value) {
        this.request_generator.send_cmd(name, value);
    },
    set_disabled(bv) {
        this.disabled = bv;
    },
    set_debug() {
        this.store.debug = true;
        this.sse_handler.disable_reconnect_and_watchdog();
    },
  },
  created() {
    this.app_watchdog = new AppWatchdog(1500, (s) => this.set_disabled(s));
    this.request_generator = new AsyncRequestGenerator("/cmd");
    this.sse_handler = new ServerSentEventHandler("/events",
                                                  view_state_store,
                                                  this.app_watchdog);
  },
}
</script>

<style>
#app {
  font-family: Avenir, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  text-align: center;
  color: #2c3e50;
}
</style>
