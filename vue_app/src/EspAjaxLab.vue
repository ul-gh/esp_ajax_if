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
            v-for="tab in tabs"
            :key="tab"
            :class="['tab-button', { active: current_tab === tab }]"
            @click="current_tab = tab"
          >
            {{ tab }}
          </button>
          <button class="tab-button tab-spacer">
            &nbsp;
          </button>
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
import UpdateNetworkSetup from "./components/UpdateNetworkSetup.vue";

import {
  AsyncRequestGenerator,
  ServerSentEventHandler,
  AppWatchdog
} from "./async_requests_sse.js";

let view_state_store = reactive({
  debug: false,
  state: {
    frequency_min_hw: Number(),
    frequency_max_hw: Number(),
    dt_sum_max_hw: Number(),
    // Runtime user setpoint limits for output frequency
    frequency_min: 90.0,
    frequency_max: 110.0,
    // Operational setpoints for PSPWM module
    frequency: 100.0,
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
    oneshot_len: Number()
  },
  // Called by server-sent event handler
  update_state(new_state) {
    if (this.debug) {
      console.log("update_state called with", new_state);
    }
    for (let key in this.state) {
      this.state[key] = new_state[key];
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
    UpdateNetworkSetup
  },
  data() {
    return {
      tabs: ["LiveController", "OperationSettings", "UpdateNetworkSetup"],
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
  }
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
