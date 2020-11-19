// When set to yes, mount SPIFFS filesystem and serve static content
// from files contained in data/www subfolder at the "/static/" URL route.
constexpr bool conf_serve_static_from_spiffs = true;
// Name of the SPIFFS subfolder storing the static files. Default is "www"
constexpr const char* conf_spiffs_static_files_folder = "www";
// The name of the URL route for static file content. Default is "/static/".
constexpr const char* conf_static_route = "/static/";
// Default file served from SPIFFS when server root URL without filename is requested
constexpr const char* conf_index_html_file = "/static/main_app.html";
// Add to static file responses a CacheControl header.
// Default is "public, max-age=86400" which instructs remote clients to
// normally not request again for the specified time in seconds (one day).
constexpr const char* conf_cache_control = "public, max-age=86400";

// Activate template processing when defined.
// This should only make sense when not using AJAX..
constexpr bool conf_template_processing_activated = false;

// Common API endpoint for AJAX GET requests registered via register_ajax_cb()
constexpr const char* conf_api_endpoint = "/cmd";
// For AJAX, reply with an plain string, default is empty string.
// When not using AJAX, reply with content from string "api_return_html" as
// defined in separate header http_content.hpp
constexpr bool conf_api_is_ajax = true;
constexpr const char* conf_ajax_return_text = "OK";

// Activate Server-Sent-Event source
constexpr bool conf_use_sse = true;
// Default URL for the SSE endpoint is "/events"
constexpr const char* conf_sse_endpoint = "/events";
// Send heartbeat message via SSE event source in regular intervals when
// set to true. This needs regular calling of update_timer().
constexpr bool conf_sending_heartbeats = false;
// Interval time in milliseconds
constexpr unsigned long conf_heartbeat_interval = 500;
// Text string sent with the heartbeat event if enabled
static constexpr const char* conf_heartbeat_message = "HB";

// Activate HTTP Basic Authentication, set to true when user/password is given
constexpr bool conf_http_auth_activated = false;
// HTTP Basic Authentication username and password
constexpr const char* conf_http_user = "";
constexpr const char* conf_http_pass = "";

// Message to send via HTTP Server-Sent-Events when HW shutdown occurs
constexpr const char* conf_shutdown_message = "Hardware Shutdown occurred!";
// Normal reply
constexpr const char* conf_normal_message = "OK";

// Errors
constexpr const char* conf_error_404_html = "Error 404, file not found!";