"""End-to-end demo: Gazebo Harmonic + AGV + safety_core wrapper nodes + Nav2 + RViz.

Configuration for wheel odometry-based navigation (no SLAM):
- Robot uses diff-drive wheel odometry for localization
- Fixed frame is base_link (robot-centric)
- Nav2 provides planning and control
- Safety system gates commands

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
    use_ekf = LaunchConfiguration("ekf")
    use_gps = LaunchConfiguration("gps")
    nav2_controller = LaunchConfiguration("nav2_controller")
    use_teleop = LaunchConfiguration("teleop")

    safety_params = os.path.join(pkg_bringup, "config", "safety_params.yaml")
    # Odometry-only Nav2 config (no AMCL / no map_server) for the default wheel-odometry mode.
    nav2_params_odometry = os.path.join(pkg_bringup, "config", "nav2_params_odometry.yaml")
    nav2_params_simple = os.path.join(pkg_bringup, "config", "nav2_params_simple.yaml")
    slam_params = os.path.join(pkg_bringup, "config", "slam_toolbox.yaml")
    ekf_config = os.path.join(pkg_bringup, "config", "ekf_config.yaml")
    # Robot-centric (Fixed Frame: odom) RViz config tuned for the wheel-odometry demo.
    rviz_config = os.path.join(pkg_bringup, "rviz", "agv_nav2_robot_frame.rviz")
    sim_launch = os.path.join(pkg_sim, "launch", "sim_only.launch.py")

    declare_use_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true")
    declare_rviz = DeclareLaunchArgument("rviz", default_value="true")
    # SLAM disabled by default — the demo runs in pure wheel-odometry mode.
    declare_slam = DeclareLaunchArgument("slam", default_value="false")
    declare_nav2 = DeclareLaunchArgument("nav2", default_value="true")
    # EKF localization disabled by default; opt in with ekf:=true.
    declare_ekf = DeclareLaunchArgument("ekf", default_value="false")
    declare_gps = DeclareLaunchArgument(
        "gps",
        default_value="true",
        description="When ekf:=true, also enable navsat_transform_node + map-frame EKF.",
    )
    declare_nav2_controller = DeclareLaunchArgument(
        "nav2_controller",
        default_value="dwb",
        description="Nav2 controller type: 'mppi' (MPPI) or 'dwb' (DWB - simpler, recommended for diff-drive)",
    )
    declare_use_teleop = DeclareLaunchArgument(
        "teleop",
        default_value="false",
        description="Enable teleoperation node for manual control",
    )

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

    teleop = Node(
        package="safety_core_ros",
        executable="teleop_node",
        name="teleop_node",
        output="screen",
        parameters=[
            {"use_sim_time": use_sim_time},
            {"linear_speed": 0.3},
            {"angular_speed": 0.3},
        ],
        condition=IfCondition(use_teleop),
    )

    safety_stack = GroupAction(
        actions=[
            LogInfo(msg="Starting safety_core wrapper stack..."),
            safety_envelope,
            safety_supervisor,
            safety_drive_bridge,
            teleop,
        ]
    )

    # 3. EKF localization stack (optional). Two-EKF + navsat_transform pattern:
    #    * ekf_filter_node       -- fuses /odom + /imu, owns odom -> base_link TF.
    #    * navsat_transform_node -- /gps + /imu + /odometry/filtered -> /odometry/gps (map frame).
    #    * ekf_filter_node_map   -- fuses /odom + /imu + /odometry/gps, owns map -> odom TF.
    ekf_local = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_config, {"use_sim_time": use_sim_time}],
        condition=IfCondition(use_ekf),
    )
    use_ekf_and_gps = PythonExpression(["'", use_ekf, "' == 'true' and '", use_gps, "' == 'true'"])
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
        condition=IfCondition(use_ekf_and_gps),
    )
    ekf_global = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node_map",
        output="screen",
        parameters=[ekf_config, {"use_sim_time": use_sim_time}],
        remappings=[("odometry/filtered", "/odometry/filtered_map")],
        condition=IfCondition(use_ekf_and_gps),
    )

    ekf_stack = GroupAction(
        actions=[
            LogInfo(msg="Starting EKF localization stack (robot_localization)..."),
            ekf_local,
            navsat_transform,
            ekf_global,
        ]
    )

    # 4. SLAM Toolbox (optional - disabled by default for wheel odometry mode).
    slam = Node(
        package="slam_toolbox",
        executable="async_slam_toolbox_node",
        name="slam_toolbox",
        output="screen",
        parameters=[slam_params, {"use_sim_time": use_sim_time}],
        condition=IfCondition(use_slam),
    )

    # 4. Nav2 stack (delayed startup, configured for odometry-based localization).
    # nav2_bringup is optional; if missing, skip the inclusion gracefully so
    # `ros2 launch` still works for the sim+safety subset.
    nav2_actions: list = []
    try:
        nav2_bringup_pkg = get_package_share_directory("nav2_bringup")
        # Select Nav2 config based on controller type
        nav2_params_selected = PythonExpression([
            "'", nav2_params_simple, "' if '", nav2_controller, "' == 'dwb' else '", nav2_params_odometry, "'"
        ])
        nav2_launch = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(nav2_bringup_pkg, "launch", "navigation_launch.py")
            ),
            launch_arguments={
                "use_sim_time": use_sim_time,
                "params_file": nav2_params_selected,
                "autostart": "true",
                "use_composition": "False",
            }.items(),
            condition=IfCondition(use_nav2),
        )
        nav2_actions.append(TimerAction(period=5.0, actions=[nav2_launch]))
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

    # 5. RViz with robot-centric fixed frame (delayed startup).
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
            declare_ekf,
            declare_gps,
            declare_nav2_controller,
            declare_use_teleop,
            sim,
            safety_stack,
            ekf_stack,
            slam,
            *nav2_actions,
            TimerAction(period=3.0, actions=[rviz]),  # Delay RViz by 3 seconds
        ]
    )
