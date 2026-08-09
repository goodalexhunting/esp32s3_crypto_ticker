#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "config_mgr.h"

namespace cryptoapp {

/**
 * HTTP server for the ticker configuration web interface.
 *
 * Serves the configuration page and exposes a JSON API for
 * viewing/adding/removing/reordering tickers. The server only
 * manipulates the ConfigManager - it never touches display state.
 */
class ConfigServer {
   public:
    explicit ConfigServer(ConfigManager& config);

    /** Start the HTTP server. */
    void begin();

    /** Must be called from loop(). */
    void handle();

   private:
    ConfigManager& _config;
    WebServer      _server;

    // Handlers
    void handleRoot();
    void handleApiTickers();
    void handleApiAdd();
    void handleApiRemove();
    void handleApiMove();
    void handleApiReset();
    void handleNotFound();

    String buildTickersJson();
};

}  // namespace cryptoapp