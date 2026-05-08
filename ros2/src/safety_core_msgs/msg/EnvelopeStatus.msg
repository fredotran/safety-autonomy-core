# Snapshot of safety envelope evaluation.
std_msgs/Header header

# Inputs
float64 distance_to_obstacle_m
float64 current_speed_mps

# Outputs from safety_core::safety::evaluate_stop_distance
bool within_envelope
SafetyZone zone
float64 stopping_distance_m
float64 required_clearance_m
float64 recommended_speed_limit_mps

# Robot footprint accounted for
float64 footprint_length_m
float64 footprint_width_m
float64 footprint_front_overhang_m
