# Aggregate safety + state machine status.

# State machine modes mirroring safety_core::sm::Mode.
uint8 INIT=0
uint8 IDLE=1
uint8 MOVING=2
uint8 DEGRADED=3
uint8 AVOIDING_OBSTACLE=4
uint8 LOCALIZATION_LOST=5
uint8 DOCKING=6
uint8 SAFE_STOP=7

std_msgs/Header header
uint8 mode
bool fault_latched
uint16 fault_code
SafetyZone zone
bool safe_stop_requested
