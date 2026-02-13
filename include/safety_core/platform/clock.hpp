#pragma once

#include "safety_core/common/time.hpp"

namespace safety_core::platform
{

    class Clock
    {
      public:
        virtual ~Clock()                                           = default;
        [[nodiscard]] virtual time::TimePoint now() const noexcept = 0;
    };

    class SteadyClock final : public Clock
    {
      public:
        static SteadyClock& instance() noexcept;
        static Clock* instance_ptr() noexcept;
        [[nodiscard]] time::TimePoint now() const noexcept override;

      private:
        SteadyClock() = default;
    };

} // namespace safety_core::platform
