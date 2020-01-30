/* HTTP server
 *
 * Based on ESPAsyncWebServer, see:
 * https://github.com/me-no-dev/ESPAsyncWebServer
 */
#include <Update.h>
#include <SPIFFS.h>
#include <FS.h>

// Activate incomplete features..
//#define WORK_IN_PROGRESS__

#include "info_debug_error.h"
#include "api_server.hpp"
#include "api_server_config.hpp"
#include "http_content.hpp"

///////////// APIServer:: public

APIServer::APIServer(AsyncWebServer* http_backend)
    // public
    : backend{http_backend}
    , event_source{nullptr}
    , reboot_requested{false}
    , event_timer{}
{   
    if (mount_spiffs_requested) {
        if (!SPIFFS.begin(true)) {
            error_print("Error mounting SPI Flash File System");
        }
    }
    event_timer.attach_ms(heartbeat_interval, on_timer_event, this);
}

APIServer::~APIServer() {
    event_timer.detach();
    free(event_source);
}

// Set an entry in the template processor string <=> string mapping 
void APIServer::set_template(const char* placeholder, const char* replacement) {
    if (!template_processing_activated) {
        error_print("ERROR: template processing must be activated!");
        return;
    };
    template_map[String(placeholder)] = String(replacement);
}

void APIServer::activate_events_on(const char* endpoint) {
    event_source = new AsyncEventSource(endpoint);
    if (event_source) {
        register_sse_callbacks();
    } else {
        error_print("Event Source could not be initialised!");
    }
}

void APIServer::register_api_cb(const char* cmd_name,
                                 CbStringT cmd_callback) {
    cmd_map[cmd_name] = cmd_callback;
    debug_print_sv("Registered String command: ", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name,
                                 CbFloatT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        // Arduino String.toFloat() defaults to zero for invalid string, hmm...
        cmd_callback(value.toFloat());
    };
    debug_print_sv("Registered float command: ", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name,
                                 CbIntT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        // Arduino String.toFloat() defaults to zero for invalid string, hmm...
        cmd_callback(value.toInt());
    };
    debug_print_sv("Registered int command: ", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name,
                                 CbVoidT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        cmd_callback();
    };
    debug_print_sv("Registered void command:", cmd_name);
}

void APIServer::begin() {
    activate_default_callbacks();
    backend->begin();
}

// Normal HTTP request handlers
void APIServer::activate_default_callbacks() {
    // Serve static HTML and related files content
    if (mount_spiffs_requested) {
        auto handler = backend->serveStatic("/", SPIFFS, "/www/")
                              .setDefaultFile(index_html_filename);
        if (template_processing_activated) {
            handler = handler.setTemplateProcessor(
                [this](const String &placeholder) {
                    return templateProcessor(placeholder);
                }
            );
        }
        if (http_auth_requested) {
            handler.setAuthentication(http_user, http_pass);
        }
    } else {
        // Route for main application home page
        backend->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
                onRootRequest(request);
            }
        );
    }
    // Route for REST API
    //using namespace std::placeholders;
    //on("/cmd", HTTP_GET, std::bind(&APIServer::onCmdRequest, this, _1));
    backend->on(api_endpoint, HTTP_GET, [this](AsyncWebServerRequest *request) {
            onCmdRequest(request);
        }
    );
    // respond to GET requests on URL /heap
    backend->on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", String(ESP.getFreeHeap()));
       }
    );
    // OTA Firmware Upgrade, see form method in data/www/upload.html
    backend->on("/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
            onUpdateRequest(request);
       },
       onUpdateUploadBody
    );
    backend->onNotFound(onRequest);
    backend->onFileUpload(onUpload);
    backend->onRequestBody(onBody);
    // Handler called when any DNS query is made via access point
    // addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    debug_print("Default callbacks set up");
}


///////// APIServer:: private

