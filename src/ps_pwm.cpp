#include <map>
#include <functional>

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <FS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

#include "driver/mcpwm.h"
#include "ps_pwm.h"

#define GPIO_PWM0A_OUT 19 //Set GPIO 19 as PWM0A, first half-bridge
#define GPIO_PWM0B_OUT 18 //Set GPIO 18 as PWM0B, first half-bridge
#define GPIO_PWM1A_OUT 17 //Set GPIO 17 as PWM1A, second half-bridge
#define GPIO_PWM1B_OUT 16 //Set GPIO 16 as PWM1B, second half-bridge

#define SERIAL_BAUD 115200

const char *ap_ssid = "test_ssid";
const char *ap_psk = "123FOO456";
const char *hostName = "test_host";
//IPAddress local_IP(192,168,4,1);
//IPAddress gateway(192,168,4,254);
//IPAddress subnet(255,255,255,0);

static bool reboot_requested = false;

// DNSServer dnsServer;
// AsyncWebServer httpServer(80);
// Server-Sent Events (SSE) enable push updates on clients
// AsyncEventSource events("/events");

static void do_ps_pwm();
static void setup_wifi_hostap();
static void setup_wifi_sta_ap();
//void httpRegisterCallbacks();


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

HTTPServer httpServer(80);

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println();

    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPI Flash File System");
        return;
    }

    setup_wifi_hostap();

    MDNS.begin(hostName);
    MDNS.addService("http", "tcp", 80);

    // dnsServer.start(53, "*", WiFi.softAPIP());
    // attach AsyncEventSource
    // server.addHandler(&events);
    //httpServer.httpRegisterCallbacks();
    httpServer.begin();

    do_ps_pwm();
}

void loop() {
    // dnsServer.processNextRequest();
    delay(20);
    if (reboot_requested) {
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
    }
}

static void do_ps_pwm() {
    /* Example settings:
     *
     * Frequency 20 kHz,
     * phase-shift duty cycle of 45% per output or 90% of rectified waveform,
     * dead-time values as indicated.
     */
    INFO("Configuring Phase-Shift-PWM...");
    pspwm_setup_gpios(MCPWM_UNIT_0,
                      GPIO_PWM0A_OUT, GPIO_PWM0B_OUT,
                      GPIO_PWM1A_OUT, GPIO_PWM1B_OUT);

    pspwm_init_individual_deadtimes(MCPWM_UNIT_0,
                                    20e3,
                                    0.45,
                                    500e-9, 1000e-9, 4000e-9, 2000e-9);
}

/* Configure WiFi for Access Point Mode
 */
static void setup_wifi_sta_ap() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(hostName);
    WiFi.begin(ap_ssid, ap_psk);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("STA: Failed!\n");
        WiFi.disconnect(false);
        delay(1000);
        WiFi.begin(ap_ssid, ap_psk);
    }
}

/* Configure WiFi for combined Station and Access Point Mode
 */
static void setup_wifi_hostap() {
    //Serial.print("Setting soft-AP configuration ... ");
    //Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
    WiFi.softAP(ap_ssid, ap_psk);
    //WiFi.softAPsetHostname(hostName);
    Serial.print(
        "Access point running. IP address: " + WiFi.softAPIP().toString());
    Serial.println("");
}

