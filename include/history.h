#pragma once

#include <cstddef>
#include <cstdint>

#include "app_config.h"

namespace cryptoapp {

/**
 * Bounded ring buffer of price samples for a single ticker.
 * No heap allocation - fixed-size storage.
 */
class HistoryBuffer {
   public:
    HistoryBuffer() = default;

    void reset();

    /** Append a price sample. Overwrites the oldest sample when full. */
    void push(float price);

    /** Number of samples currently stored. */
    size_t size() const {
        return _size;
    }

    bool empty() const {
        return _size == 0;
    }

    /** Oldest sample. */
    float front() const {
        return _data[_head];
    }

    /** Most recent sample. */
    float back() const {
        return _data[(_head + _size - 1) % HISTORY_POINTS];
    }

    /** Read sample at logical index i (0 = oldest, size()-1 = newest). */
    float at(size_t i) const {
        return _data[(_head + i) % HISTORY_POINTS];
    }

    /** Lowest value in the buffer, or 0 if empty. */
    float min() const;

    /** Highest value in the buffer, or 0 if empty. */
    float max() const;

   private:
    float  _data[HISTORY_POINTS] = {};
    size_t _head                 = 0;
    size_t _size                 = 0;
};

}  // namespace cryptoapp