// Timer update for heartbeats, reboot etc
// Static function wraps member function to obtain C API callback
void APIServer::on_timer_event(APIServer* self) {
    if (sending_heartbeats && self->event_source != nullptr) {
        self->event_source->send("OK", "heartbeat");
    }
    if (self->reboot_requested) {
        debug_print("Rebooting...");
        delay(100);
        ESP.restart();
    }
}

// Sever-Sent Event Source
void APIServer::register_sse_callbacks() {
    event_source->onConnect([](AsyncEventSourceClient *client) {
        if(client->lastId()){
            info_print_sv("Client connected! Last msg ID:", client->lastId());
        }
        //send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("Hello Message from ESP32!", NULL, millis(), 1000);
    });
    // HTTP Basic Authentication
    //if (USE_AUTH) {
    //    event_source.setAuthentication(http_user, http_pass);
    //}
    backend->addHandler(event_source);
}

// Template processor
String APIServer::templateProcessor(const String& placeholder)
{
    auto template_iterator = template_map.find(placeholder);
        if (template_iterator == template_map.end()) {
            error_print_sv("Error: Entry not registered in template mapping:",
                           placeholder);
            return placeholder;
        } else {
            return template_map[placeholder];
        }
}

// on("/")
void APIServer::onRootRequest(AsyncWebServerRequest *request) {
    if (!mount_spiffs_requested) {
        // Static content is handled by default handler for static content
        if (template_processing_activated) {
            request->send_P(200, "text/html", index_html,
                            [this](const String& placeholder) {
                                return templateProcessor(placeholder);
                            }
            );
        } else {
            request->send_P(200, "text/html", index_html);
        }
    }
}

// on("/cmd")
void APIServer::onCmdRequest(AsyncWebServerRequest *request) {
    int n_params = request->params();
    debug_print_sv("Number of parameters received:", n_params);
    for (int i = 0; i < n_params; ++i) {
        AsyncWebParameter *p = request->getParam(i);
        const String name = p->name();
        const String value_str = p->value();
        debug_print_sv("-----\nParam name:", name);
        debug_print_sv("Param value:", value_str);
        auto cb_iterator = cmd_map.find(name);
        if (cb_iterator == cmd_map.end()) {
            error_print_sv("Error: Not registered in command mapping:", name);
            continue;
        }
        CbStringT cmd_callback = (*cb_iterator).second; // = cmd_map[name];
        if (!cmd_callback) {
            error_print_sv("Error: Not a callable object!", name);
            continue;
        }
        // Finally call callback
        cmd_callback(value_str);
    }
    if (api_is_ajax) {
        // For AJAX interface: Return a plain string, default is empty string.
        request->send(200, "text/plain", ajax_return_text);
    } else if (!mount_spiffs_requested) {
        // Static content is handled by default handler for static content
        if (template_processing_activated) {
            request->send_P(200, "text/html", api_return_html,
                            [this](const String& placeholder) {
                                return templateProcessor(placeholder);
                            }
            );
        } else {
            request->send_P(200, "text/html", api_return_html);
        }
    }
}

// on("/update")
// When update is initiated via GET
void APIServer::onUpdateRequest(AsyncWebServerRequest *request) {
    reboot_requested = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/plain", reboot_requested ? "OK" : "FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
}
// When file is uploaded via POST request
void APIServer::onUpdateUploadBody(
        AsyncWebServerRequest *request, const String& filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    if(!index) {
        info_print_sv("Update Start:", filename);
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
            info_print_sv("Update Success:", index+len);
        }
        else {
            Update.printError(Serial);
        }
    }
} // on("/update")

/* Catch-All-Handlers
 */
void APIServer::onRequest(AsyncWebServerRequest *request) {
    //Handle Unknown Request
    request->send(404);
}

void APIServer::onBody(AsyncWebServerRequest *request,
        uint8_t *data, size_t len, size_t index, size_t total) {
    //Handle body
}

void APIServer::onUpload(AsyncWebServerRequest *request, const String& filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    //Handle upload
}

#ifdef WORK_IN_PROGRESS__
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
            request->host(), request->url(),
            WiFi.softAPIP().toString()
    );
    request->send(response);
}
#endif