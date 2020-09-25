/** AJAX for single-page web application
 * 
 * This uses ES7 fetch API and async co-routines for performing HTTP requests.
 * Server-Sent Events (SSE) are used to update the application view
 * with state updates sent back from the remote hardware server.
 * 
 * 2020-09-24 Ulrich Lukas
 */

// Minimum delay between two requests sent to the HTTP server in ms
const request_interval_min_ms = 200;
// Global object storing the application state, this is updated from
// server PUSH updates via the SSE event handler.
let remote_state = {};

class AsyncRequestGenerator {
    constructor () {
        // All HTML elements which are of class "ajax_btn" and have a "name"
        // and a "value" attribute can be used to generate HTTP requests.
        this.all_btns = Array.from(document.getElementsByClassName("ajax_btn"));
        // For assuring a minimum delay between HTTP requests
        this._rate_limit_active = false;
        // Last request string received which could not be sent due rate limit
        this._pending_req_str = "";
        // Connect HTML document events.
        document.addEventListener("submit", this.submit_formdata, true);
        // Connect single control click events
        document.addEventListener("click", this.submit_button, true);
        //document.addEventListener(
        //    "keydown",
        //    (e) => {if (e.key == "Enter") {this.submit_input(e);}},
        //    true);
        // Connect "input" events, e.g. from number and range type inputs
        document.addEventListener("input", this.submit_input, true);
    }

    /** Send HTTP requests rate-limited by a minimum delay timer.
     * We do not queue requests but only store the last one.
     * If requests cannot be sent due to exceeding the maximum rate,
     * all intermediate requests are ignored without any action, but the
     * very last one is stored for delayed transmission.
     */
    async do_http_request(req_str) {
        if (this._rate_limit_active) {
            this._pending_req_str = req_str;
            return;
        }
        // Rate limit timer expired, ready to send any request...
        if (!req_str) {
            if (this._pending_req_str) {
                req_str = this._pending_req_str;
                this._pending_req_str = "";
            } else {
                return;
            }
        }
        rate_limit_active = true;
        // Recursive call. If no requests are pending when timer expires,
        // only the timer_expired flag is reset.
        window.setTimeout(() => {this.rate_limit_active = false;
                                 this.do_http_request("");
                                 },
                          request_interval_min_ms
                          );
        try {
            const response = await fetch(req_str);
            if (!response.ok) {
                throw new Error('Network error.');
            }
        } catch (err) {
            alert(err);
        }
    }

    async send_cmd(name, value) {
        const req_str = "cmd?" + name + "=" + value;
        await this.do_http_request(req_str);
    }

    async submit_formdata(event) {
        event.preventDefault(); // Disable default "GET" form action
        const form_data = new FormData(event.target);
        const req_str = "cmd?" + new URLSearchParams(form_data).toString();
        await this.do_http_request(req_str);
    }

    async submit_button(event) {
        const target = event.target;
        for (let i = 0; i < this.all_btns.length; i++) {
            let btn = this.all_btns[i];
            // An element "contains" itself so this yields a valid result
            // even when the control is a single HTML DOM element
            if (btn.contains(target)) {
                if (btn.classList.contains("toggle-switchy")) {
                    // For these buttons, we use custom actions. The buttons
                    // are not immediately toggled visibly but only when an
                    // SSE response from the server is received.
                    event.preventDefault();
                    let btn_input = btn.firstElementChild;
                    var disabled = btn_input.hasAttribute("disabled");
                    var req_name = btn_input.getAttribute("name");
                    // For button to toggle, requested state is INVERSE!
                    var req_value = btn_input.checked ? "false" : "true"
                } else {
                    var disabled = btn.hasAttribute("disabled");
                    var req_name = btn.getAttribute("name");
                    var req_value = btn.getAttribute("value");
                }
                if (!disabled) {
                    await this.send_cmd(req_name, req_value);
                }
            }
        }
    }

    // Sends control name and value pair for "input" or similar events
    async submit_input(event) {
        const t = event.target;
        if (t.hasAttribute("disabled")) {
            return;
        }
        await this.send_cmd(t.name, t.value);
    }
}

