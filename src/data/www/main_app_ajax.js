/** AJAX for single-page web application
 * 
 * This uses ES7 fetch API and async co-routines for performing HTTP requests.
 * Server-Sent Events (SSE) are used to update the application view
 * with state updates sent back from the remote hardware server.
 * 
 * 2020-11-18 Ulrich Lukas
 * License: GPL v.3
 */

// Configure minimum delay between two requests sent to the HTTP server in ms
const request_interval_min_ms = 300;
// Configure Server-Sent-Event source reconnect timer timeout in ms
const sse_reconnect_timeout = 3000;

/** Singleton class implementing methods for performing asynchronous
 * HTTP requests from received HTML DOM input events.
 * 
 * HTML DOM input events are connected to the request emitting methods
 * in the constructor.
 */
class AsyncRequestGenerator {
    constructor () {
        // All HTML elements which are of class "ajax_btn" and have a "name"
        // and a "value" attribute can be used to generate HTTP requests.
        this.all_btns = Array.from(document.getElementsByClassName("ajax_btn"));
        // All form inputs. These are only be supposed to be submitted as
        // part of the form they belong to, not as a single input.
        this.all_forminputs = [];
        for (let form of document.forms) {
            for (let input of form) {
                this.all_forminputs.push(input);
            }
        }
        // For assuring a minimum delay between HTTP requests
        this._rate_limit_active = false;
        // Last request string received which could not be sent due rate limit
        this._pending_req_str = "";
        //// Connect HTML document events
        document.addEventListener("click", e => this.submit_button(e), true);
        // FIXME: Do we need this?
        //document.addEventListener(
        //    "keydown",
        //    e => {if (e.key == "Enter") this.submit_input(e)},
        //    true);
        document.addEventListener("submit", e => this.submit_formdata(e), true);
        // Button click events
        document.addEventListener("change", e => this.submit_nonform_input(e), true);
        // Connect "input" events, only used here for range type inputs
        document.addEventListener("input", e => this.submit_range_input(e), true);
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
            // Empty request means either the timer expired or a bug..
            if (this._pending_req_str) {
                req_str = this._pending_req_str;
                this._pending_req_str = "";
            } else {
                // This is always reached when timer expires and
                // there is no pending request.
                return;
            }
        }
        this._rate_limit_active = true;
        // Recursive call. If no requests are pending when timer expires, the
        // called method exits early and only rate_limit_active flag is reset.
        window.setTimeout(() => {this._rate_limit_active = false;
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

    /** Send name=value pair as a rate-limited HTTP request
     */
    async send_cmd(name, value) {
        const req_str = "/cmd?" + name + "=" + value;
        await this.do_http_request(req_str);
    }

    /** This has no check if the event was targeted at a form input element,
     * so this should only be connected to the form "submit" event
     */
    async submit_formdata(event) {
        event.preventDefault(); // Disable default "GET" form action
        const t = event.target;
        if (t.hasAttribute("disabled")) {
            return;
        }
        const form_data = new FormData(t);
        const req_str = "/cmd?" + new URLSearchParams(form_data).toString();
        await this.do_http_request(req_str);
    }

    /** This only reacts on events targeted at one of the registered buttons
     */
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

    /** Sends control name=value pair for slider "range" input or similar
     */
    async submit_range_input(event) {
        const t = event.target;
        if (t.type != "range" || t.hasAttribute("disabled")) {
            return;
        }
        await this.send_cmd(t.name, t.value);
    }

    /** Sends control name and value pair for number input or similar,
     * but only if this is not part of a form
     */
    async submit_nonform_input(event) {
        const t = event.target;
        if (this.all_forminputs.includes(t) || t.hasAttribute("disabled")) {
            return;
        }
        await this.send_cmd(t.name, t.value);
    }
}

/** Update all application view elements representing the remote state
 * with data recieved as "push" update from the hardware server.
 * 
 * For this, the update() method is called from the SSE handler
 * when data is received.
 * 
 * This also connects some HTML DOM input events in order to inhibit
 * view updates from the remote side when view elements are being edited.
 */
class ViewUpdater {
    constructor(sse_state_update_interval_ms=500) {
        // First register all application elements representing the
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
        this.frequency_min_vw = document.getElementById("frequency_min_vw");
        this.frequency_max_vw = document.getElementById("frequency_max_vw");
        this.frequency_number_vw = document.getElementById("frequency_number_vw");
        this.frequency_range_vw = document.getElementById("frequency_range_vw");
        this.duty_number_vw = document.getElementById("duty_number_vw");
        this.duty_range_vw = document.getElementById("duty_range_vw");
        // All items which are disabled when connection to server is lost
        this.all_disable_items = [
            this.power_pwm_vw, this.shutdown_vw, this.aux_temp_vw,
            this.heatsink_temp_vw, this.fan_vw, this.ref_vw, this.dut_vw,
            this.lead_dt_vw, this.lag_dt_vw, this.current_limit_vw,
            this.frequency_min_vw, this.frequency_max_vw,
            this.frequency_number_vw, this.frequency_range_vw,
            this.duty_number_vw, this.duty_range_vw,
            this.btn_pwm_on, this.btn_pwm_off,
            ];
        // We also want to disable the submit buttons
        for (let form of document.forms) {
            for (let input of form) {
                if (input.type == "submit") this.all_disable_items.push(input);
            }
        }
        ///////////////////////////////////////////////////////////////////////
        this.sse_state_update_interval_ms = sse_state_update_interval_ms;
        // Prevent updating the input fields content with remote data
        // when user starts entering a new value.
        this.view_updates_inhibited = false;
        // Same but only inhibits range slider updates
        this.range_input_updates_inhibited = false;
        // Form elements edited before submit. These are highlighted as changed.
        this.form_edited_inputs = [];
        // The methods are registered as event handlers using a lambda
        // for correct binding of the instance via "this" object
        document.addEventListener(
            "input",
            e => this.inhibit_view_updates(e.target),
            true);
        document.addEventListener(
            "submit",
            e => this.allow_view_updates(e.target),
            true);
        document.addEventListener(
            "keydown",
            e => {
                if (e.key == "Enter") {
                    console.log("ENTER pressed with value: " + e.target.value);
                    this.allow_view_updates(e.target);
                }
            },
            true);
        document.addEventListener(
            "change",
            e => {
                const t = e.target;
                if (t.nodeName == "INPUT" && t.type == "range") {
                    this.allow_view_updates(t);
                }
            },
            true);

        // Set initial state of all view elements, which is "unknown" by default
        this.disable_all();
    }

    /** Refresh the view with data from the SSE event source
     */
    update(remote_state) {
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
        this.current_limit_vw.value = remote_state.current_limit.toFixed(0);
        const frequency_min = remote_state.frequency_min.toFixed(0);
        const frequency_max = remote_state.frequency_max.toFixed(0);
        const frequency = remote_state.frequency.toFixed(0);
        const duty = remote_state.duty.toFixed(1);
        this.frequency_min_vw.value = frequency_min;
        this.frequency_max_vw.value = frequency_max;
        this.frequency_number_vw.min = frequency_min;
        this.frequency_range_vw.min = frequency_min;
        this.frequency_number_vw.max = frequency_max;
        this.frequency_range_vw.max = frequency_max;
        this.frequency_number_vw.value = frequency
        this.duty_number_vw.value = duty;
        if (!this.range_input_updates_inhibited) {
            this.frequency_range_vw.value = frequency
            this.duty_range_vw.value = duty;
        }
    }

    /** Called when connection to the server is established
     */
    enable_all() {
        this.connection_vw.setAttribute("active", "active");
        this.connection_vw.innerText = "OK";
        this.all_disable_items.forEach((item) => {
            if (item.classList.contains("toggle-switchy")) {
                item.firstElementChild.disabled = false;
            } else {
                item.removeAttribute("disabled");
            }
        });
    }

    /** Called when connection to server is lost
     */
    disable_all() {
        this.connection_vw.removeAttribute("active");
        this.connection_vw.innerText = "No Connection\nto Hardware!";
        this.power_pwm_vw.innerText = "Unknown...";
        this.shutdown_vw.innerText = "Unknown...";
        this.aux_temp_vw.innerText = "Unknown...";
        this.heatsink_temp_vw.innerText = "Unknown...";
        this.all_disable_items.forEach((item) => {
            if (item.classList.contains("toggle-switchy")) {
                item.firstElementChild.disabled = true;
            } else {
                item.setAttribute("disabled", "disabled");
            }
        });
    }

    /** Called when user starts editing an input also receiving updates
     * from the remote side to prevent clashing of inputs
     */
    inhibit_view_updates(input_el) {
        if (input_el.type == "range") {
            this.range_input_updates_inhibited = true;
            return;
        }
        if (this.all_disable_items.includes(input_el)) {
            this.view_updates_inhibited = true;
            this.form_edited_inputs.push(input_el);
            // Sets color highlight while aditing element
            input_el.setAttribute("editing", "editing");
        }
    }

    /** Opposite of inhibit_view_updates 
     */
    allow_view_updates(input_el) {
        // When range slider is released, re-allow updates for this only
        // This is delayed to prevent jumping of the slider due to outdated updates
        if (input_el.type == "range") {
            window.setTimeout(() => this.range_input_updates_inhibited = false,
                              this.sse_state_update_interval_ms * 1.5);
            return;
        }
        // Otherwise, re-allow all
        this.view_updates_inhibited = false;
        for (let i = this.form_edited_inputs.length-1; i >= 0; i--) {
            this.form_edited_inputs[i].removeAttribute("editing");
            this.form_edited_inputs.pop();
        }
    }
}


/** Connect Server-Sent Events to the view_updater.update() method.
 * This also features an auto-reconnect timer. Also, watchdog.reset() is
 * triggered on periodic SSE updates
 */
class ServerSentEventHandler {
    constructor(endpoint, view_updater, watchdog) {
        this.endpoint = endpoint;
        this.view_updater = view_updater;
        this.watchdog = watchdog;
        // The backend SSE source
        this.source = null;
        this.connect();
    }

    /** Connect to the SSE server
     */
    connect() {
        if (this.source != null) {
            console.log("Reconnecting Server-Sent Events...");
            this.source.close();
        } else {
            console.log("Connecting Server-Sent Events...");
        }
        this.source = new EventSource(this.endpoint);
        this.source.addEventListener(
            "open", e => console.log("Events Connected"), false);
        this.source.addEventListener(
            "error",
            e => {
                if (e.target.readyState != EventSource.OPEN) {
                    console.log("Events Disconnected!");
                    // Start Auto-Reconnect
                    this.reconnect_timer_id = window.setTimeout(
                        () => this.connect(), sse_reconnect_timeout);
                }
            },
            false);
        this.source.addEventListener(
            "hw_app_state",
            e => {
                // Since app state update telegram is emitted
                // periodically, we use this to reset the watchdog.
                this.watchdog.reset();
                const remote_state = JSON.parse(e.data);
                this.view_updater.update(remote_state);
            },
            false);
    }
    
    /** For debugging, reconnect clutters the console.
     * Also disables the app watchdog.
     */
    disable_reconnect_and_watchdog() {
        window.clearTimeout(this.reconnect_timer_id);
        this.watchdog.disable();
    }
}


/** Watchdog timer inactivates all view elements when periodic updates are missing
 */
class AppWatchdog {
    constructor(timeout, view_updater) {
        this.timeout = timeout;
        this.view_updater = view_updater;
        this.enable();
    }

    enable() {
        this.timer_id = window.setTimeout(
            () => this.view_updater.disable_all(),
            this.timeout);
    }

    disable() {
        window.clearTimeout(this.timer_id);
        this.view_updater.enable_all();
    }

    reset() {
        this.disable();
        this.enable();
    }
}