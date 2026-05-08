#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include <atomic>
#include <cstdint>

namespace safety_core::platform::sensors
{

    struct OdometrySample
    {
        double position_m{0.0};
        double velocity_mps{0.0};
        std::uint64_t timestamp_ns{0U};
    };

    struct OdometryIncrement
    {
        double delta_x_m{0.0};
        double delta_y_m{0.0};
        double delta_yaw_rad{0.0};
        double linear_velocity_mps{0.0};
        double angular_velocity_radps{0.0};
        std::uint64_t timestamp_ns{0U};
    };

    class OdometrySensor
    {
      public:
        virtual ~OdometrySensor()                    = default;
        virtual OdometrySample read() const noexcept = 0;

        [[nodiscard]] virtual bool is_fresh(std::uint64_t now_ns, std::uint64_t max_age_ns) const noexcept
        {
            const auto sample = read();
            if (sample.timestamp_ns == 0U)
            {
                return false;
            }
            if (now_ns < sample.timestamp_ns)
            {
                return false;
            }
            return (now_ns - sample.timestamp_ns) <= max_age_ns;
        }
    };

    class BufferedOdometrySensor final : public OdometrySensor
    {
      public:
        BufferedOdometrySensor() = default;

        OdometrySample read() const noexcept override
        {
            OdometrySample result{};
            std::uint32_t seq0{};
            std::uint32_t seq1{};
            do
            {
                seq0   = seq_.load(std::memory_order_acquire);
                result = sample_;
                seq1   = seq_.load(std::memory_order_acquire);
            } while ((seq0 != seq1) || (seq0 & 1U));
            return result;
        }

        void write(OdometrySample sample) noexcept
        {
            seq_.fetch_add(1U, std::memory_order_release);
            sample_ = sample;
            seq_.fetch_add(1U, std::memory_order_release);
        }

        [[nodiscard]] OdometryIncrement read_increment() const noexcept
        {
            return increment_;
        }

        void write_increment(OdometryIncrement inc) noexcept
        {
            increment_ = inc;
        }

      private:
        OdometrySample sample_{};
        OdometryIncrement increment_{};
        std::atomic<std::uint32_t> seq_{0U};
    };

} // namespace safety_core::platform::sensors
