// libFuzzer harness for the bounded estimation filters.
//
// Interprets the input as a byte stream of (double measurement,
// uint64 timestamp_ns) pairs and drives the alpha-beta and complementary
// filters with them, including NaN/Inf and non-monotonic timestamps.
// The harness asserts the filters never crash and the complementary
// filter's NaN clamp keeps its state finite.

#include "safety_core/filters/alpha_beta_filter.hpp"
#include "safety_core/filters/complementary_filter.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
    double read_double(const std::uint8_t* data)
    {
        double value = 0.0;
        std::memcpy(&value, data, sizeof(double));
        return value;
    }

    std::uint64_t read_u64(const std::uint8_t* data)
    {
        std::uint64_t value = 0U;
        std::memcpy(&value, data, sizeof(std::uint64_t));
        return value;
    }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    safety_core::filters::AlphaBetaFilter alpha_beta;
    safety_core::filters::ComplementaryFilter complementary;
    complementary.set_params(safety_core::filters::ComplementaryFilterParams{0.9, 1e6, 0.0});

    constexpr std::size_t kPairSize = sizeof(double) + sizeof(std::uint64_t);
    const std::size_t pairs         = size / kPairSize;

    for (std::size_t i = 0U; i < pairs; ++i)
    {
        const double measurement      = read_double(data + (i * kPairSize));
        const std::uint64_t timestamp = read_u64(data + (i * kPairSize) + sizeof(double));

        // Bound timestamps to a plausible range so dt math stays interesting
        // rather than saturating to absurd values every run.
        const std::uint64_t ts = timestamp % 1000000000000ULL;

        static_cast<void>(alpha_beta.update(measurement, ts));
        const double state = complementary.update(measurement, measurement, ts);

        // The complementary filter must clamp non-finite state to zero.
        if (!std::isfinite(state))
        {
            __builtin_trap();
        }
    }

    return 0;
}
