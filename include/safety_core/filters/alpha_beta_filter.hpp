#pragma once

#include "safety_core/config/system_config.hpp"

namespace safety_core::filters
{

    struct AlphaBetaParams
    {
        double alpha{0.85};
        double beta{0.005};
    };

    class AlphaBetaFilter
    {
      public:
        AlphaBetaFilter() = default;

        void apply_config(const config::SystemConfig& cfg) noexcept
        {
            config_ = &cfg;
        }
        void set_params(const AlphaBetaParams& params) noexcept
        {
            params_ = params;
        }
        void reset(double position = 0.0, double velocity = 0.0) noexcept
        {
            position_ = position;
            velocity_ = velocity;
        }

        double update(double measurement) noexcept;
        [[nodiscard]] double position() const noexcept
        {
            return position_;
        }
        [[nodiscard]] double velocity() const noexcept
        {
            return velocity_;
        }

      private:
        [[nodiscard]] double dt_seconds() const noexcept;

        const config::SystemConfig* config_{nullptr};
        AlphaBetaParams params_{};
        double position_{0.0};
        double velocity_{0.0};
    };

} // namespace safety_core::filters