// Update view with app state telegram when pushed via SSE
class ViewUpdater {
    constructor() {
        // Prevent updating the input fields content with remote data
        // when user starts entering a new value.
        this.view_updates_inhibited = false;
        // Same but only inhibits range slider updates
        this.range_input_updates_inhibited = false;
        // Array of currently edited elements which are not receiving updates
        this.last_edited_input_els = [];
        // Register all application elements representing the
        // remote state view handled and updated by this class.
        this.connection_vw = document.getElementById("connection_vw");
        this.power_pwm_vw = document.getElementById("power_pwm_vw");
        this.shutdown_vw = document.getElementById("shutdown_vw");
        this.aux_temp_vw = document.getElementById("aux_temp_vw");
        this.heatsink_temp_vw = document.getElementById("heatsink_temp_vw");
        this.fan_vw = document.getElementById("fan_vw");
        this.btn_pwm_on = document.getElementById("btn_pwm_on");
        this.btn_pwm_off = document.getElementById("btn_pwm_off");
        this.ref_vw = document.getElementById("ref_vw");
        this.dut_vw = document.getElementById("dut_vw");
        this.lead_dt_vw = document.getElementById("lead_dt_vw");
        this.lag_dt_vw = document.getElementById("lag_dt_vw");
        this.current_limit_vw = document.getElementById("current_limit_vw");
        this.frequency_number_vw = document.getElementById("frequency_number_vw");
        this.frequency_range_vw = document.getElementById("frequency_range_vw");
        this.duty_number_vw = document.getElementById("duty_number_vw");
        this.duty_range_vw = document.getElementById("duty_range_vw");
        this.all_remote_vws = [
            this.power_pwm_vw, this.shutdown_vw, this.aux_temp_vw,
            this.heatsink_temp_vw, this.fan_vw, this.ref_vw, this.dut_vw,
            this.lead_dt_vw, this.lag_dt_vw, this.current_limit_vw,
            this.frequency_number_vw, this.frequency_range_vw,
            this.duty_number_vw, this.duty_range_vw,
            this.btn_pwm_on, this.btn_pwm_off,
            ];
        document.addEventListener(
            "input",
            (e) => {
                this.inhibit_view_updates(e.target);
            },
            true);
        // Allow again when editing is finished.
        document.addEventListener(
            "submit",
            (e) => this.allow_view_updates(e.target),
            true);
        document.addEventListener(
            "keydown",
            (e) => {
                if (e.key == "Enter") {
                    console.log("ENTER pressed with value: " + e.target.value);
                    this.allow_view_updates(e.target);
                }
            },
            true);
        document.addEventListener(
            "change",
            (e) => {
                const t = e.target;
                if (t.nodeName == "INPUT" && t.type == "range") {
                    this.allow_view_updates(t);
                }
            },
            true);
    }

    update() {
        if (this.view_updates_inhibited) {
            return;
        }
        if (remote_state.power_pwm_active) {
            this.power_pwm_vw.setAttribute("active", "active");
            this.power_pwm_vw.innerText = "Power PWM ON";
        } else {
            this.power_pwm_vw.removeAttribute("active");
            this.power_pwm_vw.innerText = "Power PWM OFF";
        }

        if (remote_state.hw_oc_fault_present) {
            this.shutdown_vw.setAttribute("fault_present", "true");
            this.shutdown_vw.innerText = "HW OC FAULT\nClick here to Reset!!";
        } else {
            this.shutdown_vw.removeAttribute("fault_present")
            if (remote_state.hw_oc_fault_occurred) {
                this.shutdown_vw.setAttribute("fault_occurred", "true");
                this.shutdown_vw.innerText = "HW OC fault latched!\nClick here to Reset!";
            } else {
                this.shutdown_vw.removeAttribute("fault_occurred");;
                this.shutdown_vw.innerText = "State: Normal";
            }
        }
        this.aux_temp_vw.innerText = Number(
            remote_state.aux_temp).toFixed(1) + " °C";
        this.heatsink_temp_vw.innerText = Number(
            remote_state.heatsink_temp).toFixed(1) + " °C";
        this.fan_vw.firstElementChild.checked = Boolean(
            remote_state.fan_active);
        this.ref_vw.firstElementChild.checked = Boolean(
            remote_state.relay_ref_active);
        this.dut_vw.firstElementChild.checked = Boolean(
            remote_state.relay_dut_active);
        // Operational settings
        this.lag_dt_vw.value = remote_state.lag_dt.toFixed(0);
        this.lead_dt_vw.value = remote_state.lead_dt.toFixed(0);
        this.current_limit_vw.value = remote_state.current_limit.toFixed(1);
        this.frequency_number_vw.value = remote_state.frequency.toFixed(1);
        this.duty_number_vw.value = remote_state.duty.toFixed(1);
        if (!this.range_input_updates_inhibited) {
            this.frequency_range_vw.value = remote_state.frequency.toFixed(1);
            this.duty_range_vw.value = remote_state.duty.toFixed(1);
        }
    }

