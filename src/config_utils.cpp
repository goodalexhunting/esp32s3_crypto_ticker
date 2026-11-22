#include "config_utils.h"

#include "app_config.h"

namespace cryptoapp {

bool parseTickerIndex(const char* value, size_t& index) {
    if (value == nullptr || value[0] == '\0') {
        return false;
    }

    size_t parsed = 0;
    for (const char* cursor = value; *cursor != '\0'; ++cursor) {
        const char digit = *cursor;
        if (digit < '0' || digit > '9') {
            return false;
        }

        parsed = parsed * 10 + static_cast<size_t>(digit - '0');
        if (parsed >= MAX_TICKERS) {
            return false;
        }
    }

    index = parsed;
    return true;
}

}  // namespace cryptoapp
