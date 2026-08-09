#pragma once

#include <cstddef>
#include <cstdint>

namespace cryptoapp {

enum class ApiStatus : uint8_t {
    HEALTHY,     // Green: recent requests succeeding
    DEGRADED,    // Yellow: some recent requests failed
    UNAVAILABLE  // Red: all recent requests failing
};

/**
 * Tracks the health of the market-data API using a rolling
 * success/failure history.
 *
 * The indicator is deliberately not sensitive to a single failed
 * request: it only turns RED after a sustained run of failures and
 * recovers gradually as successes come back.
 */
class ApiHealth {
   public:
    ApiHealth() = default;

    /** Record a successful API request. */
    void recordSuccess();

    /** Record a failed API request. */
    void recordFailure();

    /** Current derived status. */
    ApiStatus status() const;

    /** Number of requests retained in the rolling history. */
    static constexpr size_t HISTORY_SIZE = 8;

   private:
    bool    _history[HISTORY_SIZE] = {};  // true = success
    size_t  _head                  = 0;
    size_t  _count                 = 0;
    uint8_t _successes             = 0;
};

}  // namespace cryptoapp