#pragma once

#include <atomic>
#include <cstdint>

namespace safety_core::platform::sensors
{

    struct ImuSample
    {
        double angular_velocity_x_radps{0.0};
        double angular_velocity_y_radps{0.0};
        double angular_velocity_z_radps{0.0};
        double linear_accel_x_mps2{0.0};
        double linear_accel_y_mps2{0.0};
        double linear_accel_z_mps2{0.0};
        std::uint64_t timestamp_ns{0U};
        std::uint8_t status{0U}; // bitmask: 0x01=valid, 0x02=clipped, 0x04=self_test_fail
    };

    namespace imu_status
    {
        constexpr std::uint8_t kValid        = 0x01U;
        constexpr std::uint8_t kClipped      = 0x02U;
        constexpr std::uint8_t kSelfTestFail = 0x04U;
    } // namespace imu_status

    class ImuSensor
    {
      public:
        virtual ~ImuSensor()                    = default;
        virtual ImuSample read() const noexcept = 0;

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

    class BufferedImuSensor final : public ImuSensor
    {
      public:
        ImuSample read() const noexcept override
        {
            // Seqlock read: retry until consistent
            ImuSample result{};
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

        void write(ImuSample sample) noexcept
        {
            seq_.fetch_add(1U, std::memory_order_release);
            sample_ = sample;
            seq_.fetch_add(1U, std::memory_order_release);
        }

      private:
        ImuSample sample_{};
        std::atomic<std::uint32_t> seq_{0U};
    };

} // namespace safety_core::platform::sensors
