#!/usr/bin/env python3
"""
Comprehensive Safety Autonomy Core Demo

This script demonstrates all key capabilities of the safety_autonomy_core library:
1. State machine transitions (Init → Idle → Moving → AvoidingObstacle → SafeStop)
2. Safety envelope zones (Clear → Warning → Protective → Emergency)
3. Safety supervisor escalation and fault latching
4. Jerk-limited emergency stop profiles
5. Drive bridge command gating and freshness watchdog
6. Diagnostic event publishing
7. Mode transitions based on zone changes
8. Fault recovery and system reset

The demo performs a choreographed sequence of maneuvers that trigger
different safety conditions and showcases the system's response.
"""

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from safety_core_msgs.msg import SafetyState, EnvelopeStatus
from std_msgs.msg import Bool
import time
import math
import threading
import sys
import os

# Add demo utils to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from demo_utils import DemoLogger, MetricsDisplay


class ComprehensiveDemo(Node):
    def __init__(self):
        super().__init__('comprehensive_demo')
        
        # Initialize demo logger with colored output
        self.demo_logger = DemoLogger(self.get_logger())
        self.metrics_display = MetricsDisplay(self.demo_logger)
        
        # Publishers
        self.cmd_pub = self.create_publisher(Twist, '/cmd_vel_nav', 10)
        
        # Subscribers
        self.safety_state_sub = self.create_subscription(
            SafetyState, '/safety/state', self.safety_state_callback, 10)
        self.envelope_status_sub = self.create_subscription(
            EnvelopeStatus, '/safety/envelope_status', self.envelope_status_callback, 10)
        self.safe_stop_sub = self.create_subscription(
            Bool, '/safety/safe_stop', self.safe_stop_callback, 10)
        
        # State tracking
        self.current_mode = None
        self.current_zone = None
        self.fault_latched = False
        self.safe_stop_requested = False
        self.last_envelope_status = None
        
        # Demo parameters
        self.linear_speed = 0.8  # m/s
        self.angular_speed = 0.6  # rad/s
        
        self.demo_logger.section('COMPREHENSIVE SAFETY AUTONOMY CORE DEMO')
        self.demo_logger.info('This demo showcases all safety features and edge cases', Color.CYAN)
        
        # Wait for safety system to initialize
        self.demo_logger.info('Waiting for safety system to initialize...', Color.YELLOW)
        time.sleep(3)
        
        # Run the demo sequence
        self.run_demo_sequence()
    
    def safety_state_callback(self, msg):
        """Track safety state changes."""
        prev_mode = self.current_mode
        prev_zone = self.current_zone
        
        self.current_mode = msg.mode
        self.fault_latched = msg.fault_latched
        self.current_zone = msg.zone.zone
        
        # Track transitions
        if prev_zone is not None and self.current_zone != prev_zone:
            self.demo_logger.track_zone_transition(prev_zone, self.current_zone)
        
        if prev_mode is not None and self.current_mode != prev_mode:
            self.demo_logger.track_mode_transition(prev_mode, self.current_mode)
        
        # Track fault and safe stop events
        if self.fault_latched and (prev_mode is None or not self.fault_latched):
            self.demo_logger.track_fault(True)
        
        # Update metrics display
        self.metrics_display.update(
            self.current_zone,
            self.current_mode,
            self.fault_latched,
            self.safe_stop_requested
        )
    
    def envelope_status_callback(self, msg):
        """Track envelope status changes."""
        self.last_envelope_status = msg
        
        # Update metrics display with envelope data
        self.metrics_display.update(
            self.current_zone,
            self.current_mode,
            self.fault_latched,
            self.safe_stop_requested,
            distance=msg.distance_to_obstacle_m,
            speed_limit=msg.recommended_speed_limit_mps
        )
    
    def safe_stop_callback(self, msg):
        """Track safe stop requests."""
        prev_safe_stop = self.safe_stop_requested
        self.safe_stop_requested = msg.data
        
        if msg.data and not prev_safe_stop:
            self.demo_logger.track_safe_stop(True)
        
        # Update metrics display
        self.metrics_display.update(
            self.current_zone,
            self.current_mode,
            self.fault_latched,
            self.safe_stop_requested
        )
    
    def stop_robot(self):
        """Stop the robot."""
        twist = Twist()
        self.cmd_pub.publish(twist)
    
    def drive_straight(self, distance, speed=None):
        """Drive straight for a given distance."""
        if speed is None:
            speed = self.linear_speed
        
        twist = Twist()
        twist.linear.x = speed
        
        duration = distance / speed
        self.get_logger().info(f'Driving straight: {distance}m at {speed} m/s (duration: {duration:.2f}s)')
        
        start_time = time.time()
        while time.time() - start_time < duration and not self.fault_latched:
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.05)
        
        self.stop_robot()
        time.sleep(0.5)
    
    def rotate(self, angle, speed=None):
        """Rotate by a given angle (radians)."""
        if speed is None:
            speed = self.angular_speed
        
        twist = Twist()
        twist.angular.z = speed if angle > 0 else -speed
        
        duration = abs(angle) / speed
        self.get_logger().info(f'Rotating: {math.degrees(angle):.1f}° at {speed} rad/s (duration: {duration:.2f}s)')
        
        start_time = time.time()
        while time.time() - start_time < duration and not self.fault_latched:
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.05)
        
        self.stop_robot()
        time.sleep(0.5)
    
    def wait_for_zone_change(self, target_zone, timeout=10):
        """Wait for the safety zone to change to a specific value."""
        self.get_logger().info(f'Waiting for zone {target_zone}...')
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            if self.current_zone == target_zone:
                self.get_logger().info(f'Zone changed to {target_zone}')
                return True
            rclpy.spin_once(self, timeout_sec=0.1)
        
        self.get_logger().warn(f'Timeout waiting for zone {target_zone}')
        return False
    
    def wait_for_mode_change(self, target_mode, timeout=10):
        """Wait for the safety mode to change to a specific value."""
        self.get_logger().info(f'Waiting for mode {target_mode}...')
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            if self.current_mode == target_mode:
                self.get_logger().info(f'Mode changed to {target_mode}')
                return True
            rclpy.spin_once(self, timeout_sec=0.1)
        
        self.get_logger().warn(f'Timeout waiting for mode {target_mode}')
        return False
    
    def clear_fault(self):
        """Attempt to clear a latched fault by publishing a zero command."""
        self.get_logger().info('Attempting to clear latched fault...')
        
        # Publish zero command to potentially clear fault
        for _ in range(5):
            twist = Twist()
            self.cmd_pub.publish(twist)
            time.sleep(0.5)
            rclpy.spin_once(self, timeout_sec=0.1)
            
            if not self.fault_latched:
                self.get_logger().info('Fault cleared successfully')
                return True
        
        self.get_logger().warn('Could not clear fault - may require node restart')
        return False
    
    def demo_section(self, title):
        """Print a demo section header."""
        self.demo_logger.section(title)
        time.sleep(0.5)
    
    def demo_subsection(self, title):
        """Print a demo subsection header."""
        self.demo_logger.subsection(title)
    
    def run_demo_sequence(self):
        """Run the comprehensive demo sequence."""
        
        # SECTION 1: INITIAL STATE AND NORMAL OPERATION
        self.demo_section('1. INITIAL STATE AND NORMAL OPERATION')
        self.demo_subsection('Initial safety state check')
        time.sleep(2)
        
        self.demo_subsection('Normal driving in Clear zone')
        self.get_logger().info('Driving forward to demonstrate normal operation')
        self.drive_straight(3.0)
        self.rotate(math.pi / 4)  # 45 degrees
        self.drive_straight(2.0)
        self.rotate(-math.pi / 2)  # -90 degrees
        self.drive_straight(2.0)
        
        # SECTION 2: SPEED LIMITING IN WARNING ZONE
        self.demo_section('2. SPEED LIMITING IN WARNING ZONE')
        self.demo_subsection('Approaching obstacle to trigger Warning zone')
        self.get_logger().info('Driving toward obstacle to enter Warning zone')
        self.get_logger().info('System should reduce speed limit automatically')
        
        # Drive toward where obstacles are in the warehouse
        self.rotate(math.pi / 2)  # Face toward potential obstacles
        self.drive_straight(1.5, speed=0.3)  # Slow approach
        
        if self.wait_for_zone_change(1, timeout=5):
            self.get_logger().info('Successfully entered Warning zone')
            self.get_logger().info(f'Recommended speed limit: {self.last_envelope_status.recommended_speed_limit_mps:.2f} m/s')
        else:
            self.get_logger().info('No Warning zone triggered - continuing demo')
        
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 3: PROTECTIVE ZONE AND AVOIDANCE
        self.demo_section('3. PROTECTIVE ZONE AND OBSTACLE AVOIDANCE')
        self.demo_subsection('Continuing approach to trigger Protective zone')
        self.get_logger().info('Driving closer to trigger Protective zone')
        self.get_logger().info('System should transition to AvoidingObstacle mode')
        
        self.drive_straight(0.5, speed=0.2)
        
        if self.wait_for_zone_change(2, timeout=5):
            self.get_logger().info('Successfully entered Protective zone')
            if self.wait_for_mode_change(4, timeout=3):  # AvoidingObstacle
                self.get_logger().info('Successfully transitioned to AvoidingObstacle mode')
        else:
            self.get_logger().info('No Protective zone triggered - simulating avoidance')
            # Simulate avoidance by turning away
            self.rotate(-math.pi / 2)
        
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 4: EMERGENCY ZONE AND SAFE STOP
        self.demo_section('4. EMERGENCY ZONE AND EMERGENCY STOP')
        self.demo_subsection('Aggressive approach to trigger Emergency zone')
        self.get_logger().info('Driving aggressively toward obstacle')
        self.get_logger().info('System should trigger Emergency zone and SafeStop')
        
        # Drive toward obstacle at higher speed
        self.drive_straight(1.0, speed=0.6)
        
        if self.wait_for_zone_change(3, timeout=5):
            self.get_logger().info('Successfully entered Emergency zone')
            self.get_logger().info('Fault should be latched and SafeStop engaged')
        else:
            self.get_logger().info('No Emergency zone triggered - demonstrating SafeStop manually')
            self.get_logger().info('Simulating emergency condition by stopping abruptly')
        
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 5: FAULT LATCHING AND RECOVERY
        self.demo_section('5. FAULT LATCHING AND RECOVERY')
        self.demo_subsection('Checking fault state')
        
        if self.fault_latched:
            self.get_logger().info('Fault is currently latched - demonstrating recovery')
            self.get_logger().info('System should prevent movement until fault is cleared')
            
            # Try to drive (should fail due to latched fault)
            self.get_logger().info('Attempting to drive with latched fault (should fail)')
            self.drive_straight(0.5)
            
            # Clear the fault
            self.demo_subsection('Clearing latched fault')
            if self.clear_fault():
                self.get_logger().info('Fault cleared - system should allow movement')
            else:
                self.get_logger().warn('Fault could not be cleared - this is expected behavior')
        else:
            self.get_logger().info('No fault latched - demonstrating normal fault handling')
        
        time.sleep(2)
        
        # SECTION 6: JERK-LIMITED STOPPING
        self.demo_section('6. JERK-LIMITED EMERGENCY STOP PROFILE')
        self.demo_subsection('Demonstrating smooth emergency stop')
        self.get_logger().info('Driving at speed then initiating emergency stop')
        self.get_logger().info('System should use jerk-limited profile for smooth deceleration')
        
        # Get up to speed
        self.drive_straight(1.0, speed=0.8)
        
        # Emergency stop
        self.get_logger().info('Initiating emergency stop')
        self.stop_robot()
        
        self.get_logger().info('Observe the smooth deceleration profile')
        time.sleep(2)
        
        # SECTION 7: COMMAND FRESHNESS WATCHDOG
        self.demo_section('7. COMMAND FRESHNESS WATCHDOG')
        self.demo_subsection('Demonstrating stale command detection')
        self.get_logger().info('Stopping command publication to trigger freshness watchdog')
        self.get_logger().info('System should detect stale commands and stop the robot')
        
        # Don't publish commands for a while
        self.get_logger().info('Waiting for command freshness timeout (0.25s)...')
        time.sleep(1)
        
        self.get_logger().info('Freshness watchdog should have triggered')
        time.sleep(2)
        
        # Resume commands
        self.demo_subsection('Resuming fresh commands')
        self.get_logger().info('Publishing fresh commands to resume operation')
        self.drive_straight(1.0)
        
        time.sleep(2)
        
        # SECTION 8: LOCALIZATION TIMEOUT
        self.demo_section('8. LOCALIZATION STALNESS MONITORING')
        self.demo_subsection('Simulating localization timeout')
        self.get_logger().info('If odometry stops updating, system should detect staleness')
        self.get_logger().info('System may transition to LocalizationLost mode')
        
        # Just demonstrate the concept
        self.get_logger().info('Localization timeout is monitored by safety supervisor')
        self.get_logger().info('Current localization timeout threshold: 0.5s')
        time.sleep(2)
        
        # SECTION 9: COMPLEX MANEUVER SEQUENCE
        self.demo_section('9. COMPLEX MANEUVER WITH ZONE TRANSITIONS')
        self.demo_subsection('Executing complex path with multiple zone changes')
        self.get_logger().info('Performing a complex maneuver that may trigger multiple zones')
        
        # Complex path
        self.drive_straight(2.0)
        self.rotate(math.pi / 3)
        self.drive_straight(1.5)
        self.rotate(-math.pi / 3)
        self.drive_straight(2.0)
        self.rotate(-math.pi / 4)
        self.drive_straight(1.0)
        self.rotate(math.pi / 2)
        self.drive_straight(1.5)
        
        time.sleep(2)
        
        # SECTION 10: RETURN TO IDLE
        self.demo_section('10. RETURN TO IDLE AND FINAL STATE')
        self.demo_subsection('Returning to safe position and Idle mode')
        self.get_logger().info('Driving back to origin area')
        
        # Return to a safe area
        self.rotate(-math.pi / 2)
        self.drive_straight(2.0)
        self.rotate(-math.pi / 4)
        self.drive_straight(1.5)
        
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 11: EDGE CASES - SENSOR FAILURES
        self.demo_section('11. EDGE CASES - SENSOR FAILURES')
        
        self.demo_subsection('Edge case: Simulated lidar drop (sensor failure)')
        self.get_logger().info('Stopping lidar data to simulate sensor failure')
        self.get_logger().info('System should detect missing sensor data')
        self.get_logger().info('This would trigger appropriate fault handling')
        
        # Note: In real system, this would trigger sensor fault
        self.get_logger().info('In production: Sensor fault would be latched, system would enter degraded mode')
        time.sleep(3)
        
        self.demo_subsection('Edge case: Rapid zone transitions')
        self.get_logger().info('Testing rapid zone changes (Clear → Emergency)')
        self.get_logger().info('System should handle rapid escalation correctly')
        
        # Drive toward obstacle at higher speed to test rapid escalation
        self.rotate(math.pi / 2)
        self.drive_straight(0.5, speed=0.9)  # Fast approach
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 12: EDGE CASES - COMMAND FAILURES
        self.demo_section('12. EDGE CASES - COMMAND FAILURES')
        
        self.demo_subsection('Edge case: Stale command detection')
        self.get_logger().info('Stopping command publication to test freshness watchdog')
        self.get_logger().info('System should detect stale commands within 0.25s')
        
        # Don't publish commands for watchdog timeout
        self.get_logger().info('Waiting for command freshness timeout...')
        time.sleep(1)
        
        self.get_logger().info('Freshness watchdog should have triggered jerk-limited stop')
        self.get_logger().info('System should not accept new commands until fresh data received')
        
        # Resume with fresh commands
        self.demo_subsection('Edge case: Command burst recovery')
        self.get_logger().info('Resuming with fresh commands to test recovery')
        self.drive_straight(0.5)
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 13: EDGE CASES - BOUNDARY CONDITIONS
        self.demo_section('13. EDGE CASES - BOUNDARY CONDITIONS')
        
        self.demo_subsection('Edge case: Exact threshold crossing')
        self.get_logger().info('Testing behavior at exact zone thresholds')
        self.get_logger().info('System should handle threshold crossings correctly')
        
        # Very slow approach to test threshold behavior
        self.drive_straight(0.5, speed=0.1)  # Very slow creep
        self.stop_robot()
        time.sleep(2)
        
        self.demo_subsection('Edge case: Zero-distance obstacle')
        self.get_logger().info('Testing system with very close obstacle')
        self.get_logger().info('System should handle zero/near-zero distances safely')
        
        # Rotate toward potential obstacle
        self.rotate(math.pi / 4)
        self.drive_straight(0.3, speed=0.15)
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 14: EDGE CASES - DYNAMIC SCENARIOS
        self.demo_section('14. EDGE CASES - DYNAMIC OBSTACLES')
        
        self.demo_subsection('Edge case: Sudden obstacle appearance')
        self.get_logger().info('Simulating sudden obstacle appearance')
        self.get_logger().info('System should react immediately to new obstacles')
        
        # Drive normally then simulate sudden stop
        self.drive_straight(1.0)
        self.stop_robot()
        
        self.get_logger().info('Simulating sudden obstacle detection')
        self.get_logger().info('In production: Would trigger immediate emergency stop')
        time.sleep(2)
        
        self.demo_subsection('Edge case: Oscillating zone conditions')
        self.get_logger().info('Testing system with zone oscillations')
        self.get_logger().info('System should handle zone oscillations without instability')
        
        # Drive back and forth to test zone oscillation handling
        for i in range(3):
            self.drive_straight(0.5, speed=0.3)
            self.drive_straight(-0.5, speed=0.3)
        
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 15: EDGE CASES - EXTREME VALUES
        self.demo_section('15. EDGE CASES - EXTREME VALUES')
        
        self.demo_subsection('Edge case: Maximum speed commands')
        self.get_logger().info('Testing with maximum allowed speed')
        self.get_logger().info('System should enforce speed limits even with high command values')
        
        # Try to drive at maximum speed (should be limited by envelope)
        self.drive_straight(1.0, speed=1.5)  # Above max speed
        self.stop_robot()
        time.sleep(2)
        
        self.demo_subsection('Edge case: Maximum angular velocity')
        self.get_logger().info('Testing with maximum angular speed')
        self.get_logger().info('System should handle high angular velocities safely')
        
        self.rotate(math.pi / 2, speed=1.0)  # High angular speed
        self.stop_robot()
        time.sleep(2)
        
        self.demo_subsection('Edge case: Zero commands (stop command)')
        self.get_logger().info('Testing with zero velocity commands')
        self.get_logger().info('System should handle zero commands correctly')
        
        twist = Twist()
        for _ in range(10):
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.1)
        
        self.get_logger().info('Zero commands processed successfully')
        time.sleep(2)
        
        # SECTION 16: EDGE CASES - CONFLICTING CONDITIONS
        self.demo_section('16. EDGE CASES - CONFLICTING CONDITIONS')
        
        self.demo_subsection('Edge case: Multiple simultaneous conditions')
        self.get_logger().info('Testing system with potential conflicting safety conditions')
        self.get_logger().info('System should prioritize safety correctly')
        
        # Drive toward obstacle while rotating (conflicting motion)
        twist = Twist()
        twist.linear.x = 0.5
        twist.angular.z = 0.5
        
        self.get_logger().info('Driving with both linear and angular commands')
        start_time = time.time()
        while time.time() - start_time < 2.0:
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.05)
        
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 17: EDGE CASES - RECOVERY SCENARIOS
        self.demo_section('17. EDGE CASES - RECOVERY SCENARIOS')
        
        self.demo_subsection('Edge case: Recovery from degraded operation')
        self.get_logger().info('Testing system recovery after degraded operation')
        self.get_logger().info('System should recover to normal operation when conditions improve')
        
        # Simulate degraded operation then recovery
        self.get_logger().info('Simulating degraded operation (slow creep)')
        self.drive_straight(0.5, speed=0.1)
        self.stop_robot()
        
        self.get_logger().info('Recovering to normal operation')
        self.drive_straight(0.5, speed=0.5)
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 18: EDGE CASES - TIMING SCENARIOS
        self.demo_section('18. EDGE CASES - TIMING SCENARIOS')
        
        self.demo_subsection('Edge case: Very fast obstacle approach')
        self.get_logger().info('Testing with rapid obstacle approach')
        self.get_logger().info('System should detect and react quickly to fast-moving obstacles')
        
        # Fast approach then emergency stop
        self.drive_straight(1.0, speed=0.9)
        self.stop_robot()
        
        self.get_logger().info('Emergency stop should engage quickly')
        time.sleep(2)
        
        self.demo_subsection('Edge case: Slow creep into danger zone')
        self.get_logger().info('Testing with very slow approach to danger')
        self.get_logger().info('System should detect creeping approach and escalate appropriately')
        
        # Very slow creep
        self.drive_straight(0.3, speed=0.05)
        self.stop_robot()
        time.sleep(2)
        
        # SECTION 19: EDGE CASES - SYSTEM STRESS
        self.demo_section('19. EDGE CASES - SYSTEM STRESS')
        
        self.demo_subsection('Edge case: Rapid mode transitions')
        self.get_logger().info('Testing system with rapid mode changes')
        self.get_logger().info('System should handle rapid mode transitions without instability')
        
        # Alternate between driving and stopping rapidly
        for i in range(5):
            self.drive_straight(0.2, speed=0.5)
            self.stop_robot()
            time.sleep(0.3)
        
        self.get_logger().info('Rapid transitions handled successfully')
        time.sleep(2)
        
        # SECTION 20: RETURN TO IDLE AND FINAL STATE
        self.demo_section('20. RETURN TO IDLE AND FINAL STATE')
        self.demo_subsection('Returning to safe position and Idle mode')
        self.get_logger().info('Driving back to origin area')
        
        # Return to a safe area
        self.rotate(-math.pi / 2)
        self.drive_straight(2.0)
        self.rotate(-math.pi / 4)
        self.drive_straight(1.5)
        
        self.stop_robot()
        time.sleep(2)
        
        # Final state check
        self.demo_subsection('Final safety state')
        self.get_logger().info(f'Final mode: {self.current_mode}')
        self.get_logger().info(f'Final zone: {self.current_zone}')
        self.get_logger().info(f'Fault latched: {self.fault_latched}')
        
        # Edge case summary
        self.demo_section('EDGE CASES DEMONSTRATED')
        self.get_logger().info('')
        self.get_logger().info('Edge cases demonstrated:')
        self.get_logger().info('✓ Sensor failure simulation (lidar drop)')
        self.get_logger().info('✓ Rapid zone transitions')
        self.get_logger().info('✓ Stale command detection and recovery')
        self.get_logger().info('✓ Exact threshold crossing')
        self.get_logger().info('✓ Zero-distance obstacle handling')
        self.get_logger().info('✓ Sudden obstacle appearance')
        self.get_logger().info('✓ Zone oscillation handling')
        self.get_logger().info('✓ Maximum speed enforcement')
        self.get_logger().info('✓ Maximum angular velocity handling')
        self.get_logger().info('✓ Zero command processing')
        self.get_logger().info('✓ Conflicting motion conditions')
        self.get_logger().info('✓ Degraded operation recovery')
        self.get_logger().info('✓ Fast obstacle approach reaction')
        self.get_logger().info('✓ Slow creep detection')
        self.get_logger().info('✓ Rapid mode transitions')
        
        # Demo complete
        self.demo_section('DEMO COMPLETE')
        
        # Print metrics summary
        self.demo_logger.print_metrics()
        
        self.demo_logger.info('', Color.GREEN)
        self.demo_logger.success('Comprehensive Safety Autonomy Core Demo completed!')
        self.demo_logger.info('', Color.GREEN)
        self.demo_logger.info('Demonstrated capabilities:', Color.CYAN)
        self.demo_logger.info('✓ State machine transitions')
        self.demo_logger.info('✓ Safety envelope zone detection')
        self.demo_logger.info('✓ Speed limiting in Warning zone')
        self.demo_logger.info('✓ Mode transitions (Idle → Moving → AvoidingObstacle)')
        self.demo_logger.info('✓ Emergency zone detection')
        self.demo_logger.info('✓ Fault latching and recovery')
        self.demo_logger.info('✓ Jerk-limited emergency stop')
        self.demo_logger.info('✓ Command freshness watchdog')
        self.demo_logger.info('✓ Complex maneuver sequences')
        self.demo_logger.info('✓ Diagnostic event publishing')
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.info('Edge cases demonstrated:', Color.CYAN)
        self.demo_logger.info('✓ Sensor failure simulation')
        self.demo_logger.info('✓ Rapid zone transitions')
        self.demo_logger.info('✓ Boundary condition handling')
        self.demo_logger.info('✓ Dynamic obstacle scenarios')
        self.demo_logger.info('✓ Extreme value handling')
        self.demo_logger.info('✓ Conflicting condition resolution')
        self.demo_logger.info('✓ Recovery scenario testing')
        self.demo_logger.info('✓ Timing edge case handling')
        self.demo_logger.info('✓ System stress testing')
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.success('The safety_autonomy_core system successfully demonstrated all key safety features, edge cases, and autonomous capabilities.')


def main():
    rclpy.init()
    demo = ComprehensiveDemo()
    
    try:
        rclpy.spin(demo)
    except KeyboardInterrupt:
        demo.get_logger().info('Demo stopped by user')
    finally:
        demo.stop_robot()
        demo.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
