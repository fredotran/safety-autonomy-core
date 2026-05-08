#!/usr/bin/env python3
"""
Demo utilities for safety autonomy core demos.

Provides colored console output, metrics tracking, and common demo functions.
"""

import time
from enum import Enum


class Color(Enum):
    """ANSI color codes for console output."""
    RESET = '\033[0m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'


class DemoLogger:
    """Enhanced logger with colored output and metrics tracking."""
    
    def __init__(self, node_logger):
        self.logger = node_logger
        self.start_time = time.time()
        self.metrics = {
            'zone_transitions': [],
            'mode_transitions': [],
            'safe_stop_events': [],
            'fault_events': [],
        }
        self.last_zone = None
        self.last_mode = None
    
    def info(self, message, color=None):
        """Log info message with optional color."""
        if color:
            print(f"{color.value}{message}{Color.RESET.value}")
        else:
            self.logger.info(message)
    
    def warn(self, message):
        """Log warning message with yellow color."""
        print(f"{Color.YELLOW.value}{message}{Color.RESET.value}")
    
    def error(self, message):
        """Log error message with red color."""
        print(f"{Color.RED.value}{message}{Color.RESET.value}")
    
    def success(self, message):
        """Log success message with green color."""
        print(f"{Color.GREEN.value}{message}{Color.RESET.value}")
    
    def section(self, title):
        """Print a section header."""
        self.info('', Color.CYAN)
        self.info('=' * 60, Color.CYAN)
        self.info(f'  {title}', Color.BOLD)
        self.info('=' * 60, Color.CYAN)
        self.info('', Color.CYAN)
    
    def subsection(self, title):
        """Print a subsection header."""
        self.info('', Color.BLUE)
        self.info(f'>>> {title}', Color.BOLD)
        self.info('', Color.BLUE)
    
    def track_zone_transition(self, from_zone, to_zone):
        """Track a zone transition."""
        zone_names = ['Clear', 'Warning', 'Protective', 'Emergency']
        from_name = zone_names[from_zone] if from_zone < len(zone_names) else 'Unknown'
        to_name = zone_names[to_zone] if to_zone < len(zone_names) else 'Unknown'
        
        transition_time = time.time() - self.start_time
        self.metrics['zone_transitions'].append({
            'from': from_name,
            'to': to_name,
            'time': transition_time
        })
        
        self.info(f'[Zone Transition] {from_name} -> {to_name} at {transition_time:.1f}s', Color.YELLOW)
        self.last_zone = to_zone
    
    def track_mode_transition(self, from_mode, to_mode):
        """Track a mode transition."""
        mode_names = ['Init', 'Idle', 'Moving', 'Degraded', 'AvoidingObstacle', 
                     'LocalizationLost', 'Docking', 'SafeStop']
        from_name = mode_names[from_mode] if from_mode < len(mode_names) else 'Unknown'
        to_name = mode_names[to_mode] if to_mode < len(mode_names) else 'Unknown'
        
        transition_time = time.time() - self.start_time
        self.metrics['mode_transitions'].append({
            'from': from_name,
            'to': to_name,
            'time': transition_time
        })
        
        self.info(f'[Mode Transition] {from_name} -> {to_name} at {transition_time:.1f}s', Color.MAGENTA)
        self.last_mode = to_mode
    
    def track_safe_stop(self, active):
        """Track a safe stop event."""
        if active:
            event_time = time.time() - self.start_time
            self.metrics['safe_stop_events'].append({'time': event_time})
            self.warn(f'[Safe Stop] ENGAGED at {event_time:.1f}s')
    
    def track_fault(self, latched):
        """Track a fault event."""
        if latched:
            event_time = time.time() - self.start_time
            self.metrics['fault_events'].append({'time': event_time})
            self.error(f'[Fault] LATCHED at {event_time:.1f}s')
    
    def print_metrics(self):
        """Print collected metrics."""
        total_time = time.time() - self.start_time
        
        self.section('Demo Metrics Summary')
        self.info(f'Total duration: {total_time:.1f} seconds', Color.BOLD)
        self.info(f'Zone transitions: {len(self.metrics["zone_transitions"])}')
        self.info(f'Mode transitions: {len(self.metrics["mode_transitions"])}')
        self.info(f'Safe stop events: {len(self.metrics["safe_stop_events"])}')
        self.info(f'Fault events: {len(self.metrics["fault_events"])}')
        
        if self.metrics['zone_transitions']:
            self.info('', Color.BLUE)
            self.info('Zone transition history:', Color.BOLD)
            for i, transition in enumerate(self.metrics['zone_transitions'], 1):
                self.info(f'  {i}. {transition["from"]} -> {transition["to"]} at {transition["time"]:.1f}s')
        
        if self.metrics['mode_transitions']:
            self.info('', Color.BLUE)
            self.info('Mode transition history:', Color.BOLD)
            for i, transition in enumerate(self.metrics['mode_transitions'], 1):
                self.info(f'  {i}. {transition["from"]} -> {transition["to"]} at {transition["time"]:.1f}s')
        
        if self.metrics['safe_stop_events']:
            self.info('', Color.BLUE)
            self.info('Safe stop events:', Color.BOLD)
            for i, event in enumerate(self.metrics['safe_stop_events'], 1):
                self.info(f'  {i}. At {event["time"]:.1f}s')
        
        if self.metrics['fault_events']:
            self.info('', Color.BLUE)
            self.info('Fault events:', Color.BOLD)
            for i, event in enumerate(self.metrics['fault_events'], 1):
                self.info(f'  {i}. At {event["time"]:.1f}s')


