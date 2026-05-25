"""Shared launch utilities for safety_core_bringup.

Provides reusable factory functions for common node groups, eliminating
duplication across launch files. Import and call these instead of
copy-pasting Node() declarations.
"""

import os
from typing import List, Optional

from ament_index_python.packages import get_package_share_directory
from launch.actions import DeclareLaunchArgument, GroupAction, LogInfo, TimerAction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node


def get_safety_params_path() -> str:
    """Return absolute path to the default safety_params.yaml."""
    pkg = get_package_share_directory("safety_core_bringup")
    return os.path.join(pkg, "config", "safety_params.yaml")


def get_ekf_config_path() -> str:
    """Return absolute path to the default ekf_config.yaml."""
    pkg = get_package_share_directory("safety_core_bringup")
    return os.path.join(pkg, "config", "ekf_config.yaml")


# ---------------------------------------------------------------------------
# Common launch argument declarations
# ---------------------------------------------------------------------------

def declare_use_sim_time(default: str = "true") -> DeclareLaunchArgument:
    """Declare ``use_sim_time`` launch argument."""
    return DeclareLaunchArgument(
        "use_sim_time",
        default_value=default,
        description="Use simulation clock from /clock topic.",
    )


def declare_rviz(default: str = "true") -> DeclareLaunchArgument:
    """Declare ``rviz`` launch argument."""
    return DeclareLaunchArgument(
        "rviz",
        default_value=default,
        description="Launch RViz with the demo configuration.",
    )


def declare_ekf(default: str = "false") -> DeclareLaunchArgument:
    """Declare ``ekf`` launch argument."""
    return DeclareLaunchArgument(
        "ekf",
        default_value=default,
        description="Enable EKF multi-sensor fusion localization (robot_localization).",
    )


def declare_gps(default: str = "true") -> DeclareLaunchArgument:
    """Declare ``gps`` launch argument."""
    return DeclareLaunchArgument(
        "gps",
        default_value=default,
        description="When ekf:=true, also enable navsat_transform_node + map-frame EKF.",
    )


def declare_slam(default: str = "false") -> DeclareLaunchArgument:
    """Declare ``slam`` launch argument."""
    return DeclareLaunchArgument(
        "slam",
        default_value=default,
        description="Launch SLAM Toolbox (online async mapping).",
    )


def declare_nav2(default: str = "true") -> DeclareLaunchArgument:
    """Declare ``nav2`` launch argument."""
    return DeclareLaunchArgument(
        "nav2",
        default_value=default,
        description="Launch the Nav2 navigation stack (only if installed).",
    )


# ---------------------------------------------------------------------------
# Safety stack factory
# ---------------------------------------------------------------------------

def make_safety_nodes(
    safety_params: str,
    use_sim_time: LaunchConfiguration,
    include_remappings: bool = True,
) -> List[Node]:
    """Create the three safety wrapper nodes.

    Args:
        safety_params: Path to safety_params.yaml.
        use_sim_time: LaunchConfiguration for use_sim_time.
        include_remappings: Whether to add cmd_vel topic remappings
            to the drive bridge node.

    Returns:
        List of [envelope, supervisor, drive_bridge] Node actions.
    """
    envelope = Node(
        package="safety_core_ros",
        executable="safety_envelope_node",
        name="safety_envelope_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
    )

    supervisor = Node(
        package="safety_core_ros",
        executable="safety_supervisor_node",
        name="safety_supervisor_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
    )

    remappings = [
        ("cmd_vel_nav", "/cmd_vel_nav"),
        ("cmd_vel", "/cmd_vel"),
    ] if include_remappings else []

    drive_bridge = Node(
        package="safety_core_ros",
        executable="safety_drive_bridge_node",
        name="safety_drive_bridge_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
        remappings=remappings,
    )

    return [envelope, supervisor, drive_bridge]


