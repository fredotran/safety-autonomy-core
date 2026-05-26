# Sensor health performance metrics over a rolling window.
# Published by sensor_monitor_node on /safety/sensor_health_metrics.

string sensor_name
float32 avg_update_rate_hz
float32 min_update_rate_hz
float32 healthy_percentage
float32 degraded_percentage
float32 fault_percentage
uint32 transition_count
builtin_interfaces/Duration window_duration
