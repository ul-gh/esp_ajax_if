/* HTTP server
 *
 * Based on ESPAsyncWebServer, see:
 * https://github.com/me-no-dev/ESPAsyncWebServer
 */
#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <map>
#include <functional>

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Callback function with string argument
using CbStringT = std::function<void(String)>;
// Callback function with float argument
using CbFloatT = std::function<void(float)>;
// Mapping used for resolving command strings received via HTTP request
// on the "/cmd" endpoint to specialised request handlers
using CmdMapT = std::map<String, CbStringT>;

class HTTPServer : AsyncWebServer
{
public:
    bool reboot_requested;
    CmdMapT cmd_map;
    
    HTTPServer(int port);
    // virtual ~HTTPServer();

    // Overload for string callbacks
    void register_command(const char* cmd_name, CbStringT cmd_callback);
    // Overload for float callbacks
    void register_command(const char* cmd_name, CbFloatT cmd_callback);

    void begin();

    /* Normal HTTP request handlers
     */
    void register_default_callbacks();

private:
    // on("/")
    static void onRootRequest(AsyncWebServerRequest *request);

    // on("/cmd")
    void onCmdRequest(AsyncWebServerRequest *request);

    // on("/update")
    // When update is initiated via GET
    void onUpdateRequest(AsyncWebServerRequest *request);
    // When file is uploaded via POST request
    static void onUpdateUpload(
        AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final);

    /* Catch-All-Handlers
    */
    static void onRequest(AsyncWebServerRequest *request);

    static void onBody(AsyncWebServerRequest *request,
        uint8_t *data, size_t len, size_t index, size_t total);

    static void onUpload(AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final);

    void debug_print(String value);
    static void hex_print(const char *s);
}; // class HTTPServer


/* Handler for captive portal page, only active when in access point mode
*/
class CaptiveRequestHandler : public AsyncWebHandler
{
public:
    CaptiveRequestHandler();
    // virtual ~CaptiveRequestHandler();

    bool canHandle(AsyncWebServerRequest *request);
    void handleRequest(AsyncWebServerRequest *request);
}; // class CaptiveRequestHandler

#endif