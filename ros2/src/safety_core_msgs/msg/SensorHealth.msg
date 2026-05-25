# Sensor health status for safety monitoring.
# Published by sensor_monitor_node and consumed by safety_supervisor_node.

# Health status constants
uint8 UNKNOWN=0
uint8 HEALTHY=1
uint8 DEGRADED=2
uint8 FAULT=3

std_msgs/Header header
string sensor_name    # "lidar", "imu", "gps", "odometry", "all"
uint8 status          # UNKNOWN, HEALTHY, DEGRADED, FAULT
float64 update_rate_hz    # Actual measured update rate (Hz)
float64 expected_rate_hz  # Expected update rate (Hz)
float64 data_age_s        # Time since last message (seconds)
float64 error_rate        # Error/dropout rate (0.0-1.0)
string detail             # Human-readable status detail
