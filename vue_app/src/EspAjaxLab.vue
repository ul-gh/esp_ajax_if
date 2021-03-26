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
          @action="dispatch_action"
        >
        </component>
      </li>
    </span>
  </ul>
</template>

<script>
import LiveController from "./components/LiveController.vue";
import OperationSettings from "./components/OperationSettings.vue";
import HelpDocumentation from "./components/HelpDocumentation.vue";
import NetworkAndUpdate from "./components/NetworkAndUpdate.vue";

import useApiState from "./api/useApiState.js";

export default {
  name: "EspAjaxLab",
  components: {
    LiveController,
    OperationSettings,
    HelpDocumentation,
    NetworkAndUpdate,
  },
  setup() {
    // const {debug,
    //        disabled,
    //        state,
    //        update_state,
    //        update_computed_values,
    //        dispatch_action,
    //        sse_handler,
    //        } = useApiState();
    return useApiState();
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
    };
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
