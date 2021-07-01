import { reactive, ref, watch } from "vue";

import {
  AsyncRequestGenerator,
  ServerSentEventHandler,
  AppWatchdog
} from "./async_requests_sse.js";

export default function useApiStore() {
  const debug = ref(false);
  // Disable all controls by default, is enabled on watchdog reset
  const disabled = ref(true);

  const state = reactive({
    // Hardware limits
    frequency_min_hw: 0.01,
    frequency_max_hw: 1000,
    dt_sum_max_hw: 600,
    // duty_max_hw is now a onstant, see comment at function update_computed_values() below.
    duty_max_hw: 100.0,
    current_limit_max_hw: 100,
    // Runtime user setpoint limits for output frequency and duty cycle
    frequency_min: 90.00,
    frequency_max: 110.00,
    duty_min: 0.0,
    duty_max: 95.0,
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
    // WiFi / Network configuration is not included here as global state.
    // See NetworkAndUpdate.vue
  });

  // Called by server-sent event handler
  function update_state(new_state) {
    if (debug.value) {
      console.log("Updating app state with received object: ", new_state);
    }
    for (let key in state) {
      if (new_state.hasOwnProperty(key)) {
        state[key] = new_state[key];
      }
    }
    // Some of the application state is re-computed when state update is received
    update_computed_values();
  }

  function dispatch_action(name, value) {
    if (debug.value) {
      console.log(`Dispatching action "${name}" with value: ${value}`);
    }
    request_generator.send_cmd(name, value);
  }

  function update_computed_values() {
    state.hw_error = state.hw_oc_fault ? "HW OC FAULT" : state.hw_overtemp ? "OVERTEMPERATURE" : "";
    // Assuming the configured dead-time values correspond to actual hardware
    // switching times and delays, the net effect will be zero, i.e. the output
    // waveform will have maximum duty cycle for phase-shift of 180 degrees or
    // state.duty = 1.0...
    //const dt_effective = Math.max(state.lead_dt, state.lag_dt);
    //state.duty_max_hw = 100 * (1 - 1E-6 * state.frequency * 2 * dt_effective);
  }

  update_computed_values();

  const request_generator = new AsyncRequestGenerator("/cmd");
  const app_watchdog = new AppWatchdog(1500, val => disabled.value = val);
  const sse_handler = new ServerSentEventHandler("/events", update_state, app_watchdog);

  // When debug set to boolean true value, enable UI and disable auto-reconnect
  watch(debug, val => sse_handler.disable_reconnect_and_watchdog(val));

  return {
    debug,
    disabled,
    state,
    update_state,
    update_computed_values,
    dispatch_action,
    sse_handler,
  }
}
