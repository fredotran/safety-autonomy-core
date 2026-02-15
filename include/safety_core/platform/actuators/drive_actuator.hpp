#pragma once

#include <cstdint>

namespace safety_core::platform::actuators
{

    struct DriveCommand
    {
        double velocity_mps{0.0};
        double steering_rad{0.0};
        std::uint64_t timestamp_ns{0U};
    };

    class DriveActuator
    {
      public:
        virtual ~DriveActuator()                               = default;
        virtual void command(const DriveCommand& cmd) noexcept = 0;
        virtual DriveCommand last_command() const noexcept     = 0;
    };

    class BufferedDriveActuator final : public DriveActuator
    {
      public:
        void command(const DriveCommand& cmd) noexcept override
        {
            last_ = cmd;
        }
        DriveCommand last_command() const noexcept override
        {
            return last_;
        }

      private:
        DriveCommand last_{};
    };

} // namespace safety_core::platform::actuators
