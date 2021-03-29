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
#include "esp_spiffs.h"

#include "api_server.hpp"
#include "http_content.hpp"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
static const char* TAG = "APIServer";

//////// APIServer:: public API

APIServer::APIServer(AsyncWebServer* http_backend)
    : backend{http_backend}
    , event_source{nullptr}
{}

APIServer::~APIServer() {
    delete event_source;
}

/** Begin operation.
 * This must be called when SPIFFS and Arduino environment is available.
 */
void APIServer::begin() {
    if (srv_conf.serve_static_from_spiffs) {
        ESP_LOGI(TAG, "Mounting SPI Flash File System...");
        if (!SPIFFS.begin(false)) {
            ESP_LOGE(TAG, "Error mounting SPI Flash File System!");
            abort();
        }
        size_t tot_bytes, used_bytes;
        esp_spiffs_info(NULL, &tot_bytes, &used_bytes);
        ESP_LOGI(TAG, "SPIFFS filesystem size in bytes: %d   Used bytes: %d",
                 tot_bytes, used_bytes);
    }
    backend->begin();
    _add_rewrites();
    _add_redirects();
    _add_handlers();
    if (srv_conf.use_sse) {
        _add_event_source();
    }
}

// Set an entry in the template processor string <=> string mapping 
void APIServer::set_template(const char* placeholder, const char* replacement) {
    if (!srv_conf.template_processing_activated) {
        ESP_LOGE(TAG, "ERROR: template processing must be activated!");
        return;
    };
    template_map[String(placeholder)] = String(replacement);
}


void APIServer::register_api_cb(const char* cmd_name, CbStringT cmd_callback) {
    cmd_map[cmd_name] = cmd_callback;
    ESP_LOGI(TAG, "Registered String command: %s", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name, CbFloatT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        // Arduino String.toFloat() defaults to zero for invalid string, hmm...
        cmd_callback(value.toFloat());
        };
    ESP_LOGI(TAG, "Registered float command: %s", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name, CbIntT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        // Arduino String.toFloat() defaults to zero for invalid string, hmm...
        cmd_callback(value.toInt());
        };
    ESP_LOGI(TAG, "Registered int command: %s", cmd_name);
}

void APIServer::register_api_cb(const char* cmd_name, CbVoidT cmd_callback) {
    cmd_map[cmd_name] = [cmd_callback](const String& value) {
        cmd_callback();
        };
    ESP_LOGI(TAG, "Registered void command: %s", cmd_name);
}


////// Implementation

// Add Request URL rewrites to the server instance
void APIServer::_add_rewrites() {
   //auto rewrite = new AsyncWebRewrite("/app*", srv_conf.index_html_file);
   //backend->addRewrite(rewrite);
}

// Add Request URL rewrites to the server instance
void APIServer::_add_redirects() {
    backend->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect(srv_conf.index_html_file);
        });
    backend->on(srv_conf.app_route, HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect(srv_conf.index_html_file);
        });
}

// Add request handlers to the server instance
void APIServer::_add_handlers() {
    // Route for REST API
    backend->on(srv_conf.api_endpoint, HTTP_GET, [this](AsyncWebServerRequest *request) {
            _on_cmd_request(request);
        });
    // OTA Firmware Upgrade, see form method in data/www/upload.html
    backend->on("/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
            _on_update_request(request);
        },
        _on_update_body_upload
        );

    // Serve static HTML and related files content
    if (srv_conf.serve_static_from_spiffs) {
        auto handler = &backend->serveStatic(srv_conf.static_route,
                                             SPIFFS,
                                             srv_conf.spiffs_static_files_folder,
                                             srv_conf.cache_control);
        //handler->setDefaultFile(srv_conf.index_html_file);
        if (srv_conf.template_processing_activated) {
            handler->setTemplateProcessor(
                [this](const String &placeholder) {
                    return _template_processor(placeholder);
                }
            );
        }
        if (srv_conf.http_auth_activated) {
            handler->setAuthentication(srv_conf.http_user, srv_conf.http_pass);
        }
    } else {
        // Route for main application home page
        backend->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
                _on_root_request(request);
            });
    }

    backend->onNotFound([](AsyncWebServerRequest *request){
            request->send_P(404, "text/html", srv_conf.error_404_html);
            ESP_LOGE(TAG, "%s\n Request URL: %s",
                     srv_conf.error_404_html, request->url().c_str());
        });
    backend->onFileUpload(_on_upload);
    backend->onRequestBody(_on_body);
    // Handler called when any DNS query is made via access point
    // addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    ESP_LOGI(TAG, "Default callbacks set up");
}


// Activate the SSE envent source if srv_conf.use_sse == true
void APIServer::_add_event_source() {
    event_source = new AsyncEventSource(srv_conf.sse_endpoint);
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
    if (!srv_conf.serve_static_from_spiffs) {
        // Static content is handled by default handler for static content
        if (srv_conf.template_processing_activated) {
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
    auto n_params = request->params();
    ESP_LOGD(TAG, "Number of parameters received: %d", n_params);
    for (size_t i = 0; i < n_params; ++i) {
        auto p = request->getParam(i);
        const auto &name = p->name();
        const auto &value_str = p->value();
        ESP_LOGD(TAG, "-->Param name: %s  with value: %s", name.c_str(), value_str.c_str());
        auto cb_iterator = cmd_map.find(name);
        if (cb_iterator == cmd_map.end()) {
            ESP_LOGE(TAG, "Error: Not registered in command mapping: %s", name.c_str());
            continue;
        }
        auto cmd_callback = (*cb_iterator).second; // = cmd_map[name];
        if (!cmd_callback) {
            ESP_LOGE(TAG, "Error: Not a callable object!: %s", name.c_str());
            continue;
        }
        // Finally call callback
        cmd_callback(value_str);
    }
    if (srv_conf.api_is_ajax) {
        // For AJAX interface: Return a plain string, default is empty string.
        request->send(200, "text/plain", srv_conf.ajax_return_text);
    } else if (!srv_conf.serve_static_from_spiffs) {
        // Static content is handled by default handler for static content
        if (srv_conf.template_processing_activated) {
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
    auto update_ok = !Update.hasError();
    auto response = request->beginResponse(
        200, "text/plain", update_ok ? "OK" : "Update FAIL!");
    response->addHeader("Connection", "close");
    request->send(response);
    if (update_ok) {
        // If update is OK, we can reboot. If not, this is not a good idea..
        reboot_requested = true;
        if (srv_conf.reboot_enabled) {
            ESP.restart();
        }
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


////////
// HTTP response string template processor
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