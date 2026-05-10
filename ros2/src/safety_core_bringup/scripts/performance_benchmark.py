#!/usr/bin/env python3
"""
Performance Benchmark for Safety Autonomy Core.

Measures and reports performance characteristics of the safety system:
- Message latency
- Processing time
- CPU/memory usage
- Throughput metrics
"""

import os
import sys
import threading
import time

import psutil
import rclpy
from rclpy.node import Node
from safety_core_msgs.msg import EnvelopeStatus, SafetyState
from sensor_msgs.msg import LaserScan

# Add demo utils to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from demo_utils import Color, DemoLogger


class PerformanceBenchmark(Node):
    """Performance benchmark for safety autonomy core system."""

    def __init__(self):
        """Initialize the performance benchmark node."""
        super().__init__('performance_benchmark')
        
        # Initialize demo logger
        self.demo_logger = DemoLogger(self.get_logger())
        
        # Subscribers
        self.safety_state_sub = self.create_subscription(
            SafetyState, '/safety/state', self.safety_state_callback, 10)
        self.envelope_status_sub = self.create_subscription(
            EnvelopeStatus, '/safety/envelope_status', self.envelope_status_callback, 10)
        self.scan_sub = self.create_subscription(
            LaserScan, '/scan', self.scan_callback, 10)
        
        # Performance metrics
        self.metrics = {
            'safety_state_latency': [],
            'envelope_status_latency': [],
            'scan_latency': [],
            'safety_state_count': 0,
            'envelope_status_count': 0,
            'scan_count': 0,
            'start_time': time.time(),
            'cpu_samples': [],
            'memory_samples': []
        }
        
        self.current_cpu = psutil.cpu_percent()
        self.current_memory = psutil.virtual_memory().percent
        
        self.demo_logger.section('PERFORMANCE BENCHMARK')
        self.demo_logger.info('Collecting performance metrics for safety system...', Color.CYAN)
        self.demo_logger.info('Benchmark will run for 30 seconds', Color.YELLOW)
        
        # Start benchmark timer
        self.benchmark_duration = 30.0
        self.benchmark_start = time.time()
        
        # Start CPU/memory monitoring thread
        self.monitoring_thread = None
        self.start_monitoring()
    
    def safety_state_callback(self, msg):
        """Measure safety state message latency."""
        current_time = time.time()
        msg_time = msg.header.stamp.sec + msg.header.stamp.nanosec / 1e9
        latency = (current_time - self.benchmark_start) - (msg_time - self.benchmark_start)
        
        self.metrics['safety_state_latency'].append(latency)
        self.metrics['safety_state_count'] += 1
    
    def envelope_status_callback(self, msg):
        """Measure envelope status message latency."""
        current_time = time.time()
        msg_time = msg.header.stamp.sec + msg.header.stamp.nanosec / 1e9
        latency = (current_time - self.benchmark_start) - (msg_time - self.benchmark_start)
        
        self.metrics['envelope_status_latency'].append(latency)
        self.metrics['envelope_status_count'] += 1
    
    def scan_callback(self, msg):
        """Measure scan message latency."""
        current_time = time.time()
        msg_time = msg.header.stamp.sec + msg.header.stamp.nanosec / 1e9
        latency = (current_time - self.benchmark_start) - (msg_time - self.benchmark_start)
        
        self.metrics['scan_latency'].append(latency)
        self.metrics['scan_count'] += 1
    
    def start_monitoring(self):
        """Start CPU/memory monitoring thread."""
        self.monitoring_thread = threading.Thread(target=self.monitor_resources)
        self.monitoring_thread.daemon = True
        self.monitoring_thread.start()
    
    def monitor_resources(self):
        """Monitor CPU and memory usage."""
        while time.time() - self.benchmark_start < self.benchmark_duration:
            cpu = psutil.cpu_percent()
            memory = psutil.virtual_memory().percent
            self.metrics['cpu_samples'].append(cpu)
            self.metrics['memory_samples'].append(memory)
            time.sleep(1.0)
    
    def calculate_statistics(self, data):
        """Calculate statistics for a list of values."""
        if not data:
            return {'min': 0, 'max': 0, 'avg': 0, 'count': 0}
        
        return {
            'min': min(data),
            'max': max(data),
            'avg': sum(data) / len(data),
            'count': len(data)
        }
    
    def print_results(self):
        """Print benchmark results."""
        duration = time.time() - self.benchmark_start
        
        self.demo_logger.section('PERFORMANCE BENCHMARK RESULTS')
        self.demo_logger.info(f'Benchmark duration: {duration:.1f} seconds', Color.BOLD)
        
        # Message rates
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.info('Message Rates:', Color.BOLD)
        safety_state_rate = self.metrics['safety_state_count'] / duration
        envelope_status_rate = self.metrics['envelope_status_count'] / duration
        scan_rate = self.metrics['scan_count'] / duration
        
        self.demo_logger.info(f'Safety State: {safety_state_rate:.1f} Hz', Color.CYAN)
        self.demo_logger.info(f'Envelope Status: {envelope_status_rate:.1f} Hz', Color.CYAN)
        self.demo_logger.info(f'Scan: {scan_rate:.1f} Hz', Color.CYAN)
        
        # Latency statistics
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.info('Message Latency (seconds):', Color.BOLD)
        
        safety_state_stats = self.calculate_statistics(self.metrics['safety_state_latency'])
        self.demo_logger.info(f'Safety State - Min: {safety_state_stats["min"]*1000:.2f}ms, '
                            f'Max: {safety_state_stats["max"]*1000:.2f}ms, '
                            f'Avg: {safety_state_stats["avg"]*1000:.2f}ms', Color.CYAN)
        
        envelope_stats = self.calculate_statistics(self.metrics['envelope_status_latency'])
        self.demo_logger.info(f'Envelope Status - Min: {envelope_stats["min"]*1000:.2f}ms, '
                            f'Max: {envelope_stats["max"]*1000:.2f}ms, '
                            f'Avg: {envelope_stats["avg"]*1000:.2f}ms', Color.CYAN)
        
        scan_stats = self.calculate_statistics(self.metrics['scan_latency'])
        self.demo_logger.info(f'Scan - Min: {scan_stats["min"]*1000:.2f}ms, '
                            f'Max: {scan_stats["max"]*1000:.2f}ms, '
                            f'Avg: {scan_stats["avg"]*1000:.2f}ms', Color.CYAN)
        
        # Resource usage
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.info('Resource Usage:', Color.BOLD)
        
        cpu_stats = self.calculate_statistics(self.metrics['cpu_samples'])
        memory_stats = self.calculate_statistics(self.metrics['memory_samples'])
        
        self.demo_logger.info(f'CPU - Min: {cpu_stats["min"]:.1f}%, '
                            f'Max: {cpu_stats["max"]:.1f}%, '
                            f'Avg: {cpu_stats["avg"]:.1f}%', Color.CYAN)
        
        self.demo_logger.info(f'Memory - Min: {memory_stats["min"]:.1f}%, '
                            f'Max: {memory_stats["max"]:.1f}%, '
                            f'Avg: {memory_stats["avg"]:.1f}%', Color.CYAN)
        
        # Performance assessment
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.info('Performance Assessment:', Color.BOLD)
        
        # Assess latency
        avg_scan_latency = scan_stats['avg'] * 1000
        if avg_scan_latency < 10:
            self.demo_logger.success('✓ Excellent latency (< 10ms)')
        elif avg_scan_latency < 50:
            self.demo_logger.info('✓ Good latency (< 50ms)', Color.YELLOW)
        else:
            self.demo_logger.error('✗ High latency (> 50ms)')
        
        # Assess CPU usage
        avg_cpu = cpu_stats['avg']
        if avg_cpu < 20:
            self.demo_logger.success('✓ Low CPU usage (< 20%)')
        elif avg_cpu < 50:
            self.demo_logger.info('✓ Moderate CPU usage (< 50%)', Color.YELLOW)
        else:
            self.demo_logger.error('✗ High CPU usage (> 50%)')
        
        # Assess memory usage
        avg_memory = memory_stats['avg']
        if avg_memory < 30:
            self.demo_logger.success('✓ Low memory usage (< 30%)')
        elif avg_memory < 60:
            self.demo_logger.info('✓ Moderate memory usage (< 60%)', Color.YELLOW)
        else:
            self.demo_logger.error('✗ High memory usage (> 60%)')
        
        # Assess throughput
        if scan_rate > 10:
            self.demo_logger.success('✓ High scan throughput (> 10 Hz)')
        elif scan_rate > 5:
            self.demo_logger.info('✓ Moderate scan throughput (> 5 Hz)', Color.YELLOW)
        else:
            self.demo_logger.error('✗ Low scan throughput (< 5 Hz)')
    
    def run_benchmark(self):
        """Run the benchmark."""
        self.demo_logger.info('Benchmark running...', Color.YELLOW)
        
        # Wait for benchmark duration
        while time.time() - self.benchmark_start < self.benchmark_duration:
            rclpy.spin_once(timeout_sec=0.1)
        
        # Print results
        self.print_results()
        
        self.demo_logger.success('Benchmark completed successfully')


def main():
    rclpy.init()
    benchmark = PerformanceBenchmark()
    
    try:
        benchmark.run_benchmark()
    except KeyboardInterrupt:
        benchmark.demo_logger.warn('Benchmark interrupted')
    finally:
        benchmark.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
