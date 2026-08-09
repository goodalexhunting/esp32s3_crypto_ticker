#include "config_server.h"

#include <ArduinoJson.h>
#include <esp_task_wdt.h>

#include "app_config.h"
#include "embedded_ui.h"

namespace cryptoapp {

ConfigServer::ConfigServer(ConfigManager& config)
    : _config(config), _server(80), _queueMutex(xSemaphoreCreateMutex()) {
    // The /api/tickers routes share an URI but differ by HTTP method.
    _server.on("/", AsyncWebRequestMethod::HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });
    _server.on(CONFIG_PATH,
               AsyncWebRequestMethod::HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleConfigPage(request); });
    _server.on("/api/tickers",
               AsyncWebRequestMethod::HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleApiTickers(request); });
    _server.on("/api/tickers",
               AsyncWebRequestMethod::HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleApiAdd(request); });
    _server.on("/api/tickers",
               AsyncWebRequestMethod::HTTP_DELETE,
               [this](AsyncWebServerRequest* request) { handleApiRemove(request); });
    _server.on("/api/tickers/move",
               AsyncWebRequestMethod::HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleApiMove(request); });
    _server.on("/api/tickers/reset",
               AsyncWebRequestMethod::HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleApiReset(request); });
    _server.onNotFound([this](AsyncWebServerRequest* request) { handleNotFound(request); });
}

void ConfigServer::begin() {
    if (_queueMutex == nullptr) {
        Serial.println("[CFG] Mutex creation failed - config API disabled");
        return;
    }
    _server.begin();
    Serial.println("[CFG] Async config server started on port 80");
}

void ConfigServer::handle() {
    // Drain the operation queue on the loop task. ConfigManager is owned by
    // this task, so all mutations and the JSON reads happen here.
    ConfigOp op;
    while (dequeue(op)) {
        executeOp(op);
        // Keep the boot watchdog fed on slow WiFi/first-boot scenarios.
        esp_task_wdt_reset();
    }
}

// ---------------------------------------------------------------------------
// Operation queue
// ---------------------------------------------------------------------------

