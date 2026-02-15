#pragma once

#include <cstdint>

namespace safety_core::platform::sensors
{

    struct OdometrySample
    {
        double position_m{0.0};
        double velocity_mps{0.0};
        std::uint64_t timestamp_ns{0U};
    };

    class OdometrySensor
    {
      public:
        virtual ~OdometrySensor()                    = default;
        virtual OdometrySample read() const noexcept = 0;
    };

    class BufferedOdometrySensor final : public OdometrySensor
    {
      public:
        BufferedOdometrySensor() = default;

        OdometrySample read() const noexcept override
        {
            return sample_;
        }
        void write(OdometrySample sample) noexcept
        {
            sample_ = sample;
        }

      private:
        OdometrySample sample_{};
    };

} // namespace safety_core::platform::sensors
