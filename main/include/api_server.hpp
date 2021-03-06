/** @file api_server.hpp
 * 
 * License: GPL v.3 
 * U. Lukas 2020-11-18
 */
#ifndef API_SERVER_HPP__
#define API_SERVER_HPP__

#include <map>
#include <functional>

//#include <Arduino.h>
#include <Ticker.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "app_config.hpp"

// Callback function with string argument
using CbStringT = std::function<void(const String&)>;
// Callback function with float argument
using CbFloatT = std::function<void(const float)>;
// Callback function with integer argument
using CbIntT = std::function<void(const int)>;
// Callback function without arguments
using CbVoidT = std::function<void(void)>;

// Mapping used for resolving command strings received via HTTP request
// on the "/cmd" endpoint to specialised request handlers
using CmdMapT = std::map<String, CbStringT>;
// String replacement mapping for template processor
using TemplateMapT = std::map<String, String>;


/** @brief AJAX HTTP API server for ESP-AJAX-Lab
 *
 * Based on ESPAsyncWebServer, see:
 * https://github.com/me-no-dev/ESPAsyncWebServer
 * 
 * This implements higher-level API callbacks on the /cmd endpoint
 * 
 * For details, see README.md
 */
class APIServer
{
public:
    // API server configuration
    static constexpr APIServerConfig srv_conf{};
    // Base ESPAsyncWebServer
    AsyncWebServer* backend;
    // Server-Sent Events (SSE) for "PUSH" updates of application data
    AsyncEventSource* event_source;
    // Callback registry, see above
    CmdMapT cmd_map;
    // String replacement mapping for template processor
    TemplateMapT template_map;
    // Can be polled externally as an alternative to conf_enable_reboot=true
    bool reboot_requested;

    
    APIServer(AsyncWebServer* http_backend);
    ~APIServer();

    /** Set an entry in the template processor string <=> string mapping
     */
    void set_template(const char* placeholder, const char* replacement);

    /** Setup HTTP request callbacks to a common API endpoint,
     *  distinguished by individual command names.
     *
     * Overload for string argument callbacks:
     */
    void register_api_cb(const char* cmd_name, CbStringT cmd_callback);
    /** Setup HTTP request callbacks to a common API endpoint,
     *  distinguished by individual command names.
     *
     * Overload for float argument callbacks:
     */
    void register_api_cb(const char* cmd_name, CbFloatT cmd_callback);
    /** Setup HTTP request callbacks to a common API endpoint,
     *  distinguished by individual command names.
     *
     * Overload for integer argument callbacks:
     */
    void register_api_cb(const char* cmd_name, CbIntT cmd_callback);
    /** Setup HTTP request callbacks to a common API endpoint,
     *  distinguished by individual command names.
     *
     * Overload for void argument callbacks:
     */
    void register_api_cb(const char* cmd_name, CbVoidT cmd_callback);

    /** Start execution, includes starting the ESPAsyncWebServer backend.
     * Do not call this when using WifiManger or when backend has been
     * activated before by other means
     */
    void begin();


private:
    /////// Setup functions
    // Add Request URL rewrites to the server instance
    void _add_rewrites();
    // Add URL redirects to the server instance
    void _add_redirects();
    // Add request handlers to the server instance
    void _add_handlers();
    // Activate the SSE envent source if conf_use_sse == true
    void _add_event_source();
    // Helper function for _add_event_source, only sends "Hello" message and info print
    void _register_sse_on_connect_callback();

    /////// Backend callback implementation

    // on("/")
    void _on_root_request(AsyncWebServerRequest *request);
    // on("/cmd")
    void _on_cmd_request(AsyncWebServerRequest *request);
    // on("/update")
    // When update is initiated via GET
    void _on_update_request(AsyncWebServerRequest *request);
    // When file is uploaded via POST request
    static void _on_update_body_upload(
        AsyncWebServerRequest *request, const String& filename,
        size_t index, uint8_t *data, size_t len, bool final);
    // Catch-All-Handlers
    static void _on_request(AsyncWebServerRequest *request);
    static void _on_body(AsyncWebServerRequest *request,
        uint8_t *data, size_t len, size_t index, size_t total);
    static void _on_upload(AsyncWebServerRequest *request, const String& filename,
        size_t index, uint8_t *data, size_t len, bool final);

    //////
    // Template processor
    String _template_processor(const String& placeholder);
};

#endif /* API_SERVER_HPP__ */