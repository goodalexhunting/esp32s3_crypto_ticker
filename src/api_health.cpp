#include "api_health.h"

namespace cryptoapp {

void ApiHealth::recordSuccess() {
    if (_count < HISTORY_SIZE) {
        _count++;
    } else {
        // Buffer full: remove the oldest entry from the success count.
        if (_history[_head]) {
            _successes--;
        }
    }

    _history[_head] = true;
    _successes++;
    _head = (_head + 1) % HISTORY_SIZE;
}

void ApiHealth::recordFailure() {
    if (_count < HISTORY_SIZE) {
        _count++;
    } else {
        // Buffer full: remove the oldest entry from the success count.
        if (_history[_head]) {
            _successes--;
        }
    }

    _history[_head] = false;
    _head           = (_head + 1) % HISTORY_SIZE;
}

ApiStatus ApiHealth::status() const {
    if (_count == 0) {
        return ApiStatus::UNAVAILABLE;
    }

    if (_successes == _count) {
        return ApiStatus::HEALTHY;
    }

    if (_successes == 0) {
        return ApiStatus::UNAVAILABLE;
    }

    return ApiStatus::DEGRADED;
}

}  // namespace cryptoapp