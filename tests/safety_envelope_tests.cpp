#include "safety_core/safety/safety_envelope.hpp"

#include <cmath>
#include <iostream>

namespace
{

    bool check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }
        return true;
    }

} // namespace

int main()
{
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

    // Property: required clearance should increase monotonically with speed.
    double previous_required = 0.0;
    for (double speed = 0.0; speed <= 3.0; speed += 0.25)
    {
        eval = evaluate_stop_distance(/*distance_to_obstacle_m=*/100.0,
                                      /*current_speed_mps=*/speed,
                                      /*max_comfort_decel_mps2=*/0.8,
                                      /*control_latency_s=*/0.2,
                                      /*safety_buffer_m=*/0.1);
        ok &= check(eval.required_clearance + 1e-9 >= previous_required,
                    "Required clearance should be monotonic with speed");
        previous_required = eval.required_clearance;
    }

    // Property: more available obstacle distance cannot reduce safety status.
    const auto near_eval = evaluate_stop_distance(/*distance_to_obstacle_m=*/1.0,
                                                  /*current_speed_mps=*/1.2,
                                                  /*max_comfort_decel_mps2=*/0.8,
                                                  /*control_latency_s=*/0.2,
                                                  /*safety_buffer_m=*/0.1);
    const auto far_eval  = evaluate_stop_distance(/*distance_to_obstacle_m=*/2.5,
                                                 /*current_speed_mps=*/1.2,
                                                 /*max_comfort_decel_mps2=*/0.8,
                                                 /*control_latency_s=*/0.2,
                                                 /*safety_buffer_m=*/0.1);
    ok &= check(!(near_eval.within_envelope && !far_eval.within_envelope),
                "Increasing obstacle distance must not worsen envelope status");

    // Boundary: negative speed should clamp to zero when dynamics are otherwise valid.
    eval = evaluate_stop_distance(/*distance_to_obstacle_m=*/1.0,
                                  /*current_speed_mps=*/-1.0,
                                  /*max_comfort_decel_mps2=*/1.0,
                                  /*control_latency_s=*/0.0,
                                  /*safety_buffer_m=*/0.0);
    ok &= check(eval.within_envelope, "Negative speed should clamp to zero speed");

    // Boundary: non-positive decel yields infinite stopping distance by design.
    eval = evaluate_stop_distance(/*distance_to_obstacle_m=*/1.0,
                                  /*current_speed_mps=*/1.0,
                                  /*max_comfort_decel_mps2=*/-0.1,
                                  /*control_latency_s=*/0.1,
                                  /*safety_buffer_m=*/0.0);
    ok &= check(!eval.within_envelope, "Non-positive decel should fail envelope");
    ok &= check(std::isinf(eval.required_clearance), "Non-positive decel should produce infinite clearance");

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] safety envelope smoke tests" << '\n';
    return 0;
}
