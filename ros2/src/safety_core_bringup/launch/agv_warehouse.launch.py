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
    amcl_config = os.path.join(pkg_bringup, "config", "amcl_config.yaml")
    # RViz configs: robot-centric for outdoor, map-centric for indoor/warehouse
    rviz_config_robot = os.path.join(pkg_bringup, "rviz", "agv_nav2_robot_frame.rviz")
    rviz_config_localization = os.path.join(pkg_bringup, "rviz", "agv_localization.rviz")
    rviz_config = PythonExpression([
        "'", rviz_config_localization, "' if '", environment, "' == 'indoor' or '", environment, "' == 'warehouse' else '", rviz_config_robot, "'"
    ])
    sim_launch = os.path.join(pkg_sim, "launch", "sim_only.launch.py")

    declare_use_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true")
    declare_rviz = DeclareLaunchArgument("rviz", default_value="true")
    # SLAM disabled by default — the demo runs in pure wheel-odometry mode.
    declare_slam = DeclareLaunchArgument("slam", default_value="false")
    declare_nav2 = DeclareLaunchArgument("nav2", default_value="true")
    # EKF localization enabled by default for better odometry in outdoor/mixed environments.
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
    declare_environment = DeclareLaunchArgument(
        "environment",
        default_value="outdoor",
        description="Environment type: 'outdoor' (EKF+GPS+Nav2), 'indoor' (EKF+SLAM+Nav2), 'warehouse' (EKF+Nav2, map-based)",
        choices=["outdoor", "indoor", "warehouse"],
    )

    # Environment-specific configuration
    # Outdoor: EKF + GPS + Nav2 (default)
    # Indoor: EKF + SLAM + Nav2
    # Warehouse: EKF + Nav2 with map-based navigation
    use_ekf_env = PythonExpression([
        ekf
    ])
    use_gps_env = PythonExpression([
        "'true' if '", environment, "' == 'outdoor' else 'false'"
    ])
    use_slam_env = PythonExpression([
        "'true' if '", environment, "' == 'indoor' else 'false'"
    ])

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
        remappings=[
            ("odometry/filtered", "/odometry/filtered"),
        ],
        condition=IfCondition(use_ekf_env),
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
        remappings=[
            ("odometry/filtered", "/odometry/filtered_map"),
        ],
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

    # 4. SLAM Toolbox (optional - for indoor environments)
    slam = Node(
        package="slam_toolbox",
        executable="async_slam_toolbox_node",
        name="slam_toolbox",
        output="screen",
        parameters=[slam_params, {"use_sim_time": use_sim_time}],
        condition=IfCondition(use_slam_env),
    )

    # 5. AMCL + Map Server (optional - for warehouse environments with pre-existing map)
    # Note: Map file must exist - use SLAM to generate a map first
    use_amcl_env = PythonExpression([
        "'true' if '", environment, "' == 'warehouse' else 'false'"
    ])
    map_file = LaunchConfiguration("map_file")
    declare_map_file = DeclareLaunchArgument(
        "map_file",
        default_value=os.path.join(pkg_bringup, "maps", "warehouse_map.yaml"),
        description="Path to map file for AMCL (warehouse environment)"
    )

    map_server = Node(
        package="nav2_map_server",
        executable="map_server",
        name="map_server",
        output="screen",
        parameters=[{"yaml_filename": map_file, "use_sim_time": use_sim_time}],
        condition=IfCondition(use_amcl_env),
    )

    lifecycle_manager_map = Node(
        package="nav2_lifecycle_manager",
        executable="lifecycle_manager",
        name="lifecycle_manager_map",
        output="screen",
        parameters=[{"use_sim_time": use_sim_time, "autostart": True, "node_names": ["map_server"]}],
        condition=IfCondition(use_amcl_env),
    )

    amcl = Node(
        package="nav2_amcl",
        executable="amcl",
        name="amcl",
        output="screen",
        parameters=[amcl_config, {"use_sim_time": use_sim_time}],
        condition=IfCondition(use_amcl_env),
    )

    lifecycle_manager_localization = Node(
        package="nav2_lifecycle_manager",
        executable="lifecycle_manager",
        name="lifecycle_manager_localization",
        output="screen",
        parameters=[{"use_sim_time": use_sim_time, "autostart": True, "node_names": ["amcl"]}],
        condition=IfCondition(use_amcl_env),
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
            condition=IfCondition(PythonExpression(["'", environment, "' != 'warehouse'"])),
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
            map_server,
            lifecycle_manager_map,
            amcl,
            lifecycle_manager_localization,
            *nav2_actions,
            TimerAction(period=3.0, actions=[rviz]),  # Delay RViz by 3 seconds
        ]
    )
