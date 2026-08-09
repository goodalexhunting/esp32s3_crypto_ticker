#include "config_server.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace cryptoapp {

ConfigServer::ConfigServer(ConfigManager& config) : _config(config), _server(80) {
    _server.on("/", HTTP_GET, std::bind(&ConfigServer::handleRoot, this));
    _server.on("/api/tickers", HTTP_GET, std::bind(&ConfigServer::handleApiTickers, this));
    _server.on("/api/tickers", HTTP_POST, std::bind(&ConfigServer::handleApiAdd, this));
    _server.on("/api/tickers", HTTP_DELETE, std::bind(&ConfigServer::handleApiRemove, this));
    _server.on("/api/tickers/move", HTTP_POST, std::bind(&ConfigServer::handleApiMove, this));
    _server.on("/api/tickers/reset", HTTP_POST, std::bind(&ConfigServer::handleApiReset, this));
    _server.onNotFound(std::bind(&ConfigServer::handleNotFound, this));
}

void ConfigServer::begin() {
    _server.begin();
    Serial.println("[CFG] Config server started on port 80");
}

void ConfigServer::handle() {
    _server.handleClient();
}

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

void ConfigServer::handleRoot() {
    File file = LittleFS.open("/ticker_config.html", "r");
    if (!file) {
        _server.send(500, "text/plain", "Config page not found");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.streamFile(file, "text/html");
    file.close();
}

void ConfigServer::handleApiTickers() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", buildTickersJson());
}

void ConfigServer::handleApiAdd() {
    if (!_server.hasArg("label") || !_server.hasArg("apiId") || !_server.hasArg("quote")) {
        _server.send(400, "text/plain", "Missing label, apiId or quote");
        return;
    }

    String   label = _server.arg("label");
    String   apiId = _server.arg("apiId");
    String   quote = _server.arg("quote");
    uint16_t color = _server.hasArg("color")
                         ? (uint16_t)strtoul(_server.arg("color").c_str(), nullptr, 0)
                         : 0xFFFF;

    if (_config.add(label, apiId, quote, color)) {
        _config.save();
        _server.sendHeader("Cache-Control", "no-store");
        _server.send(200, "application/json", buildTickersJson());
    } else {
        _server.send(400, "text/plain", "Failed to add ticker (duplicate or list full)");
    }
}

void ConfigServer::handleApiRemove() {
    if (!_server.hasArg("id")) {
        _server.send(400, "text/plain", "Missing id");
        return;
    }

    size_t id = (size_t)_server.arg("id").toInt();
    if (_config.remove(id)) {
        _config.save();
        _server.sendHeader("Cache-Control", "no-store");
        _server.send(200, "application/json", buildTickersJson());
    } else {
        _server.send(400, "text/plain", "Invalid id");
    }
}

void ConfigServer::handleApiMove() {
    if (!_server.hasArg("from") || !_server.hasArg("to")) {
        _server.send(400, "text/plain", "Missing from or to");
        return;
    }

    size_t from = (size_t)_server.arg("from").toInt();
    size_t to   = (size_t)_server.arg("to").toInt();

    if (_config.move(from, to)) {
        _config.save();
        _server.sendHeader("Cache-Control", "no-store");
        _server.send(200, "application/json", buildTickersJson());
    } else {
        _server.send(400, "text/plain", "Invalid move indices");
    }
}

void ConfigServer::handleApiReset() {
    _config.resetToDefaults();
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", buildTickersJson());
}

void ConfigServer::handleNotFound() {
    _server.send(404, "text/plain", "Not found");
}

}  // namespace cryptoapp