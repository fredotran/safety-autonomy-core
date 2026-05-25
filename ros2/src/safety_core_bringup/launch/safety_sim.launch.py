"""Safety stack + Gazebo simulation (no Nav2 / no SLAM).

Useful for:
  * Iterating on safety_envelope / supervisor / drive_bridge against the warehouse SDF
    without paying the Nav2 / SLAM startup cost.
  * Visually validating the safety zones in RViz with /scan, /odom, and /tf only.

Equivalent to ``agv_warehouse.launch.py nav2:=false slam:=false rviz:=true`` but
with a leaner RViz config (``safety_simple.rviz``) and shorter startup.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    GroupAction,
    IncludeLaunchDescription,
    LogInfo,
    TimerAction,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_sim = get_package_share_directory("safety_core_sim")
    pkg_bringup = get_package_share_directory("safety_core_bringup")

    use_sim_time = LaunchConfiguration("use_sim_time")
    use_rviz = LaunchConfiguration("rviz")

    safety_params = os.path.join(pkg_bringup, "config", "safety_params.yaml")
    rviz_config = os.path.join(pkg_bringup, "rviz", "safety_simple.rviz")
    sim_launch = os.path.join(pkg_sim, "launch", "sim_only.launch.py")

    declare_use_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true", description="Use simulation time from /clock topic.")
    declare_rviz = DeclareLaunchArgument("rviz", default_value="true", description="Launch RViz with the demo configuration.")

    sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(sim_launch),
        launch_arguments={"use_sim_time": use_sim_time}.items(),
    )

    safety_envelope = Node(
        package="safety_core_ros",
        executable="safety_envelope_node",
        name="safety_envelope_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
    )

    safety_supervisor = Node(
        package="safety_core_ros",
        executable="safety_supervisor_node",
        name="safety_supervisor_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
    )

    safety_drive_bridge = Node(
        package="safety_core_ros",
        executable="safety_drive_bridge_node",
        name="safety_drive_bridge_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
        remappings=[
            ("cmd_vel_nav", "/cmd_vel_nav"),
            ("cmd_vel", "/cmd_vel"),
        ],
    )

    safety_stack = GroupAction(
        actions=[
            LogInfo(msg="Starting safety_core wrapper stack (no Nav2)..."),
            safety_envelope,
            safety_supervisor,
            safety_drive_bridge,
        ]
    )

    rviz = Node(
        package="rviz2",
        executable="rviz2",
        arguments=["-d", rviz_config],
        output="screen",
        parameters=[{"use_sim_time": use_sim_time}],
        condition=IfCondition(use_rviz),
    )

    return LaunchDescription(
        [
            declare_use_sim_time,
            declare_rviz,
            sim,
            safety_stack,
            TimerAction(period=3.0, actions=[rviz]),
        ]
    )
