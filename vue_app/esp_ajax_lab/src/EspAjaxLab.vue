<template>
  <img alt="Application Logo" style="height: 3rem" src="./assets/logo.svg">

  <EspAjaxLabMain ref="eal"
      :state="state"
      :disabled="disabled"
      :view_updates_inhibited="view_updates_inhibited"
      @submit_cmd="submit_cmd"/>
</template>

<script>
import EspAjaxLabMain from './components/EspAjaxLabMain.vue'

import { AsyncRequestGenerator,
         ServerSentEventHandler,
         AppWatchdog,
        } from './async_requests_sse.js'

let view_state_store = {
  debug: false,
  updates_inhibited: false,
  state: {
      frequency_min_hw: Number(),
      frequency_max_hw: Number(),
      dt_sum_max_hw: Number(),
      // Runtime user setpoint limits for output frequency
      frequency_min: Number(),
      frequency_max: Number(),
      // Operational setpoints for PSPWM module
      frequency: Number(),
      duty: Number(),
      lead_dt: Number(),
      lag_dt: Number(),
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
  update_state (new_state) {
    if (this.debug) {
      console.log('update_state called with', new_state);
    }
    if (!this.updates_inhibited) {
      //for (let key of this.state.keys) {
      //    this.state[key] = new_state[key];
      //}
      Object.assign(this.state, new_state);
    }
  },
};


export default {
  name: 'EspAjaxLab',
  components: {
    EspAjaxLabMain
  },
  data() {
    return {
      disabled: true,
      view_updates_inhibited: view_state_store.updates_inhibited,
      state: view_state_store.state,
      };
  },
  methods: {
    submit_cmd(name, value) {
        this.request_generator.send_cmd(name, value);
    },
    inhibit_view_updates() {
        this.view_state_store.inhibit_view_updates = true;
    },
    allow_view_updates() {
        this.view_state_store.inhibit_view_updates = false;
    },
    set_disabled(new_state) {
        this.disabled = new_state;
    },
  },
  created() {
    this.app_watchdog = new AppWatchdog(1500, this.set_disabled);
    this.request_generator = new AsyncRequestGenerator();
    this.sse_handler = new ServerSentEventHandler("/events",
                                                  this.remote_state_store,
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
