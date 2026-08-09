#include "history.h"

namespace cryptoapp {

void HistoryBuffer::reset() {
    _head = 0;
    _size = 0;
}

void HistoryBuffer::push(float price) {
    size_t idx = (_head + _size) % HISTORY_POINTS;
    _data[idx] = price;
    if (_size < HISTORY_POINTS) {
        _size++;
    } else {
        // Buffer full: advance the head so the oldest sample is dropped.
        _head = (_head + 1) % HISTORY_POINTS;
    }
}

float HistoryBuffer::min() const {
    if (_size == 0) {
        return 0.0f;
    }
    float m = _data[_head];
    for (size_t i = 1; i < _size; i++) {
        float v = _data[(_head + i) % HISTORY_POINTS];
        if (v < m) m = v;
    }
    return m;
}

float HistoryBuffer::max() const {
    if (_size == 0) {
        return 0.0f;
    }
    float m = _data[_head];
    for (size_t i = 1; i < _size; i++) {
        float v = _data[(_head + i) % HISTORY_POINTS];
        if (v > m) m = v;
    }
    return m;
}

}  // namespace cryptoapp