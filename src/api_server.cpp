/* AJAX HTTP API server for ESP32
 *
 * Based on ESPAsyncWebServer, see:
 * https://github.com/me-no-dev/ESPAsyncWebServer
 * 
 * License: GPL v.3 
 * U. Lukas 2020-11-18
 */
#include <Update.h>
#include <SPIFFS.h>
#include <FS.h>

// Activate incomplete features..
//#define WORK_IN_PROGRESS__

#include "api_server.hpp"
#include "api_server_config.hpp"
#include "http_content.hpp"

// Local debug level
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static const char* TAG = "api_server.cpp";

//////// APIServer:: public API

APIServer::APIServer(AsyncWebServer* http_backend)
    : backend{http_backend}
    , event_source{nullptr}
    , _reboot_requested{false}
    , _event_timer{}
    , _heartbeat_cb{}
{   
    if (conf_serve_static_from_spiffs) {
        if (!SPIFFS.begin(true)) {
            ESP_LOGE(TAG, "Error mounting SPI Flash File System!");
            abort();
        }
    }
    _add_rewrites();
    _add_redirects();
    _add_handlers();
    if (conf_use_sse) {
        _add_event_source();
    }
    // FIXME: This is even called when no heartbeats are sent, we use the timer
    // for reboots etc, this is a temporary solution..
    _event_timer.attach_ms(conf_heartbeat_interval, _on_timer_event, this);
}

APIServer::~APIServer() {
    _event_timer.detach();
    delete event_source;
}

// Set an entry in the template processor string <=> string mapping 
void APIServer::set_template(const char* placeholder, const char* replacement) {
    if (!conf_template_processing_activated) {
        ESP_LOGE(TAG, "ERROR: template processing must be activated!");
        return;
    };
    template_map[String(placeholder)] = String(replacement);
}


void APIServer::register_api_cb(const char* cmd_name, CbStringT cmd_callback) {
    cmd_map[cmd_name] = cmd_callback;
    ESP_LOGD(TAG, "Registered String command: %s", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name, CbFloatT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        // Arduino String.toFloat() defaults to zero for invalid string, hmm...
        cmd_callback(value.toFloat());
        };
    ESP_LOGD(TAG, "Registered float command: %s", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name, CbIntT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        // Arduino String.toFloat() defaults to zero for invalid string, hmm...
        cmd_callback(value.toInt());
        };
    ESP_LOGD(TAG, "Registered int command: %s", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name, CbVoidT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        cmd_callback();
        };
    ESP_LOGD(TAG, "Registered void command: %s", cmd_name);
}

// Do not call this if the server is already running!
void APIServer::begin() {
    backend->begin();
}


////// Implementation

// Add Request URL rewrites to the server instance
void APIServer::_add_rewrites() {
   //auto rewrite = new AsyncWebRewrite("/foo.html", "/bar.html");
   //backend->addRewrite(rewrite);
}

// Add Request URL rewrites to the server instance
void APIServer::_add_redirects() {
    backend->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect(conf_index_html_file);
        });
}

// Add request handlers to the server instance
void APIServer::_add_handlers() {
    // Route for REST API
    backend->on(conf_api_endpoint, HTTP_GET, [this](AsyncWebServerRequest *request) {
            _on_cmd_request(request);
        });
    // OTA Firmware Upgrade, see form method in data/www/upload.html
    backend->on("/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
            _on_update_request(request);
        },
        _on_update_body_upload
        );

    // Serve static HTML and related files content
    if (conf_serve_static_from_spiffs) {
        auto handler = &backend->serveStatic(
            conf_static_route, SPIFFS, conf_spiffs_static_files_folder);
        //handler->setDefaultFile(conf_index_html_file);
        if (conf_template_processing_activated) {
            handler->setTemplateProcessor(
                [this](const String &placeholder) {
                    return _template_processor(placeholder);
                }
            );
        } else {
            // When no template processing is used, this is assumed to be all
            // static files which do not change and can be cached indefinitely
            handler->setCacheControl(conf_cache_control);
        }
        if (conf_http_auth_activated) {
            handler->setAuthentication(conf_http_user, conf_http_pass);
        }
    } else {
        // Route for main application home page
        backend->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
                _on_root_request(request);
            });
    }

    backend->onNotFound([](AsyncWebServerRequest *request){
            request->send_P(404, "text/html", conf_error_404_html);
            ESP_LOGD(TAG, "%s", conf_error_404_html);
        });
    backend->onFileUpload(_on_upload);
    backend->onRequestBody(_on_body);
    // Handler called when any DNS query is made via access point
    // addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    ESP_LOGD(TAG, "Default callbacks set up");
}


// Activate the SSE envent source if conf_use_sse == true
void APIServer::_add_event_source() {
    event_source = new AsyncEventSource(conf_sse_endpoint);
    if (event_source) {
        _register_sse_on_connect_callback();
        // HTTP Basic Authentication
        //if (USE_AUTH) {
        //    event_source.setAuthentication(http_user, http_pass);
        //}
        backend->addHandler(event_source);
    } else {
        ESP_LOGE(TAG, "Event Source could not be initialised!");
        abort();
    }
}

