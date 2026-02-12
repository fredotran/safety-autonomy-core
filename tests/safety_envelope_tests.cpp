#include <iostream>

#include "safety_core/safety/safety_envelope.hpp"

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    using safety_core::safety::EnvelopeEvaluation;
    using safety_core::safety::evaluate_stop_distance;

    bool ok = true;

    EnvelopeEvaluation eval = evaluate_stop_distance(/*distance_to_obstacle_m=*/5.0,
                                                     /*current_speed_mps=*/1.0,
                                                     /*max_comfort_decel_mps2=*/1.0,
                                                     /*control_latency_s=*/0.2,
                                                     /*safety_buffer_m=*/0.2);
    ok &= check(eval.within_envelope, "Should be within envelope for 5m obstacle at 1 m/s");

    eval = evaluate_stop_distance(/*distance_to_obstacle_m=*/0.5,
                                  /*current_speed_mps=*/1.5,
                                  /*max_comfort_decel_mps2=*/0.8,
                                  /*control_latency_s=*/0.2,
                                  /*safety_buffer_m=*/0.1);
    ok &= check(!eval.within_envelope, "Should exceed envelope for close obstacle at higher speed");

    eval = evaluate_stop_distance(/*distance_to_obstacle_m=*/2.0,
                                  /*current_speed_mps=*/0.0,
                                  /*max_comfort_decel_mps2=*/1.0,
                                  /*control_latency_s=*/0.2,
                                  /*safety_buffer_m=*/0.1);
    ok &= check(eval.within_envelope, "Stopped vehicle should always be within envelope");

    if (!ok) {
        return 1;
    }

    std::cout << "[PASS] safety envelope smoke tests" << '\n';
    return 0;
}
