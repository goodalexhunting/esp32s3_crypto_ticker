#pragma once

// Minimal Arduino environment mock for host-side (native) tests.
// Provides the subset of the Arduino API used by the modules under test:
//   - String with the operations used by wifi_mgr/config_mgr/crypto/ota
//   - millis()/delay() driven by a test-controllable clock
//   - a no-op Serial sink
//   - IPAddress, ESP.getEfuseMac(), PROGMEM/HEX/DEC macros
//
// This header is compiled only in the test environment ([env:native]); the
// firmware never sees it.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define PROGMEM
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

typedef uint8_t byte;

// ---------------------------------------------------------------------------
// String
// ---------------------------------------------------------------------------
class String {
   public:
    String() = default;
    String(const char* s) {
        if (s != nullptr) _s = s;
    }
    String(char c) {
        _s = c;
    }
    String(const unsigned char* s, unsigned int len) {
        if (s != nullptr && len > 0) _s.assign(reinterpret_cast<const char*>(s), len);
    }
    String(const char* s, unsigned int len) {
        if (s != nullptr && len > 0) _s.assign(s, len);
    }
    String(const String& o) : _s(o._s) {}
    String(int v) {
        _s = std::to_string(v);
    }
    String(unsigned int v) {
        _s = std::to_string(v);
    }
    String(long v) {
        _s = std::to_string(v);
    }
    String(unsigned long v) {
        _s = std::to_string(v);
    }
    String(long long v) {
        _s = std::to_string(v);
    }
    String(unsigned long long v) {
        _s = std::to_string(v);
    }
    String(float v, unsigned char = 2) {
        _s = std::to_string(v);
    }
    String(double v, unsigned char = 2) {
        _s = std::to_string(v);
    }
    String(uint32_t v, uint8_t base) {
        char buf[40];
        if (base == HEX) {
            snprintf(buf, sizeof(buf), "%lX", (unsigned long)v);
        } else if (base == OCT) {
            snprintf(buf, sizeof(buf), "%lo", (unsigned long)v);
        } else if (base == BIN) {
            buf[0] = '\0';
            if (v == 0) {
                _s = "0";
                return;
            }
            for (int i = 31; i >= 0; i--) {
                if (v & (1u << i))
                    _s += '1';
                else if (!_s.empty())
                    _s += '0';
            }
            return;
        } else {
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);
        }
        _s = buf;
    }

    String& operator=(const String& o) {
        _s = o._s;
        return *this;
    }
    String& operator=(const char* cstr) {
        _s = cstr != nullptr ? cstr : "";
        return *this;
    }
    String& operator=(char c) {
        _s = c;
        return *this;
    }

    size_t length() const {
        return _s.size();
    }
    bool isEmpty() const {
        return _s.empty();
    }
    const char* c_str() const {
        return _s.c_str();
    }
    char operator[](unsigned int idx) const {
        return _s[idx];
    }
    char& operator[](unsigned int idx) {
        return _s[idx];
    }

    void concat(const char* s) {
        if (s != nullptr) _s += s;
    }
    void concat(const String& o) {
        _s += o._s;
    }
    void concat(char c) {
        _s += c;
    }
    void concat(long v) {
        _s += std::to_string(v);
    }
    void concat(unsigned long v) {
        _s += std::to_string(v);
    }

    String& operator+=(const char* s) {
        concat(s);
        return *this;
    }
    String& operator+=(char c) {
        concat(c);
        return *this;
    }
    String& operator+=(const String& o) {
        concat(o);
        return *this;
    }
    String& operator+=(long v) {
        concat(v);
        return *this;
    }
    String& operator+=(unsigned long v) {
        concat(v);
        return *this;
    }
    String& operator+=(int v) {
        _s += std::to_string(v);
        return *this;
    }
    String& operator+=(unsigned int v) {
        _s += std::to_string(v);
        return *this;
    }
    // NOTE: no operator+=(long long) / operator+=(unsigned long long) pair.
    // On Windows size_t == unsigned long long and GCC's long long/unsigned
    // long long overloads create ambiguity when building "String + size_t";
    // the unsigned long long overload below is kept (it is the exact match
    // for size_t on Windows), and a signed variant is intentionally omitted.
    String& operator+=(unsigned long long v) {
        _s += std::to_string(v);
        return *this;
    }

    String operator+(const char* s) const {
        String r(*this);
        r.concat(s);
        return r;
    }
    String operator+(const String& o) const {
        String r(*this);
        r.concat(o);
        return r;
    }
    String operator+(long v) const {
        String r(*this);
        r.concat(v);
        return r;
    }
    String operator+(unsigned long v) const {
        String r(*this);
        r.concat(v);
        return r;
    }
    // See the NOTE above about signed/unsigned long long ambiguity.
    String operator+(unsigned long long v) const {
        String r(*this);
        r += v;
        return r;
    }

    bool operator==(const String& o) const {
        return _s == o._s;
    }
    bool operator==(const char* cstr) const {
        return _s == (cstr != nullptr ? cstr : "");
    }
    bool operator!=(const String& o) const {
        return !(*this == o);
    }
    bool equals(const char* cstr) const {
        return *this == cstr;
    }
    bool equalsIgnoreCase(const char* cstr) const {
        if (cstr == nullptr) return false;
        std::string a = _s, b = cstr;
        for (char& c : a) c = (char)tolower((unsigned char)c);
        for (char& c : b) c = (char)tolower((unsigned char)c);
        return a == b;
    }
    bool equalsIgnoreCase(const String& o) const {
        return equalsIgnoreCase(o.c_str());
    }
    bool startsWith(const char* prefix) const {
        if (prefix == nullptr) return false;
        return _s.rfind(prefix, 0) == 0;
    }
    bool startsWith(const String& prefix) const {
        return startsWith(prefix.c_str());
    }

    void trim() {
        size_t b = _s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) {
            _s.clear();
            return;
        }
        size_t e = _s.find_last_not_of(" \t\r\n");
        _s       = _s.substr(b, e - b + 1);
    }

    String substring(unsigned int begin, unsigned int end) const {
        if (begin >= end || begin >= _s.size()) return String();
        return String(_s.substr(begin, end - begin).c_str());
    }
    String substring(unsigned int begin) const {
        return substring(begin, (unsigned int)_s.size());
    }

    void toUpperCase() {
        for (char& c : _s) c = (char)toupper((unsigned char)c);
    }
    void toLowerCase() {
        for (char& c : _s) c = (char)tolower((unsigned char)c);
    }

    // ArduinoJson's generic reader consumes a String via read().
    size_t _readpos = 0;
    int    read() {
        if (_readpos >= _s.size()) return -1;
        return (unsigned char)_s[_readpos++];
    }

    void replace(const char* from, const char* to) {
        replaceWith(from, to != nullptr ? to : "");
    }
    void replace(const char* from, const String& to) {
        replaceWith(from, to.c_str() != nullptr ? to.c_str() : "");
    }

   private:
    void replaceWith(const char* from, const char* to) {
        if (from == nullptr) return;
        std::string f(from), t(to != nullptr ? to : ""), out;
        size_t      pos = 0;
        while (pos < _s.size()) {
            size_t idx = _s.find(f, pos);
            if (idx == std::string::npos) {
                out += _s.substr(pos);
                break;
            }
            out += _s.substr(pos, idx - pos);
            out += t;
            pos = idx + f.size();
        }
        _s = out;
    }

   public:
    long toInt() const {
        return strtol(_s.c_str(), nullptr, 10);
    }
    float toFloat() const {
        return (float)atof(_s.c_str());
    }

   private:
    std::string _s;
};

