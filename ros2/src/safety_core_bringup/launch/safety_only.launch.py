"""Brings up only the safety_core wrapper nodes (envelope, supervisor, drive
bridge) with externally-supplied /scan, /odom, and /cmd_vel_nav topics.

Useful for:
  * Bench testing on a physical AGV that already publishes its own sensor stack.
  * Replaying ROS bags through the safety_core gating layer.
  * Safety stack testing with simulation (without Nav2/SLAM)

This launch file now uses the modular safety_stack.launch.py component for
reusability and maintainability.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    pkg_bringup = get_package_share_directory('safety_core_bringup')
    safety_params = os.path.join(pkg_bringup, 'config', 'safety_params.yaml')
    rviz_config = os.path.join(pkg_bringup, 'rviz', 'safety_simple.rviz')

    use_sim_time = LaunchConfiguration('use_sim_time')
    use_rviz = LaunchConfiguration('rviz')

    # Declare launch arguments
    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false',
        description='Use simulation clock from /clock topic.'
    )
    declare_rviz = DeclareLaunchArgument(
        'rviz',
        default_value='true',
        description='Launch RViz with the demo configuration.'
    )

    # Include the modular safety stack component with Nav2 remappings
    safety_stack = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_bringup, 'launch', 'safety_stack.launch.py')),
        launch_arguments={
            'safety_params': safety_params,
            'use_sim_time': use_sim_time,
            'include_remappings': 'true',  # Enable Nav2 cmd_vel remappings
        }.items()
    )

    # Include the modular RViz component
    rviz = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_bringup, 'launch', 'rviz_component.launch.py')),
        launch_arguments={
            'rviz_config': rviz_config,
            'use_sim_time': use_sim_time,
            'enable_rviz': use_rviz,
            'delay_s': '2.0',
        }.items()
    )

    return LaunchDescription([
        declare_use_sim_time,
        declare_rviz,
        safety_stack,
        rviz,
    ])