// Sends "Hello" message when a client connects to the Server-Sent Event Source
void APIServer::_register_sse_on_connect_callback() {
    event_source->onConnect([](AsyncEventSourceClient *client) {
        if(client->lastId()){
            ESP_LOGI(TAG, "Client connected! Last msg ID: %d", client->lastId());
        }
        // Send confirmation message via SSE source when connection has been
        // established, ID is current millis. Set reconnect delay to 1 second.
        client->send("Hello Message from ESP32!", NULL, millis(), 1000);
    });
}


/////// Backend callback implementation

// on("/")
void APIServer::_on_root_request(AsyncWebServerRequest *request) {
    if (!conf_serve_static_from_spiffs) {
        // Static content is handled by default handler for static content
        if (conf_template_processing_activated) {
            request->send_P(200, "text/html", index_html,
                            [this](const String& placeholder) {
                                return _template_processor(placeholder);
                            }
            );
        } else {
            request->send_P(200, "text/html", index_html);
        }
    }
}

// on("/cmd")
void APIServer::_on_cmd_request(AsyncWebServerRequest *request) {
    int n_params = request->params();
    ESP_LOGD(TAG, "Number of parameters received: %d", n_params);
    for (int i = 0; i < n_params; ++i) {
        AsyncWebParameter *p = request->getParam(i);
        const String &name = p->name();
        const String &value_str = p->value();
        ESP_LOGD(TAG, "-----\nParam name: %s", name.c_str());
        ESP_LOGD(TAG, "Param value: %s", value_str.c_str());
        auto cb_iterator = cmd_map.find(name);
        if (cb_iterator == cmd_map.end()) {
            ESP_LOGE(TAG, "Error: Not registered in command mapping: %s", name.c_str());
            continue;
        }
        CbStringT cmd_callback = (*cb_iterator).second; // = cmd_map[name];
        if (!cmd_callback) {
            ESP_LOGE(TAG, "Error: Not a callable object!: %s", name.c_str());
            continue;
        }
        // Finally call callback
        cmd_callback(value_str);
    }
    if (conf_api_is_ajax) {
        // For AJAX interface: Return a plain string, default is empty string.
        request->send(200, "text/plain", conf_ajax_return_text);
    } else if (!conf_serve_static_from_spiffs) {
        // Static content is handled by default handler for static content
        if (conf_template_processing_activated) {
            request->send_P(200, "text/html", api_return_html,
                            [this](const String& placeholder) {
                                return _template_processor(placeholder);
                            }
            );
        } else {
            request->send_P(200, "text/html", api_return_html);
        }
    }
}

// on("/update")
// When update is initiated via GET
void APIServer::_on_update_request(AsyncWebServerRequest *request) {
    bool update_ok = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/plain", update_ok ? "OK" : "Update FAIL!");
    response->addHeader("Connection", "close");
    request->send(response);
    if (update_ok) {
        // If update is OK, we can reboot. If not, this is not a good idea..
        _reboot_requested = true;
    }
}
// When file is uploaded via POST request
void APIServer::_on_update_body_upload(
        AsyncWebServerRequest *request, const String& filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    if(!index) {
        ESP_LOGI(TAG, "Update Start: %s", filename.c_str());
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
            ESP_LOGI(TAG, "Update Success: %d", index+len);
        }
        else {
            Update.printError(Serial);
        }
    }
} // on("/update")

/* Catch-All-Handlers
 */
void APIServer::_on_request(AsyncWebServerRequest *request) {
    //Handle Unknown Request
    request->send(404);
}

void APIServer::_on_body(AsyncWebServerRequest *request,
        uint8_t *data, size_t len, size_t index, size_t total) {
    //Handle body
}

void APIServer::_on_upload(AsyncWebServerRequest *request, const String& filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    //Handle upload
}

////// Timer callback

// Timer update for heartbeats, reboot etc
// Static function wraps member function to obtain C API callback
void APIServer::_on_timer_event(APIServer* self) {
    // If a callback function object is registered for the heartebat timer
    // event, this is called first.
    if (conf_sending_heartbeats && self->event_source != nullptr) {
        self->event_source->send(conf_heartbeat_message, "heartbeat");
    }
    if (self->_reboot_requested) {
        ESP_LOGD(TAG, "Rebooting...");
        delay(100);
        ESP.restart();
    }
}

////// Special functions

// Template processor
String APIServer::_template_processor(const String& placeholder)
{
    auto template_iterator = template_map.find(placeholder);
        if (template_iterator == template_map.end()) {
            ESP_LOGE(TAG, "Error: Entry not registered in template mapping: %s",
                     placeholder.c_str());
            return placeholder;
        } else {
            return template_map[placeholder];
        }
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
