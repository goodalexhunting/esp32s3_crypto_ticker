#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config_mgr.h"

namespace cryptoapp {

/**
 * Asynchronous HTTP server for the ticker configuration web interface.
 *
 * Serves the configuration page (a PROGMEM string, see embedded_ui.h),
 * exposes a JSON API for viewing/adding/removing/reordering/resetting
 * tickers, and redirects "/" to the GitHub Pages landing page.
 *
 * AsyncWebServer handlers run on the AsyncTCP task. The ConfigManager is
 * owned by the main loop task (display code reads it from loop()), so
 * handlers never touch ConfigManager directly: they parse their arguments
 * and enqueue a ConfigOp. The queue is drained from loop() via handle(),
 * which applies the op on the loop task, saves, builds the JSON response,
 * and sends it back on the request. This keeps ConfigManager strictly
 * single-threaded (the loop task) and removes the network processing
 * from the critical display loop path entirely.
 */
class ConfigServer {
   public:
    explicit ConfigServer(ConfigManager& config);

    /** Start the HTTP server. */
    void begin();

    /**
     * Must be called from loop(). Drains the operation queue: applies
     * pending config changes on the loop task and sends their responses.
     */
    void handle();

   private:
    enum class OpType : uint8_t { LIST, ADD, REMOVE, MOVE, RESET };

    struct ConfigOp {
        OpType                 type;
        AsyncWebServerRequest* request = nullptr;
        String                 label;
        String                 apiId;
        String                 quote;
        uint16_t               color = 0xFFFF;
        size_t                 id    = 0;
        size_t                 from  = 0;
        size_t                 to    = 0;
    };

    static constexpr size_t OP_QUEUE_SIZE = 8;

    ConfigManager&    _config;
    AsyncWebServer    _server;
    ConfigOp          _queue[OP_QUEUE_SIZE];
    SemaphoreHandle_t _queueMutex;
    size_t            _head = 0;
    size_t            _tail = 0;

    bool enqueue(const ConfigOp& op);
    bool dequeue(ConfigOp& op);

    // Async request handlers (run on the AsyncTCP task).
    void handleRoot(AsyncWebServerRequest* request);
    void handleConfigPage(AsyncWebServerRequest* request);
    void handleApiTickers(AsyncWebServerRequest* request);
    void handleApiAdd(AsyncWebServerRequest* request);
    void handleApiRemove(AsyncWebServerRequest* request);
    void handleApiMove(AsyncWebServerRequest* request);
    void handleApiReset(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);

    // Handler for a single op once drained onto the loop task.
    void executeOp(ConfigOp& op);

    String buildTickersJson();
};

}  // namespace cryptoapp