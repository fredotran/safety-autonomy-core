"""Modular EKF localization stack launch component.

This launch file provides the EKF multi-sensor fusion localization stack
as a reusable component that can be included in other launch files.

The stack follows the two-EKF + navsat_transform pattern:
  * ekf_filter_node       — fuses /odom + /imu, owns odom→base_link TF.
  * navsat_transform_node — converts GPS to map-frame odometry.
  * ekf_filter_node_map   — fuses /odom + /imu + /odometry/gps, owns map→odom TF.

Usage:
    from launch import LaunchDescription
    from launch.actions import IncludeLaunchDescription
    from launch.launch_description_sources import PythonLaunchDescriptionSource
    from launch.substitutions import LaunchConfiguration
    
    ekf_stack = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([get_package_share_directory('safety_core_bringup'), 'launch', 'ekf_stack.launch.py']),
        launch_arguments={
            'ekf_config': LaunchConfiguration('ekf_config'),
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'enable_gps': LaunchConfiguration('enable_gps'),
        }.items()
    )
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, LogInfo
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    """Generate EKF localization stack component."""
    ekf_config = LaunchConfiguration('ekf_config')
    use_sim_time = LaunchConfiguration('use_sim_time')
    enable_gps = LaunchConfiguration('enable_gps')

    # Declare launch arguments
    declare_ekf_config = DeclareLaunchArgument(
        'ekf_config',
        default_value=os.path.join(get_package_share_directory('safety_core_bringup'), 'config', 'ekf_config.yaml'),
        description='Path to EKF configuration YAML file.'
    )
    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation clock from /clock topic.'
    )
    declare_enable_gps = DeclareLaunchArgument(
        'enable_gps',
        default_value='true',
        description='Enable navsat_transform_node and global EKF for GPS integration.'
    )

    # Local EKF: odom -> base_link (always launched)
    ekf_local = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[ekf_config, {'use_sim_time': use_sim_time}],
        remappings=[('odometry/filtered', '/odometry/filtered')],
    )

    # GPS covariance adapter (launched conditionally with GPS)
    gps_adapter = Node(
        package='safety_core_ros',
        executable='gps_covariance_adapter_node',
        name='gps_covariance_adapter_node',
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
        condition=IfCondition(enable_gps),
    )

    # GPS -> map-frame Odometry (launched conditionally)
    # Uses /gps_adapted (from gps_covariance_adapter_node) for quality-scaled covariance
    navsat_transform = Node(
        package='robot_localization',
        executable='navsat_transform_node',
        name='navsat_transform',
        output='screen',
        parameters=[ekf_config, {'use_sim_time': use_sim_time}],
        remappings=[
            ('imu/data', '/imu'),
            ('gps/fix', '/gps_adapted'),
            ('odometry/filtered', '/odometry/filtered'),
            ('odometry/gps', '/odometry/gps'),
            ('gps/filtered', '/gps/filtered'),
        ],
        condition=IfCondition(enable_gps),
    )

    # Global EKF: map -> odom (launched conditionally with GPS)
    ekf_global = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node_map',
        output='screen',
        parameters=[ekf_config, {'use_sim_time': use_sim_time}],
        remappings=[('odometry/filtered', '/odometry/filtered_map')],
        condition=IfCondition(enable_gps),
    )

    # Group the EKF nodes
    ekf_stack = GroupAction(
        actions=[
            LogInfo(msg='Starting EKF localization stack (robot_localization)...'),
            ekf_local,
            gps_adapter,
            navsat_transform,
            ekf_global,
        ]
    )

    return LaunchDescription([
        declare_ekf_config,
        declare_use_sim_time,
        declare_enable_gps,
        ekf_stack,
    ])