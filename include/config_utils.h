#pragma once

#include <cstddef>

namespace cryptoapp {

/** Parse a non-negative decimal ticker index within the configured capacity. */
bool parseTickerIndex(const char* value, size_t& index);

}  // namespace cryptoapp
