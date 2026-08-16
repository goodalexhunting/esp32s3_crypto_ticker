#pragma once

// Mock HTTPClient sufficient to compile src/crypto.cpp on the host.
// The real network path (httpGetJson) is never exercised by the host
// integration tests — they inject a CryptoHttp fake — but the symbols
// must link.

#include <ArduinoJson.h>

#include "Arduino.h"

#define HTTP_CODE_OK 200

class HTTPClient {
   public:
    void begin(const String&) {}
    void setConnectTimeout(int) {}
    void setTimeout(int) {}
    void addHeader(const char*, const char*) {}
    void setFollowRedirects(int) {}
    int  GET() {
        return _code;
    }
    void   end() {}
    String getString() {
        return _body;
    }

    int    _code = 0;
    String _body;
};