class MetricsDisplay:
    """Real-time metrics display for demos."""
    
    def __init__(self, demo_logger):
        self.logger = demo_logger
        self.update_interval = 1.0  # Update every second
        self.last_update = 0
    
    def update(self, current_zone, current_mode, fault_latched, safe_stop, 
               distance=None, speed_limit=None):
        """Update the metrics display."""
        current_time = time.time()
        if current_time - self.last_update < self.update_interval:
            return
        
        self.last_update = current_time
        
        zone_names = ['Clear', 'Warning', 'Protective', 'Emergency']
        mode_names = ['Init', 'Idle', 'Moving', 'Degraded', 'AvoidingObstacle', 
                     'LocalizationLost', 'Docking', 'SafeStop']
        
        zone_name = zone_names[current_zone] if current_zone < len(zone_names) else 'Unknown'
        mode_name = mode_names[current_mode] if current_mode < len(mode_names) else 'Unknown'
        
        # Build metrics string
        metrics = []
        metrics.append(f'Time: {current_time - self.logger.start_time:.1f}s')
        metrics.append(f'Zone: {zone_name}')
        metrics.append(f'Mode: {mode_name}')
        metrics.append(f'Fault: {"LATCHED" if fault_latched else "Clear"}')
        metrics.append(f'SafeStop: {"ACTIVE" if safe_stop else "Inactive"}')
        
        if distance is not None:
            metrics.append(f'Distance: {distance:.2f}m')
        
        if speed_limit is not None:
            metrics.append(f'SpeedLimit: {speed_limit:.2f}m/s')
        
        # Print with color coding
        zone_color = Color.GREEN
        if current_zone == 1:
            zone_color = Color.YELLOW
        elif current_zone == 2:
            zone_color = Color.MAGENTA
        elif current_zone == 3:
            zone_color = Color.RED
        
        mode_color = Color.GREEN
        if current_mode == 4:  # AvoidingObstacle
            mode_color = Color.YELLOW
        elif current_mode == 7:  # SafeStop
            mode_color = Color.RED
        
        # Print compact metrics line
        metrics_str = ' | '.join(metrics)
        print(f"\r{Color.CYAN.value}[METRICS]{Color.RESET.value} {metrics_str}", end='', flush=True)