    enable_all() {
        this.connection_vw.setAttribute("active", "active");
        this.connection_vw.innerText = "OK";
        this.all_remote_vws.forEach((item) => {
            if (item.classList.contains("toggle-switchy")) {
                item.firstElementChild.disabled = false;
            } else {
                item.removeAttribute("disabled");
            }
        });
    }

    disable_all() {
        this.connection_vw.removeAttribute("active");
        this.connection_vw.innerText = "No Connection\nto Hardware!";
        this.power_pwm_vw.innerText = "Unknown...";
        this.shutdown_vw.innerText = "Unknown...";
        this.aux_temp_vw.innerText = "Unknown...";
        this.heatsink_temp_vw.innerText = "Unknown...";
        this.all_remote_vws.forEach((item) => {
            if (item.classList.contains("toggle-switchy")) {
                item.firstElementChild.disabled = true;
            } else {
                item.setAttribute("disabled", "disabled");
            }
        });
    }

    inhibit_view_updates(input_el) {
        if (input_el.type == "range") {
            this.range_input_updates_inhibited = true;
            return;
        }
        if (this.all_remote_vws.includes(input_el)) {
            this.view_updates_inhibited = true;
            this.last_edited_input_els.push(input_el);
            // Sets color highlight while aditing element
            input_el.setAttribute("editing", "editing");
        }
    }

    allow_view_updates(input_el) {
        // When range slider is released, re-allow updates for this only
        if (input_el.type == "range") {
            this.range_input_updates_inhibited = false;
            return;
        }
        // Otherwise, re-allow all
        this.view_updates_inhibited = false;
        for (let i = this.last_edited_input_els.length-1; i >= 0; i--) {
            this.last_edited_input_els[i].removeAttribute("editing");
            this.last_edited_input_els.pop();
        }
    }
}


// Connect Server-Sent Events, with auto-reconnect timer
class ServerSentEventHandler {
    constructor(endpoint, reconnect_timeout,
        heartbeat_watchdog, view_updater) {
        this.endpoint = endpoint;
        this.heartbeat_watchdog = heartbeat_watchdog;
        this.view_updater = view_updater;
        this.source = null;
        const timeout = reconnect_timeout;
        this.reconnect_timer = () => window.setTimeout(
            () => this.connect(), timeout
        );
        this.connect();
    }

    connect() {
        if (this.source != null) {
            console.log("Reconnecting Server-Sent Events...");
            this.source.close();
        } else {
            console.log("Connecting Server-Sent Events...");
        }
        this.source = new EventSource(this.endpoint);
        //this.source.addEventListener(
        //    "heartbeat", this.heartbeat_watchdog.reset_cb, false);
        this.source.addEventListener(
            "open", event => { console.log("Events Connected"); },
            false
        );
        this.source.addEventListener(
            "error", event => {
                if (event.target.readyState != EventSource.OPEN) {
                    console.log("Events Disconnected!");
                    // Start Auto-Reconnect
                    this.reconnect_timer_id = this.reconnect_timer();
                }
            },
            false
        );
        this.source.addEventListener(
            "hw_app_state", event => {
                // Since app state update telegram is emitted
                // periodically, we use this to reset the watchdog.
                this.heartbeat_watchdog.reset("HB");
                // console.log(`App State: "${event.data}"`);
                // Set global object
                remote_state = JSON.parse(event.data);
                this.view_updater.update();
            }, false
        );
    }
    disable_reconnect() {
        window.clearTimeout(this.reconnect_timer_id);
    }
}


// Set connection state display to "nok" when heartbeats are missing
// TODO: Also integrate SSE reconnection here
class HeartbeatWatchdog {
    constructor(timeout, view_updater) {
        this.timeout = timeout;
        this.view_updater = view_updater;
        this.timer_id = this.start_timer();
        this.reset_cb = event => this.reset(event.data);
    }

    start_timer() {
        return window.setTimeout(() => this.set_nok(), this.timeout);
    }

    reset(data) {
        if (data == "HB") {
            window.clearTimeout(this.timer_id);
            this.set_ok();
            this.timer_id = this.start_timer();
        }
    }

    set_nok() {
        this.view_updater.disable_all();
    }

    set_ok() {
        this.view_updater.enable_all();
    }
}