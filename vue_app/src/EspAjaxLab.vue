<!-- ESP Ajax Lab - Web Application for Live Feedback Remote Hardware Control
 *
 * See home page: https://github.com/ul-gh/esp_ajax_if
 *
 * This single-page application is built using Vue.js 3.
 * 
 * Remote hardware is interfaced using ES7 fetch API and async co-routines
 * performing outgoing HTTP GET requests.
 *
 * The return channel is built using Server-Sent Events (SSE) updating the
 * application state and view with async data telegrams received in JSON format.
 * 
 * => See imported modules in "./api/useApiStore.js"
 *
 * This main app component is a container for a tabbed view of sub-components.
 * The sub-components are provided with the HTTP API via a state object
 * and action method added as Vue component properties.
 * 
 * 2021-03-27 Ulrich Lukas
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
            v-for="(tab, index) in tabs"
            :key="index"
            :class="['tab-button', { active: $route.path === tab.path }]"
            @click="open_tab(tab.path)"
          >
            {{ tab.name }}
          </button>
          <!--
          <button class="tab-button tab-spacer">
            &nbsp;
          </button>
          -->
        </span>
      </li>
      <li class="grid_item">
        <router-view
          :ref="$route.name"
          :state="state"
          :disabled="disabled"
          @action="dispatch_action"
        />
      </li>
    </span>
  </ul>
</template>

<script>
import LiveController from "./views/LiveController.vue";
import OperationSettings from "./views/OperationSettings.vue";
import HelpDocumentation from "./views/HelpDocumentation.vue";
import NetworkAndUpdate from "./views/NetworkAndUpdate.vue";

import useApiStore from "./api/useApiStore.js";

export default {
  name: "EspAjaxLab",
  components: {
    LiveController,
    OperationSettings,
    HelpDocumentation,
    NetworkAndUpdate,
  },
  setup() {
    // HTTP API interface implemented in module imported above (api/useApiStore.js).
    // API request actions and live representation of remote state are
    // implemented similar in style of a Vuex store.
    // The store properties, the state object and methods and are injected
    // into the application instance using the Vue 3 composition API, see:
    // https://v3.vuejs.org/guide/composition-api-introduction.html
    const {debug,
           disabled,
           state,
           update_state,
           update_computed_values,
           dispatch_action,
           sse_handler,
           } = useApiStore();
    return {debug,
            disabled,
            state,
            dispatch_action,
            };
  },
  data() {
    // All named routes defined in router/index.js (We omit the catch-all-route)
    const tabs = this.$router.options.routes.filter(r => r.hasOwnProperty('name'));
    //const tab_paths = routes.map(route => route.path);
    return {
      tabs,
    };
  },
  methods: {
    open_tab(path) {
      this.$router.push(path);
    },
  },
};
</script>

<style src="./main_app.css">
</style>

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
