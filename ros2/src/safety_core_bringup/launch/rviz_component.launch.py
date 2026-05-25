"""Modular RViz launch component.

This launch file provides RViz as a reusable component with delayed startup
that can be included in other launch files.

Usage:
    from launch import LaunchDescription
    from launch.actions import IncludeLaunchDescription
    from launch.launch_description_sources import PythonLaunchDescriptionSource
    from launch.substitutions import LaunchConfiguration
    
    rviz = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([get_package_share_directory('safety_core_bringup'), 'launch', 'rviz_component.launch.py']),
        launch_arguments={
            'rviz_config': LaunchConfiguration('rviz_config'),
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'enable_rviz': LaunchConfiguration('enable_rviz'),
            'delay_s': LaunchConfiguration('delay_s'),
        }.items()
    )
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, TimerAction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    """Generate RViz component with delayed startup."""
    rviz_config = LaunchConfiguration('rviz_config')
    use_sim_time = LaunchConfiguration('use_sim_time')
    enable_rviz = LaunchConfiguration('enable_rviz')
    delay_s = LaunchConfiguration('delay_s')

    # Declare launch arguments
    declare_rviz_config = DeclareLaunchArgument(
        'rviz_config',
        description='Path to RViz configuration file (.rviz).'
    )
    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation clock from /clock topic.'
    )
    declare_enable_rviz = DeclareLaunchArgument(
        'enable_rviz',
        default_value='true',
        description='Enable RViz launch.'
    )
    declare_delay_s = DeclareLaunchArgument(
        'delay_s',
        default_value='3.0',
        description='Delay RViz startup by this many seconds.'
    )

    # RViz node with conditional launching and delay
    rviz = Node(
        package='rviz2',
        executable='rviz2',
        arguments=['-d', rviz_config],
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
        condition=IfCondition(enable_rviz),
    )

    # Delay RViz startup
    delayed_rviz = TimerAction(period=delay_s, actions=[rviz])

    return LaunchDescription([
        declare_rviz_config,
        declare_use_sim_time,
        declare_enable_rviz,
        declare_delay_s,
        delayed_rviz,
    ])