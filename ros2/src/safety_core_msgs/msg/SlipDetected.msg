# Wheel slip detection status
# Published by slip_detector_node

std_msgs/Header header
bool is_slipping
float32 slip_magnitude   # Absolute velocity difference in m/s
float32 slip_ratio       # Slip as percentage (0.0-100.0+)
string detail            # Description: "linear slip", "angular slip", "combined slip", "no slip"
