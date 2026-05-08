#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

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

        [[nodiscard]] virtual bool is_command_fresh(std::uint64_t now_ns, std::uint64_t max_age_ns) const noexcept
        {
            const auto cmd = last_command();
            if (cmd.timestamp_ns == 0U)
            {
                return false;
            }
            if (now_ns < cmd.timestamp_ns)
            {
                return false;
            }
            return (now_ns - cmd.timestamp_ns) <= max_age_ns;
        }
    };

    class BufferedDriveActuator final : public DriveActuator
    {
      public:
        void command(const DriveCommand& cmd) noexcept override
        {
            last_          = cmd;
            command_count_ = command_count_ + 1U;
        }
        DriveCommand last_command() const noexcept override
        {
            return last_;
        }

        [[nodiscard]] std::uint64_t command_count() const noexcept
        {
            return command_count_;
        }

      private:
        DriveCommand last_{};
        std::uint64_t command_count_{0U};
    };

} // namespace safety_core::platform::actuators
