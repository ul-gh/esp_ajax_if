<!-- ESP Ajax Lab - Web Application for Remote Hardware Control
 *
 * See home page: https://github.com/ul-gh/esp_ajax_if
 *
 * This single-page web application is built using vue.js v.3.
 * 
 * Remote hardware is interfaced using ES7 fetch API and async co-routines
 * performing HTTP requests. Server-Sent Events (SSE) are used to update the
 * application view with state updates sent back from the server.
 * => See imported modules from async_requests_sse.js
 *
 * This main app component is a container for a tabbed view of sub-components.
 * This component also sets up the hardware interfacing and app state model
 * using the vue.js live-cycle hooks below.
 * 
 * 2021-03-12 Ulrich Lukas
 * License: GPL v.3
-->
<template>
  <!-- Grid for header, tab bar and tab area -->
  <ul class="grid_container">
    <li class="grid_item">
      <img alt="Application Logo" src="./assets/logo.svg">
    </li>
    <span class="tab-dimensions">
      <li class="grid_item">
        <span class="tab-bar">
          <button
            v-for="(title, key) in tabs"
            :key="key"
            :class="['tab-button', { active: current_tab === key }]"
            @click="current_tab = key"
          >
            {{ title }}
          </button>
          <!--
          <button class="tab-button tab-spacer">
            &nbsp;
          </button>
          -->
        </span>
      </li>
      <li class="grid_item">
        <component
          :is="current_tab"
          :ref="current_tab"
          :state="store.state"
          :disabled="store.disabled"
          @action="store.dispatch_action"
        >
        </component>
      </li>
    </span>
  </ul>
</template>

<script>
import { reactive } from "vue";

import LiveController from "./components/LiveController.vue";
import OperationSettings from "./components/OperationSettings.vue";
import HelpDocumentation from "./components/HelpDocumentation.vue";
import NetworkAndUpdate from "./components/NetworkAndUpdate.vue";

import {
  AsyncRequestGenerator,
  ServerSentEventHandler,
  AppWatchdog
} from "./async_requests_sse.js";

const request_generator = new AsyncRequestGenerator("/cmd");

let view_state_store = reactive({
  debug: false,
  disabled: true, // Disable all controls by default, is enabled on watchdog reset
  state: {
    // Hardware limits
    frequency_min_hw: 0.01,
    frequency_max_hw: 1000,
    dt_sum_max_hw: 600,
    duty_max_hw: 100.0,
    current_limit_max_hw: 100,
    // Runtime user setpoint limits for output frequency and duty cycle
    frequency_min: 90.00,
    frequency_max: 110.00,
    duty_min: 0.0,
    duty_max: 100.0,
    // Operational setpoints for PSPWM module
    frequency: 100.00,
    frequency_changerate: 25.00,
    duty: 0.0,
    duty_changerate: 250.0,
    lead_dt: 300,
    lag_dt: 300,
    power_pwm_active: true,
    // Settings for auxiliary HW control module
    current_limit: 0,
    relay_ref_active: false,
    relay_dut_active: false,
    // Temperatures and fan
    temp_1: 0.0,
    temp_2: 0.0,
    // Overtemperature protection limits
    temp_1_limit: 50.0,
    temp_2_limit: 50.0,
    fan_active: false,
    fan_override: false,
    // Clock divider settings
    base_div: 1.0,
    timer_div: 1.0,
    // Gate driver supply and disable signals
    drv_supply_active: true,
    drv_disabled: true,
    // Hardware Fault Shutdown Status is latched using these flags
    hw_error: "",
    hw_oc_fault: false,
    hw_overtemp: false,
    // Length of the power output one-shot timer pulse
    oneshot_len: 0.001,
  },
  // Called by server-sent event handler
  update_state(new_state) {
    if (this.debug) {
      console.log("Updating app state with received object: ", new_state);
    }
    for (let key in this.state) {
      if (new_state.hasOwnProperty(key)) {
        this.state[key] = new_state[key];
      }
    }
    // Probably unsafe as it adds arbitrary new attributes from unsafe JSON
    //Object.assign(this.state, new_state);
    // Some of the application state is re-computed when state update is received
    this.compute_mutations();
  },
  dispatch_action(name, value) {
    if (this.debug) {
      console.log(`Dispatching action "${name}" with value: ${value}`);
    }
    request_generator.send_cmd(name, value);
  },
  compute_mutations() {
    const state = this.state;
    state.hw_error = state.hw_oc_fault ? "HW OC FAULT" : state.hw_overtemp ? "OVERTEMPERATURE" : "";
    const dt_effective = Math.max(state.lead_dt, state.lag_dt);
    state.duty_max_hw = 100 * (1 - 1E-6 * state.frequency * 2 * dt_effective);
  },
});


export default {
  name: "EspAjaxLab",
  components: {
    LiveController,
    OperationSettings,
    HelpDocumentation,
    NetworkAndUpdate,
  },
  data() {
    return {
      tabs: {
        "LiveController": "Live HW Control",
        "OperationSettings": "Operation Settings",
        "HelpDocumentation": "Help / Documentation",
        "NetworkAndUpdate": "Network and Update",
      },
      current_tab: "LiveController",
      store: view_state_store,
      disabled: view_state_store.disabled,
    };
  },
  methods: {
    set_debug() {
      this.store.debug = true;
      this.sse_handler.disable_reconnect_and_watchdog();
    }
  },
  created() {
    this.app_watchdog = new AppWatchdog(1500, val => view_state_store.disabled = val);
    this.sse_handler = new ServerSentEventHandler("/events", view_state_store, this.app_watchdog);
    // Initial computed state update
    this.store.compute_mutations();
  },
};
</script>

<style src="./main_app.css"></style>

<style scoped>
img {
  margin: auto;
  margin-bottom: 1rem;
  display: block;
  width: 28rem;
  height: 3rem;
}
ul.grid_container {
  display: grid;
  list-style: none;
  padding: 0;
  grid-auto-rows: minmax(auto, auto);
  gap: 0;
  justify-content: center;
  justify-items: stretch;
}
.grid_item {
  max-width: 100vw;
}

.tab-dimensions{
  background: #e0e0e0;
  /* background-color: #e4f0f5; */
  border: 3px solid #f0f0f0;
}
.tab-bar {
  width: 100%;
  height: 100%;
  margin-right: auto;
  display: flex;
  flex-flow: row nowrap;
  align-items: center;
  justify-content: stretch;
}
.tab-button {
  padding: 6px 10px;
  border-top-left-radius: 3px;
  border-top-right-radius: 3px;
  border: 1px solid #ccc;
  cursor: pointer;
  background: #f0f0f0;
  margin-bottom: -1px;
  margin-right: -1px;
}
.tab-button:hover {
  background: #e0e0e0;
}
.tab-button:focus {
  outline: none;
}
.tab-button.active {
  background: #e0e0e0;
}
.tab-spacer {
  cursor: default;
  flex-grow: 1;
}
.tab-spacer:hover {
  background: #f0f0f0;
}
</style>
