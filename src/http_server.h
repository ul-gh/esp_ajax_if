/* HTTP server
 *
 * Based on ESPAsyncWebServer, see:
 * https://github.com/me-no-dev/ESPAsyncWebServer
 */

// Generalised callback type
using CbT = std::function<void(String value)>;
// Mapping used for resolving command strings received via HTTP request
// on the "/cmd" endpoint to specialised request handlers
using CmdMapT = std::map<String, CbT>;

class HTTPServer : AsyncWebServer {
public:
    CmdMapT cmdMap;
    
    HTTPServer(int port)
        : AsyncWebServer(port),
        cmdMap{ {String("set_output"), [this](auto value) {printFoo(value);}},
                {String("set_input"), [this](auto value) {printFoo(value);}},
        } {
    }
    // virtual ~HTTPServer() {}

    void begin() {
        registerCallbacks();
        AsyncWebServer::begin();
    }

    /* Normal HTTP request handlers
     */
    void registerCallbacks() {
        // Route for main application home page
        on("/", HTTP_GET, onRootRequest);
        // Serve static HTML and related files content
        serveStatic("/", SPIFFS, "/www/");
        // Route for REST API
        //using namespace std::placeholders;
        //on("/cmd", HTTP_GET, std::bind(&HTTPServer::onCmdRequest, this, _1));
        on("/cmd", HTTP_GET, [this](auto request) {onCmdRequest(request);});
        // respond to GET requests on URL /heap
        on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
                    request->send(200, "text/plain", String(ESP.getFreeHeap()));
                });
        // OTA Firmware Upgrade, see form method in data/www/upload.html
        on("/update", HTTP_POST, onUpdateRequest, onUpdateUpload);
        onNotFound(onRequest);
        onFileUpload(onUpload);
        onRequestBody(onBody);
        // Handler called when any DNS query is made via access point
        // addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
        Serial.println("Callbacks set up");
    } // registerCallbacks()

private:
    void printFoo(String value) {
        Serial.println("Nice Foo!, Value is: " + value);
    }

    // on("/")
    static void onRootRequest(AsyncWebServerRequest *request) {
        //request->send(SPIFFS, "/control.html", String(), false, processor);
        request->send(SPIFFS, "/www/control.html");
    }

    static void raw_print(const char *s) {
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

    // on("/cmd")
    void onCmdRequest(AsyncWebServerRequest *request) {
        int n_params = request->params();
        Serial.println(n_params);
        for (int i = 0; i < n_params; ++i) {
            static AsyncWebParameter *p = request->getParam(i);
            String name = p->name();
            String text = p->value();
            Serial.print("Param name: ");
            Serial.println(name);
            Serial.print("Param value: ");
            Serial.println(text);
            Serial.println("------");
            if (name == "set_output") {
                CbT fp = cmdMap[name];
                fp(text);
                raw_print(name.c_str());
                raw_print("set_output");
                Serial.println(
                        String("Is identical: ")
                        + name == "set_output" ? "Yes" : "No");
                // Serial.println(String("Dispatch pointer: ") + fp);
                Serial.println("Is Switch Command with value: " + text);
            }
        }
        request->send(SPIFFS, "/www/control.html");
    };

    // on("/update")
    // When update is initiated via GET
    static void onUpdateRequest(AsyncWebServerRequest *request) {
        reboot_requested = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", reboot_requested ? "OK": "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
    };
    // When file is uploaded via POST request
    static void onUpdateUpload(AsyncWebServerRequest *request, String filename,
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
    }; // on("/update")

    /* Catch-All-Handlers
    */
    static void onRequest(AsyncWebServerRequest *request) {
        //Handle Unknown Request
        request->send(404);
    };

    static void onBody(AsyncWebServerRequest *request,
            uint8_t *data, size_t len, size_t index, size_t total) {
        //Handle body
    };

    static void onUpload(AsyncWebServerRequest *request, String filename,
            size_t index, uint8_t *data, size_t len, bool final) {
        //Handle upload
    };
};


/* Handler for captive portal page, only active when in access point mode
*/
class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request) {
        //request->addInterestingHeader("ANY");
        return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("text/html");
        response->print("<!DOCTYPE html><html><head><title>Captive Portal</title></head><body>");
        response->print("<p>This is out captive portal front page.</p>");
        response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
        response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
        response->print("</body></html>");
        request->send(response);
    }
}; // class CaptiveRequestHandler