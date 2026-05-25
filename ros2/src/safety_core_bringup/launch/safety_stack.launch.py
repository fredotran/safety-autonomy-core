"""Modular safety stack launch component.

This launch file provides the safety_core wrapper nodes (envelope, supervisor, drive_bridge)
as a reusable component that can be included in other launch files.

Usage:
    from launch import LaunchDescription
    from launch.actions import IncludeLaunchDescription
    from launch.launch_description_sources import PythonLaunchDescriptionSource
    from launch.substitutions import LaunchConfiguration
    
    safety_stack = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([get_package_share_directory('safety_core_bringup'), 'launch', 'safety_stack.launch.py']),
        launch_arguments={
            'safety_params': LaunchConfiguration('safety_params'),
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'include_remappings': LaunchConfiguration('include_remappings'),
        }.items()
    )
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, LogInfo
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node


def generate_launch_description():
    """Generate safety stack component."""
    safety_params = LaunchConfiguration('safety_params')
    use_sim_time = LaunchConfiguration('use_sim_time')
    include_remappings = LaunchConfiguration('include_remappings')

    # Declare launch arguments
    declare_safety_params = DeclareLaunchArgument(
        'safety_params',
        default_value=os.path.join(get_package_share_directory('safety_core_bringup'), 'config', 'safety_params.yaml'),
        description='Path to safety parameters YAML file.'
    )
    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation clock from /clock topic.'
    )
    declare_include_remappings = DeclareLaunchArgument(
        'include_remappings',
        default_value='true',
        description='Include cmd_vel topic remappings for Nav2 integration.'
    )

    # Create remappings for Nav2 integration
    nav2_remappings = [
        ("cmd_vel_nav", "/cmd_vel_nav"),
        ("cmd_vel", "/cmd_vel"),
    ]

    # Safety envelope node
    safety_envelope = Node(
        package='safety_core_ros',
        executable='safety_envelope_node',
        name='safety_envelope_node',
        output='screen',
        parameters=[safety_params, {'use_sim_time': use_sim_time}],
    )

    # Safety supervisor node
    safety_supervisor = Node(
        package='safety_core_ros',
        executable='safety_supervisor_node',
        name='safety_supervisor_node',
        output='screen',
        parameters=[safety_params, {'use_sim_time': use_sim_time}],
    )

    # Safety drive bridge node with Nav2 remappings
    safety_drive_bridge_with_remappings = Node(
        package='safety_core_ros',
        executable='safety_drive_bridge_node',
        name='safety_drive_bridge_node',
        output='screen',
        parameters=[safety_params, {'use_sim_time': use_sim_time}],
        remappings=nav2_remappings,
        condition=IfCondition(include_remappings),
    )

    # Safety drive bridge node without Nav2 remappings
    safety_drive_bridge_without_remappings = Node(
        package='safety_core_ros',
        executable='safety_drive_bridge_node',
        name='safety_drive_bridge_node',
        output='screen',
        parameters=[safety_params, {'use_sim_time': use_sim_time}],
        condition=IfCondition(PythonExpression(["'", include_remappings, "' == 'false'"])),
    )

    # Sensor monitor node
    sensor_monitor = Node(
        package='safety_core_ros',
        executable='sensor_monitor_node',
        name='sensor_monitor_node',
        output='screen',
        parameters=[safety_params, {'use_sim_time': use_sim_time}],
    )

    # Group the safety nodes
    safety_stack = GroupAction(
        actions=[
            LogInfo(msg='Starting safety_core wrapper stack...'),
            sensor_monitor,
            safety_envelope,
            safety_supervisor,
            safety_drive_bridge_with_remappings,
            safety_drive_bridge_without_remappings,
        ]
    )

    return LaunchDescription([
        declare_safety_params,
        declare_use_sim_time,
        declare_include_remappings,
        safety_stack,
    ])