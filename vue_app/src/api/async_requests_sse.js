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

// Make request with request_obj turned into JSON data.
// Retuns object from response JSON.
async function do_json_request(url_endpoint, obj_input=undefined, method='GET') {
  const json_str = JSON.stringify(obj_input);
  let request_body = undefined;
  if (method === 'POST') {
    request_body = json_str;
  } else if (method === 'GET') {
    if (json_str) {
      url_endpoint += `?${encodeURIComponent(json_str)}`;
    }
  }
  // Prepare fetch request
  const fetch_options = {
    method: method,
    //mode: 'cors', // no-cors, *cors, same-origin
    cache: 'no-cache',
    //credentials: 'same-origin', // include, *same-origin, omit
    headers: {
      'Content-Type': 'application/json'
      // 'Content-Type': 'application/x-www-form-urlencoded',
    },
    redirect: 'error', // manual, *follow, error
    // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin,
    // same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
    referrerPolicy: 'no-referrer',
    // body data type must match "Content-Type" header
    body: request_body,
  };
  const response = await fetch(url_endpoint, fetch_options);
  if (!response.ok) {
    alert("Server error, bad request or bad configuration...");
    return {};
  } else {
    const obj_out = await response.json();
    return obj_out;
  }
}

// Configure minimum delay between two requests sent to the HTTP server in ms
const request_interval_min_ms = 300;
// Configure Server-Sent-Event source reconnect timer timeout in ms
const sse_reconnect_timeout = 3000;

/** Asynchronous and rate-limited HTTP GET request generator for single endpoint
 */
class AsyncRequestGenerator {
  constructor(endpoint) {
    // HTTP GET API remote endpoint
    this._endpoint = endpoint;
    // For assuring a minimum delay between HTTP requests
    this._rate_limit_active = false;
    // Last request string received which could not be sent due rate limit
    this._pending_req_str = "";
  }

  /** Send HTTP requests rate-limited by a minimum delay timer.
   * If requests cannot be sent due to exceeding the maximum rate,
   * all intermediate requests are ignored without any action, except for the
   * very last one which is stored for automatic pending transmission.
   */
  async do_passive_get_request(req_str) {
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
    setTimeout(() => {
      this._rate_limit_active = false;
      this.do_passive_get_request("");
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
    let req_str;
    if (value === undefined) {
      req_str = `${this._endpoint}?${name}`;
    } else {
      req_str = `${this._endpoint}?${name}=${value}`;
    }
    await this.do_passive_get_request(req_str);
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
  constructor(endpoint, callback, watchdog) {
    this.endpoint = endpoint;
    this.callback = callback;
    this.watchdog = watchdog;
    // The backend SSE source
    this.source = null;
    this.reconnect_timer_id = undefined;
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
    try {
      this.source = new EventSource(this.endpoint);
      this.source.addEventListener("open", _ => console.log("Events Connected"));
      this.source.addEventListener("error", err => this.on_error(err));
      this.source.addEventListener("hw_app_state", e => this.on_hw_app_state(e));
    } catch (err) {
      this.source = null;
      this.on_error(err);
    }
  }

  /** For debugging, disable reconnect feature cluttering the console.
   * Also disables the app watchdog.
   */
  disable_reconnect_and_watchdog(bv) {
    if (bv) {
      clearTimeout(this.reconnect_timer_id);
      this.source.removeEventListener("error", this.on_error);
      this.watchdog.disable();
    } else {
      this.connect();
      this.watchdog.enable();
    }
  }

  on_hw_app_state(e) {
    // Since app state update telegram is emitted
    // periodically, we use this to reset the watchdog.
    this.watchdog.reset();
    const remote_state = JSON.parse(e.data);
    this.callback(remote_state);
  }

  on_error(err) {
    const connected = this.source != null && this.source.readyState == EventSource.OPEN;
    console.error(`${connected ? "" : "Events disconnected! "}SSE event source error: `, err);
    // Start Auto-Reconnect in any case...
    this.reconnect_timer_id = setTimeout(() => this.connect(), sse_reconnect_timeout);
  }
}


/** Watchdog calls "on_timeout" callback with "true" argument when expired,
 * and calls with "false" when reset is needed
 */
class AppWatchdog {
  constructor(timeout_ms, on_timeout) {
    this.timeout_ms = timeout_ms;
    this.on_timeout = on_timeout;
    this.timer_id = undefined;
    this._triggered = true;
    // Debug
    console.log("App Watchdog instance created...");
  }

  enable() {
    this.timer_id = setTimeout(
      () => {
        this._triggered = true;
        this.on_timeout(true);
      },
      this.timeout_ms);
  }

  disable() {
    clearTimeout(this.timer_id);
    if (this._triggered) {
      this._triggered = false;
      this.on_timeout(false);
    }
  }

  reset() {
    this.disable();
    this.enable();
  }
}


export {
  do_json_request,
  AsyncRequestGenerator,
  ServerSentEventHandler,
  AppWatchdog
};