/** Async HTTP GET requests and Server-Sent Event handling for vue.js app
 * 
 * This uses ES7 fetch API and async co-routines for performing HTTP requests.
 * 
 * Server-Sent Events (SSE) are used to update the application view
 * with state updates sent back from the remote hardware server.
 * 
 * 2021-01-09 Ulrich Lukas
 * License: GPL v.3
 */

// Configure minimum delay between two requests sent to the HTTP server in ms
const request_interval_min_ms = 300;
// Configure Server-Sent-Event source reconnect timer timeout in ms
const sse_reconnect_timeout = 3000;

/** Asynchronous and rate-limited HTTP request generator for single API endpoint
 */
class AsyncRequestGenerator {
    constructor (endpoint) {
        // HTTP GET API remote endpoint
        this._req_start_str = endpoint + "?";
        // For assuring a minimum delay between HTTP requests
        this._rate_limit_active = false;
        // Last request string received which could not be sent due rate limit
        this._pending_req_str = "";
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
        setTimeout(() => {this._rate_limit_active = false;
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
        const req_str = this._req_start_str + name + "=" + value;
        await this.do_http_request(req_str);
    }
}


/** Connect Server-Sent Events to the vue.js application.
 * 
 * This updates the remote state store when data is received via SSE.
 * 
 * Extra features:
 *   - implements auto-reconnect timer
 *   - calls watchdog.reset() on SSE reception
 */
class ServerSentEventHandler {
    constructor(endpoint, state_store, watchdog) {
        this.endpoint = endpoint;
        this.state_store = state_store;
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
            // eslint-disable-next-line no-unused-vars
            "open", e => console.log("Events Connected"), false);
        this.source.addEventListener(
            "error",
            e => {
                if (e.target.readyState != EventSource.OPEN) {
                    console.log("Events Disconnected!");
                    // Start Auto-Reconnect
                    this.reconnect_timer_id = setTimeout(
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
                this.state_store.update_state(remote_state);
            },
            false);
    }
    
    /** For debugging, disable reconnect feature cluttering the console.
     * Also disables the app watchdog.
     */
    disable_reconnect_and_watchdog() {
        clearTimeout(this.reconnect_timer_id);
        this.watchdog.disable();
    }
}


/** Watchdog timer cals disable_callback with "true" value on timeout,
 * and calls with "false" when reset
 */
class AppWatchdog {
    constructor(timeout_ms, disable_callback) {
        this.timeout_ms = timeout_ms;
        this.disable_callback = disable_callback;
        this._triggered = false;
    }

    enable() {
        this.timer_id = setTimeout(
            () => {
                this._triggered = true;
                this.disable_callback(true);
            },
            this.timeout_ms);
    }

    disable() {
        clearTimeout(this.timer_id);
         if (this._triggered) {
            this._triggered = false;
            this.disable_callback(false);
         }
    }

    reset() {
        this.disable();
        this.enable();
    }
}


export { AsyncRequestGenerator, ServerSentEventHandler, AppWatchdog };