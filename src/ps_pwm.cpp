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
const char *ap_psk = "123456";
const char *hostName = "test_host";
//IPAddress local_IP(192,168,4,1);
//IPAddress gateway(192,168,4,254);
//IPAddress subnet(255,255,255,0);

static bool reboot_requested = false;

// DNSServer dnsServer;
AsyncWebServer httpServer(80);
// Server-Sent Events (SSE) enable push updates on clients
// AsyncEventSource events("/events");

static void do_ps_pwm();
static void setup_wifi_hostap();
static void setup_wifi_sta_ap();
void httpRegisterCallbacks();

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

    httpRegisterCallbacks();
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

/* Request handlers for HTTP server
 */
void httpRegisterCallbacks() {
    /* Normal HTTP request handlers
     */
    // httpServer.on("/" ...
    ArRequestHandlerFunction onRootRequest = [](AsyncWebServerRequest *request) {
        //request->send(SPIFFS, "/control.html", String(), false, processor);
        request->send(SPIFFS, "/www/control.html");
    };

    // httpServer.on("/cmd" ...
    ArRequestHandlerFunction onCmdRequest = [](AsyncWebServerRequest *request) {
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
                Serial.println("Is Switch Command with value: " + text);
            }
        }
    };

    // httpServer.on("/update" ...
    ArRequestHandlerFunction onUpdateRequest = [](AsyncWebServerRequest *request) {
        reboot_requested = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", reboot_requested ? "OK": "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
    };
    ArUploadHandlerFunction onUpdateUpload = [](AsyncWebServerRequest *request,
                                                String filename, size_t index,
                                                uint8_t *data, size_t len, bool final) {
        if(!index) {
            Serial.printf("Update Start: %s\n", filename.c_str());
            //Update.runAsync(true);
            if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
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
    }; // httpServer.on("/update" ...

    /* Catch-All-Handlers
     */
    ArRequestHandlerFunction onRequest = [](AsyncWebServerRequest *request) {
        //Handle Unknown Request
        request->send(404);
    };

    ArBodyHandlerFunction onBody = [](AsyncWebServerRequest *request,
            uint8_t *data, size_t len, size_t index, size_t total) {
        //Handle body
    };

    ArUploadHandlerFunction onUpload = [](AsyncWebServerRequest *request,
            String filename, size_t index, uint8_t *data, size_t len, bool final) {
        //Handle upload
    };

    /* Request handler for captive portal page is only active when in access point mode
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


    /* Register handlers
     */
    // Route for main application home page
    httpServer.on("/", HTTP_GET, onRootRequest);

    // Serve static HTML and related files content
    httpServer.serveStatic("/", SPIFFS, "/www/");

    // Route for REST API
    httpServer.on("/cmd", HTTP_GET, onCmdRequest);

    // respond to GET requests on URL /heap
    httpServer.on("/heap", HTTP_GET,
                  [](AsyncWebServerRequest *request) {
                      request->send(200, "text/plain", String(ESP.getFreeHeap()));
                      }
                  );

    // OTA Firmware Upgrade via file input, see form method in data/www/upload.html
    httpServer.on("/update", HTTP_POST, onUpdateRequest, onUpdateUpload);

    httpServer.onNotFound(onRequest);
    httpServer.onFileUpload(onUpload);
    httpServer.onRequestBody(onBody);

    // Handler called when any DNS query is made via access point
    // httpServer.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);

} // httpRegisterCallbacks()
