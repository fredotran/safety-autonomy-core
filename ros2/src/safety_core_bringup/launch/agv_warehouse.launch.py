"""End-to-end demo: Gazebo Harmonic + AGV + safety_core wrapper nodes + Nav2 + SLAM + RViz.

Topology (edges show topic publish direction):

    [Gazebo (industrial_warehouse.sdf, AGV)]
        |  /scan, /imu, /odom, /clock      ^  /cmd_vel
        v                                  |
    [ros_gz_bridge] ----- ROS 2 -----------+
        |                                  |
        v                                  |
    [safety_envelope_node] --/safety/envelope_status, /safety/zone_markers-->
        |
        v
    [safety_supervisor_node] ----/safety/state, /safety/safe_stop-------->
        |
        v
    [Nav2 stack] (controller_server publishes /cmd_vel_smoothed -> collision_monitor /cmd_vel_nav)
        |  /cmd_vel_nav
        v
    [safety_drive_bridge_node] (gates with envelope + jerk-limited stop) --/cmd_vel-->
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    GroupAction,
    IncludeLaunchDescription,
    LogInfo,
    SetEnvironmentVariable,
    TimerAction,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node


def generate_launch_description():
    pkg_sim = get_package_share_directory("safety_core_sim")
    pkg_bringup = get_package_share_directory("safety_core_bringup")

    use_sim_time = LaunchConfiguration("use_sim_time")
    use_rviz = LaunchConfiguration("rviz")
    use_slam = LaunchConfiguration("slam")
    use_nav2 = LaunchConfiguration("nav2")

    safety_params = os.path.join(pkg_bringup, "config", "safety_params.yaml")
    nav2_params = os.path.join(pkg_bringup, "config", "nav2_params.yaml")
    slam_params = os.path.join(pkg_bringup, "config", "slam_toolbox.yaml")
    rviz_config = os.path.join(pkg_bringup, "rviz", "agv_warehouse.rviz")
    sim_launch = os.path.join(pkg_sim, "launch", "sim_only.launch.py")

    declare_use_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true")
    declare_rviz = DeclareLaunchArgument("rviz", default_value="true")
    declare_slam = DeclareLaunchArgument("slam", default_value="true")
    declare_nav2 = DeclareLaunchArgument("nav2", default_value="true")

    # 1. Simulation (Gazebo + AGV + ros_gz_bridge + robot_state_publisher).
    sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(sim_launch),
        launch_arguments={
            "use_sim_time": use_sim_time,
        }.items(),
    )

    # 2. safety_core wrapper nodes.
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
            # Nav2 controller_server publishes to /cmd_vel after collision_monitor; we
            # intercept that here as cmd_vel_nav and republish to /cmd_vel ourselves.
            ("cmd_vel_nav", "/cmd_vel_nav"),
            ("cmd_vel", "/cmd_vel"),
        ],
    )

    safety_stack = GroupAction(
        actions=[
            LogInfo(msg="Starting safety_core wrapper stack..."),
            safety_envelope,
            safety_supervisor,
            safety_drive_bridge,
        ]
    )

    # 3. SLAM Toolbox (online async mapping).
    slam = Node(
        package="slam_toolbox",
        executable="async_slam_toolbox_node",
        name="slam_toolbox",
        output="screen",
        parameters=[slam_params, {"use_sim_time": use_sim_time}],
        condition=IfCondition(use_slam),
    )

    # 4. Nav2 stack (delayed so SLAM has time to publish a baseline map).
    # nav2_bringup is optional; if missing, skip the inclusion gracefully so
    # `ros2 launch` still works for the sim+safety subset.
    nav2_actions: list = []
    try:
        nav2_bringup_pkg = get_package_share_directory("nav2_bringup")
        nav2_launch = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(nav2_bringup_pkg, "launch", "navigation_launch.py")
            ),
            launch_arguments={
                "use_sim_time": use_sim_time,
                "params_file": nav2_params,
                "autostart": "true",
                "use_composition": "False",
            }.items(),
            condition=IfCondition(use_nav2),
        )
        nav2_actions.append(TimerAction(period=8.0, actions=[nav2_launch]))
    except Exception as exc:  # PackageNotFoundError or import error
        nav2_actions.append(
            LogInfo(
                msg=(
                    "Skipping Nav2 launch — nav2_bringup not installed. "
                    "Install with `apt install ros-jazzy-navigation2 "
                    "ros-jazzy-nav2-bringup` to enable autonomous navigation. "
                    f"Reason: {exc}"
                )
            )
        )

    # 5. RViz.
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
            declare_slam,
            declare_nav2,
            sim,
            safety_stack,
            slam,
            *nav2_actions,
            rviz,
        ]
    )
