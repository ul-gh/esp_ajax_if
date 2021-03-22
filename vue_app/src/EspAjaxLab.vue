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
          :state="state"
          :disabled="disabled"
          @submit_cmd="submit_cmd"
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

let view_state_store = reactive({
  debug: false,
  state: {
    // Hardware limits
    frequency_min_hw: Number(),
    frequency_max_hw: Number(),
    dt_sum_max_hw: Number(),
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
    power_pwm_active: Boolean(),
    // Settings for auxiliary HW control module
    current_limit: Number(),
    relay_ref_active: Boolean(),
    relay_dut_active: Boolean(),
    // Temperatures and fan
    temp_1: Number(),
    temp_2: Number(),
    // Overtemperature protection limits
    temp_1_limit: 50,
    temp_2_limit: 50,
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
    oneshot_len: Number()
  },
  // Called by server-sent event handler
  update_state(new_state) {
    if (this.debug) {
      console.log("update_state called with", new_state);
    }
    for (let key in this.state) {
      if (new_state[key] !== undefined) {
        this.state[key] = new_state[key];
      }
    }
    // Probably unsafe as it adds arbitrary new attributes from unsafe JSON
    //Object.assign(this.state, new_state);
  }
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
      state: view_state_store.state,
      disabled: true
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
    }
  },
  created() {
    this.app_watchdog = new AppWatchdog(1500, s => this.set_disabled(s));
    this.request_generator = new AsyncRequestGenerator("/cmd");
    this.sse_handler = new ServerSentEventHandler(
      "/events",
      view_state_store,
      this.app_watchdog
    );
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
