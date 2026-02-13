#pragma once

#include "safety_core/platform/clock.hpp"

namespace safety_core::platform
{

    class ManualClock final : public Clock
    {
      public:
        ManualClock() noexcept = default;

        [[nodiscard]] time::TimePoint now() const noexcept override
        {
            return now_;
        }

        void set(time::TimePoint tp) noexcept
        {
            now_ = tp;
        }
        void advance(time::Duration delta) noexcept
        {
            now_ += delta;
        }

      private:
        time::TimePoint now_{};
    };

} // namespace safety_core::platform