def make_safety_stack(
    safety_params: str,
    use_sim_time: LaunchConfiguration,
    log_message: str = "Starting safety_core wrapper stack...",
    include_remappings: bool = True,
) -> GroupAction:
    """Create a GroupAction containing the full safety stack.

    Args:
        safety_params: Path to safety_params.yaml.
        use_sim_time: LaunchConfiguration for use_sim_time.
        log_message: Startup message logged when the group starts.
        include_remappings: Whether to add cmd_vel topic remappings.

    Returns:
        A GroupAction wrapping [LogInfo, envelope, supervisor, drive_bridge].
    """
    nodes = make_safety_nodes(safety_params, use_sim_time, include_remappings)
    return GroupAction(
        actions=[LogInfo(msg=log_message)] + nodes,
    )


# ---------------------------------------------------------------------------
# EKF / localization stack factory
# ---------------------------------------------------------------------------

def make_ekf_stack(
    ekf_config: str,
    use_sim_time: LaunchConfiguration,
    use_ekf_condition: Optional[IfCondition] = None,
    use_gps_condition: Optional[IfCondition] = None,
    log_message: str = "Starting EKF localization stack (robot_localization)...",
) -> GroupAction:
    """Create a GroupAction containing the EKF localization nodes.

    The stack follows the two-EKF + navsat_transform pattern:
      * ekf_filter_node       — fuses /odom + /imu, owns odom→base_link TF.
      * navsat_transform_node — converts GPS to map-frame odometry.
      * ekf_filter_node_map   — fuses odom + /imu + /odometry/gps, owns map→odom TF.

    Args:
        ekf_config: Path to ekf_config.yaml.
        use_sim_time: LaunchConfiguration for use_sim_time.
        use_ekf_condition: IfCondition for the local EKF node (optional).
        use_gps_condition: IfCondition for navsat + global EKF (optional).
        log_message: Startup log message.

    Returns:
        A GroupAction wrapping the localization nodes.
    """
    ekf_local = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_config, {"use_sim_time": use_sim_time}],
        remappings=[("odometry/filtered", "/odometry/filtered")],
        condition=use_ekf_condition,
    )

    navsat_transform = Node(
        package="robot_localization",
        executable="navsat_transform_node",
        name="navsat_transform",
        output="screen",
        parameters=[ekf_config, {"use_sim_time": use_sim_time}],
        remappings=[
            ("imu/data", "/imu"),
            ("gps/fix", "/gps"),
            ("odometry/filtered", "/odometry/filtered"),
            ("odometry/gps", "/odometry/gps"),
            ("gps/filtered", "/gps/filtered"),
        ],
        condition=use_gps_condition,
    )

    ekf_global = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node_map",
        output="screen",
        parameters=[ekf_config, {"use_sim_time": use_sim_time}],
        remappings=[("odometry/filtered", "/odometry/filtered_map")],
        condition=use_gps_condition,
    )

    return GroupAction(
        actions=[
            LogInfo(msg=log_message),
            ekf_local,
            navsat_transform,
            ekf_global,
        ]
    )


# ---------------------------------------------------------------------------
# RViz factory
# ---------------------------------------------------------------------------

def make_rviz_node(
    rviz_config: str,
    use_sim_time: LaunchConfiguration,
    use_rviz: LaunchConfiguration,
    delay_s: float = 3.0,
) -> TimerAction:
    """Create a delayed RViz node wrapped in a TimerAction.

    Args:
        rviz_config: Path to the .rviz configuration file.
        use_sim_time: LaunchConfiguration for use_sim_time.
        use_rviz: LaunchConfiguration for the rviz toggle.
        delay_s: Seconds to delay RViz startup.

    Returns:
        A TimerAction launching RViz after the specified delay.
    """
    rviz = Node(
        package="rviz2",
        executable="rviz2",
        arguments=["-d", rviz_config],
        output="screen",
        parameters=[{"use_sim_time": use_sim_time}],
        condition=IfCondition(use_rviz),
    )
    return TimerAction(period=delay_s, actions=[rviz])