bool ConfigServer::enqueue(const ConfigOp& op) {
    if (_queueMutex == nullptr) {
        return false;
    }
    if (xSemaphoreTake(_queueMutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    size_t next = (_tail + 1) % OP_QUEUE_SIZE;
    bool   ok   = (next != _head);  // full when head == tail+1 (ring buffer)
    if (ok) {
        _queue[_tail] = op;
        _tail         = next;
    }
    xSemaphoreGive(_queueMutex);
    return ok;
}

bool ConfigServer::dequeue(ConfigOp& op) {
    if (_queueMutex == nullptr) {
        return false;
    }
    if (xSemaphoreTake(_queueMutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    bool ok = (_head != _tail);
    if (ok) {
        op    = _queue[_head];
        _head = (_head + 1) % OP_QUEUE_SIZE;
    }
    xSemaphoreGive(_queueMutex);
    return ok;
}

// ---------------------------------------------------------------------------
// JSON building (loop task only)
// ---------------------------------------------------------------------------

String ConfigServer::buildTickersJson() {
    JsonDocument doc;
    JsonArray    arr = doc.to<JsonArray>();

    for (size_t i = 0; i < _config.count(); i++) {
        const TickerConfig& t   = _config.get(i);
        JsonObject          obj = arr.add<JsonObject>();
        obj["label"]            = t.label;
        obj["apiId"]            = t.apiId;
        obj["quote"]            = t.quote;
        obj["color"]            = t.color;
    }

    String json;
    serializeJson(doc, json);
    return json;
}

// ---------------------------------------------------------------------------
// Async request handlers (AsyncTCP task) — only parse + enqueue
// ---------------------------------------------------------------------------

void ConfigServer::handleRoot(AsyncWebServerRequest* request) {
    // Route the user to the public GitHub Pages landing page, passing the
    // device's mDNS name so the landing page can link back to the config UI.
    String target = String(GITHUB_PAGES_URL) + "?device=" + MDNS_HOSTNAME + ".local";
    request->redirect(target);
}

void ConfigServer::handleConfigPage(AsyncWebServerRequest* request) {
    // Serve the embedded page straight from flash.
    request->send_P(200, "text/html", TICKER_CONFIG_HTML);
}

void ConfigServer::handleApiTickers(AsyncWebServerRequest* request) {
    ConfigOp op;
    op.type    = OpType::LIST;
    op.request = request;
    if (!enqueue(op)) {
        request->send(503, "text/plain", "Server busy");
    }
}

void ConfigServer::handleApiAdd(AsyncWebServerRequest* request) {
    if (!request->hasArg("label") || !request->hasArg("apiId") || !request->hasArg("quote")) {
        request->send(400, "text/plain", "Missing label, apiId or quote");
        return;
    }

    ConfigOp op;
    op.type    = OpType::ADD;
    op.request = request;
    op.label   = request->arg("label");
    op.apiId   = request->arg("apiId");
    op.quote   = request->arg("quote");
    op.color   = request->hasArg("color")
                     ? (uint16_t)strtoul(request->arg("color").c_str(), nullptr, 0)
                     : 0xFFFF;

    if (!enqueue(op)) {
        request->send(503, "text/plain", "Server busy");
    }
}

void ConfigServer::handleApiRemove(AsyncWebServerRequest* request) {
    if (!request->hasArg("id")) {
        request->send(400, "text/plain", "Missing id");
        return;
    }

    ConfigOp op;
    op.type    = OpType::REMOVE;
    op.request = request;
    op.id      = (size_t)request->arg("id").toInt();

    if (!enqueue(op)) {
        request->send(503, "text/plain", "Server busy");
    }
}

void ConfigServer::handleApiMove(AsyncWebServerRequest* request) {
    if (!request->hasArg("from") || !request->hasArg("to")) {
        request->send(400, "text/plain", "Missing from or to");
        return;
    }

    ConfigOp op;
    op.type    = OpType::MOVE;
    op.request = request;
    op.from    = (size_t)request->arg("from").toInt();
    op.to      = (size_t)request->arg("to").toInt();

    if (!enqueue(op)) {
        request->send(503, "text/plain", "Server busy");
    }
}

void ConfigServer::handleApiReset(AsyncWebServerRequest* request) {
    ConfigOp op;
    op.type    = OpType::RESET;
    op.request = request;
    if (!enqueue(op)) {
        request->send(503, "text/plain", "Server busy");
    }
}

void ConfigServer::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------------------
// Op execution (loop task) — the only place ConfigManager is touched
// ---------------------------------------------------------------------------

void ConfigServer::executeOp(ConfigOp& op) {
    AsyncWebServerRequest* request = op.request;
    if (request == nullptr) {
        return;
    }

    switch (op.type) {
        case OpType::LIST: {
            request->send(200, "application/json", buildTickersJson());
            break;
        }
        case OpType::ADD: {
            if (_config.add(op.label, op.apiId, op.quote, op.color)) {
                _config.save();
                request->send(200, "application/json", buildTickersJson());
            } else {
                request->send(400, "text/plain", "Failed to add ticker (duplicate or list full)");
            }
            break;
        }
        case OpType::REMOVE: {
            if (_config.remove(op.id)) {
                _config.save();
                request->send(200, "application/json", buildTickersJson());
            } else {
                request->send(400, "text/plain", "Invalid id");
            }
            break;
        }
        case OpType::MOVE: {
            if (_config.move(op.from, op.to)) {
                _config.save();
                request->send(200, "application/json", buildTickersJson());
            } else {
                request->send(400, "text/plain", "Invalid move indices");
            }
            break;
        }
        case OpType::RESET: {
            // resetToDefaults() saves internally.
            _config.resetToDefaults();
            request->send(200, "application/json", buildTickersJson());
            break;
        }
    }
}

}  // namespace cryptoapp