inline String operator+(const char* cstr, const String& rhs) {
    return String(cstr) + rhs;
}
inline String operator+(char c, const String& rhs) {
    return String(c) + rhs;
}

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------
inline unsigned long& __mockMillis() {
    static unsigned long t = 0;
    return t;
}
inline unsigned long millis() {
    return __mockMillis();
}
inline void mockSetMillis(unsigned long v) {
    __mockMillis() = v;
}
inline void mockAdvanceMillis(unsigned long d) {
    __mockMillis() += d;
}
inline void mockResetMillis() {
    __mockMillis() = 0;
}
// Advance the fake clock by the sleep duration so blocking loops that use
// millis() (e.g. WifiManager::tryConnect) actually make progress and
// terminate instead of spinning forever.
inline void delay(unsigned long ms) {
    mockAdvanceMillis(ms);
}

// ---------------------------------------------------------------------------
// Serial
// ---------------------------------------------------------------------------
struct MockSerial {
    void begin(unsigned long) {}
    template <typename T>
    void println(T) {}
    void println() {}
    template <typename T>
    void print(T) {}
    int  printf(const char*, ...) {
        return 0;
    }
    void flush() {}
    int  available() {
        return 0;
    }
    String readStringUntil(char) {
        return String();
    }
};
static MockSerial Serial;

// ---------------------------------------------------------------------------
// IPAddress
// ---------------------------------------------------------------------------
class IPAddress {
   public:
    IPAddress() {
        memset(_addr, 0, sizeof(_addr));
    }
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        set(a, b, c, d);
    }

    void set(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        _addr[0] = a;
        _addr[1] = b;
        _addr[2] = c;
        _addr[3] = d;
    }

    String toString() const {
        char buf[20];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", _addr[0], _addr[1], _addr[2], _addr[3]);
        return String(buf);
    }

    bool operator==(const IPAddress& o) const {
        return memcmp(_addr, o._addr, sizeof(_addr)) == 0;
    }
    uint8_t operator[](int i) const {
        return _addr[i];
    }

   private:
    uint8_t _addr[4];
};

// ---------------------------------------------------------------------------
// ESP
// ---------------------------------------------------------------------------
inline uint64_t& __mockEfuseMac() {
    static uint64_t v = 0x1234ABCDULL;
    return v;
}
inline void mockSetEfuseMac(uint64_t v) {
    __mockEfuseMac() = v;
}
struct MockEsp {
    uint64_t getEfuseMac() const {
        return __mockEfuseMac();
    }
};
static MockEsp ESP;