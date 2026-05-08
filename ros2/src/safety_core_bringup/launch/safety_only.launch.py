"""Brings up only the safety_core wrapper nodes (envelope, supervisor, drive
bridge) with externally-supplied /scan, /odom, and /cmd_vel_nav topics.

Useful for:
  * Bench testing on a physical AGV that already publishes its own sensor stack.
  * Replaying ROS bags through the safety_core gating layer.
  * Safety stack testing with simulation (without Nav2/SLAM)
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, TimerAction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_bringup = get_package_share_directory("safety_core_bringup")
    safety_params = os.path.join(pkg_bringup, "config", "safety_params.yaml")
    rviz_config = os.path.join(pkg_bringup, "rviz", "safety_simple.rviz")  # Use simpler config

    use_sim_time = LaunchConfiguration("use_sim_time")
    use_rviz = LaunchConfiguration("rviz")
    declare_use_sim_time = DeclareLaunchArgument("use_sim_time", default_value="false")
    declare_rviz = DeclareLaunchArgument("rviz", default_value="true")

    return LaunchDescription(
        [
            declare_use_sim_time,
            declare_rviz,
            Node(
                package="safety_core_ros",
                executable="safety_envelope_node",
                name="safety_envelope_node",
                output="screen",
                parameters=[safety_params, {"use_sim_time": use_sim_time}],
            ),
            Node(
                package="safety_core_ros",
                executable="safety_supervisor_node",
                name="safety_supervisor_node",
                output="screen",
                parameters=[safety_params, {"use_sim_time": use_sim_time}],
            ),
            Node(
                package="safety_core_ros",
                executable="safety_drive_bridge_node",
                name="safety_drive_bridge_node",
                output="screen",
                parameters=[safety_params, {"use_sim_time": use_sim_time}],
                remappings=[
                    ("cmd_vel_nav", "/cmd_vel_nav"),
                    ("cmd_vel", "/cmd_vel"),
                ],
            ),
            TimerAction(period=2.0, actions=[  # Delay RViz by 2 seconds
                Node(
                    package="rviz2",
                    executable="rviz2",
                    arguments=["-d", rviz_config],
                    output="screen",
                    parameters=[{"use_sim_time": use_sim_time}],
                    condition=IfCondition(use_rviz),
                ),
            ]),
        ]
    )
