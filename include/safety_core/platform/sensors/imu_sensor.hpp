#pragma once

#include <cstdint>

namespace safety_core::platform::sensors
{

    struct ImuSample
    {
        double angular_velocity_z_radps{0.0};
        double linear_accel_x_mps2{0.0};
        double linear_accel_y_mps2{0.0};
        std::uint64_t timestamp_ns{0U};
    };

    class ImuSensor
    {
      public:
        virtual ~ImuSensor()                    = default;
        virtual ImuSample read() const noexcept = 0;
    };

    class BufferedImuSensor final : public ImuSensor
    {
      public:
        ImuSample read() const noexcept override
        {
            return sample_;
        }
        void write(ImuSample sample) noexcept
        {
            sample_ = sample;
        }

      private:
        ImuSample sample_{};
    };

} // namespace safety_core::platform::sensors
