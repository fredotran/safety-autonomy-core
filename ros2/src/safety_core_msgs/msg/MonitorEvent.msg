# Mirrors safety_core::safety::MonitorEvent for external observers.
uint8 SEVERITY_NONE=0
uint8 SEVERITY_WARNING=1
uint8 SEVERITY_DEGRADED=2
uint8 SEVERITY_CRITICAL=3

uint8 TYPE_CLOCK_REGRESSION=0
uint8 TYPE_LOCALIZATION_STALE=1
uint8 TYPE_TRAJECTORY_ENVELOPE=2
uint8 TYPE_DIAGNOSTIC_DROP=3

std_msgs/Header header
uint8 type
uint8 severity
uint16 code
string detail
