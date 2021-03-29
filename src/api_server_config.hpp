struct APIServerConfig
{
    // When set to yes, mount SPIFFS filesystem and serve static content
    // from files contained in data/www subfolder at the "/static/" URL route.
    bool serve_static_from_spiffs = true;
    // Name of the SPIFFS subfolder storing the static files. Default is "www"
    const char* spiffs_static_files_folder = "www";
    // The name of the URL route for static file content. Default is "/".
    const char* static_route = "/";
    // Route for which all sub-routes should be served the same index.html file
    const char* app_route = "/app*";
    // Main page / main application HTML file (public location not SPIFFS path).
    // Served when application route or sub-routes are requested.
    const char* index_html_file = "/index.html";
    // Add to static file responses a CacheControl header.
    // Default is "public, max-age=86400" which instructs remote clients to
    // normally not request again for the specified time in seconds (one day).
    const char* cache_control = "public, max-age=86400";

    // Activate template processing when defined.
    // This should only make sense when not using AJAX..
    bool template_processing_activated = false;

    // Common API endpoint for AJAX GET requests registered via register_ajax_cb()
    const char* api_endpoint = "/cmd";
    // For AJAX, reply with an plain string, default is empty string.
    // When not using AJAX, reply with content from string "api_return_html" as
    // defined in separate header http_content.hpp
    bool api_is_ajax = true;
    const char* ajax_return_text = "OK";

    // Activate Server-Sent-Event source
    bool use_sse = true;
    // Default URL for the SSE endpoint is "/events"
    const char* sse_endpoint = "/events";

    // When set to true, reboot the system on request or after updates
    bool reboot_enabled = false;

    // Activate HTTP Basic Authentication, set to true when user/password is given
    bool http_auth_activated = false;
    // HTTP Basic Authentication username and password
    const char* http_user = "";
    const char* http_pass = "";

    // Errors
    const char* error_404_html = "Error 404, file not found!";
};