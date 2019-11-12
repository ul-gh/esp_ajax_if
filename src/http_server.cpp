/* HTTP server
 *
 * Based on ESPAsyncWebServer, see:
 * https://github.com/me-no-dev/ESPAsyncWebServer
 */
#include <Update.h>
#include <SPIFFS.h>
#include <FS.h>
#include "http_server.h"

HTTPServer::HTTPServer(int port)
    : AsyncWebServer(port),
    reboot_requested{false}
    {
}
// virtual HTTPServer::~HTTPServer() {}

void HTTPServer::register_command(const char* cmd_name,
                                  CbStringT cmd_callback) {
    cmd_map[cmd_name] = cmd_callback;
    Serial.println(String("Registered String command: ") + cmd_name);
}

void HTTPServer::register_command(const char* cmd_name,
                                  CbFloatT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback] (String value) {
        cmd_callback(value.toFloat()); // Defaults to zero for invalid string?
    };
    Serial.println(String("Registered float command: ") + cmd_name);
}

void HTTPServer::begin() {
    register_default_callbacks();
    AsyncWebServer::begin();
}

/* Normal HTTP request handlers
 */
void HTTPServer::register_default_callbacks() {
    // Route for main application home page
    on("/", HTTP_GET, onRootRequest);
    // Serve static HTML and related files content
    serveStatic("/", SPIFFS, "/www/");
    // Route for REST API
    //using namespace std::placeholders;
    //on("/cmd", HTTP_GET, std::bind(&HTTPServer::onCmdRequest, this, _1));
    on("/cmd", HTTP_GET, [this](AsyncWebServerRequest *request) {
            onCmdRequest(request);
        }
    );
    // respond to GET requests on URL /heap
    on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", String(ESP.getFreeHeap()));
       }
    );
    // OTA Firmware Upgrade, see form method in data/www/upload.html
    on("/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
            onUpdateRequest(request);
       },
       onUpdateUpload
    );
    onNotFound(onRequest);
    onFileUpload(onUpload);
    onRequestBody(onBody);
    // Handler called when any DNS query is made via access point
    // addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    Serial.println("Default callbacks set up");
}


// on("/")
void HTTPServer::onRootRequest(AsyncWebServerRequest *request) {
    //request->send(SPIFFS, "/control.html", String(), false, processor);
    request->send(SPIFFS, "/www/control.html");
}

// on("/cmd")
void HTTPServer::onCmdRequest(AsyncWebServerRequest *request) {
    int n_params = request->params();
    Serial.println(String("Number of parameters received: ") + n_params);
    for (int i = 0; i < n_params; ++i) {
        AsyncWebParameter *p = request->getParam(i);
        String name = p->name();
        String value_str = p->value();
        Serial.println("-----\n"
                       "Param name: " + name
                       + "\nParam value: " + value_str);
        auto cb_iterator = cmd_map.find(name);
        if (cb_iterator == cmd_map.end()) {
            Serial.println(name + ": entry not registered in command mapping!");
            continue;
        }
        CbStringT cmd_callback = (*cb_iterator).second; // = cmd_map[name];
        if (!cmd_callback) {
            Serial.println(name + ": Error: Not a callable object!");
            continue;
        }
        // Finally call callback
        cmd_callback(value_str);
    }
    // For AJAX interface: Return "OK"
    request->send(200, "text/plain", String("OK"));
    // Alternatively for plain HTTP form input, resend form:
    //request->send(SPIFFS, "/www/control.html");
}

// on("/update")
// When update is initiated via GET
void HTTPServer::onUpdateRequest(AsyncWebServerRequest *request) {
    reboot_requested = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/plain", reboot_requested ? "OK": "FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
}
// When file is uploaded via POST request
void HTTPServer::onUpdateUpload(AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    if(!index) {
        Serial.printf("Update Start: %s\n", filename.c_str());
        //Update.runAsync(true);
        if(!Update.begin((ESP.getFreeSketchSpace()-0x1000) & 0xFFFFF000)) {
            Update.printError(Serial);
        }
    }
    if(!Update.hasError()) {
        if(Update.write(data, len) != len){
            Update.printError(Serial);
        }
    }
    if(final) {
        if(Update.end(true)) {
            Serial.printf("Update Success: %uB\n", index+len);
        }
        else {
            Update.printError(Serial);
        }
    }
} // on("/update")

/* Catch-All-Handlers
 */
void HTTPServer::onRequest(AsyncWebServerRequest *request) {
    //Handle Unknown Request
    request->send(404);
}

void HTTPServer::onBody(AsyncWebServerRequest *request,
        uint8_t *data, size_t len, size_t index, size_t total) {
    //Handle body
}

void HTTPServer::onUpload(AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    //Handle upload
}

void HTTPServer::debug_print(String value) {
    Serial.println("String value is: " + value + "\nAnd char hex value is: ");
    hex_print(value.c_str());
}

void HTTPServer::hex_print(const char *s) {
    char cbuffer[4];
    char sbuffer[200] = {0};
    char* p = (char*) s;
    while (*p) {
        snprintf(cbuffer, sizeof(cbuffer), isalnum((int) *p) ? "%c" : "\\%hhx", *p);
        strncat(sbuffer, cbuffer, sizeof(sbuffer) - 2 - strlen(sbuffer)); 
        p++;
    }
    strncat(sbuffer, "\n", 1);
    Serial.print(sbuffer);
}


/* Handler for captive portal page, only active when in access point mode
*/
CaptiveRequestHandler::CaptiveRequestHandler() {}
// virtual CaptiveRequestHandler::~CaptiveRequestHandler() {}

bool CaptiveRequestHandler::canHandle(AsyncWebServerRequest *request) {
    //request->addInterestingHeader("ANY");
    return true;
}

void CaptiveRequestHandler::handleRequest(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->printf(
            "<!DOCTYPE html><html><head><title>Captive Portal</title></head>"
            "<body><p>Captive portal front page.</p>"
            "<p>You were trying to reach: http://%s%s</p>"
            "<p>Opening <a href='http://%s'>this link</a> instead</p>"
            "</body></html>",
            request->host().c_str(), request->url().c_str(),
            WiFi.softAPIP().toString().c_str()
    );
    request->send(response);
}
