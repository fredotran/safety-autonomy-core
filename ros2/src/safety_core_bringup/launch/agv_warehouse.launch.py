"""End-to-end demo: Gazebo Harmonic + AGV + safety_core wrapper nodes + Nav2 + RViz.

Configuration for wheel odometry-based navigation (no SLAM):
|- Robot uses diff-drive wheel odometry for localization
|- Fixed frame is base_link (robot-centric)
|- Nav2 provides planning and control
|- Safety system gates commands

This launch file now uses modular launch components for reusability and maintainability.

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
    TimerAction,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node


def generate_launch_description():
    pkg_sim = get_package_share_directory('safety_core_sim')
    pkg_bringup = get_package_share_directory('safety_core_bringup')

    use_sim_time = LaunchConfiguration('use_sim_time')
    use_rviz = LaunchConfiguration('rviz')
    use_slam = LaunchConfiguration('slam')
    use_nav2 = LaunchConfiguration('nav2')
    use_ekf = LaunchConfiguration('ekf')
    use_gps = LaunchConfiguration('gps')
    environment = LaunchConfiguration('environment')

    safety_params = os.path.join(pkg_bringup, 'config', 'safety_params.yaml')
    nav2_params = os.path.join(pkg_bringup, 'config', 'nav2_params_odometry.yaml')
    slam_params = os.path.join(pkg_bringup, 'config', 'slam_toolbox.yaml')
    ekf_config = os.path.join(pkg_bringup, 'config', 'ekf_config.yaml')
    amcl_config = os.path.join(pkg_bringup, 'config', 'amcl_config.yaml')
    
    # RViz configs: robot-centric for outdoor, map-centric for indoor/warehouse
    rviz_config_robot = os.path.join(pkg_bringup, 'rviz', 'agv_nav2_robot_frame.rviz')
    rviz_config_localization = os.path.join(pkg_bringup, 'rviz', 'agv_localization.rviz')
    rviz_config = PythonExpression([
        "'", rviz_config_localization, "' if '", environment, "' == 'indoor' or '", environment, "' == 'warehouse' else '", rviz_config_robot, "'"
    ])
    sim_launch = os.path.join(pkg_sim, 'launch', 'sim_only.launch.py')

    # Common launch arguments
    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation clock from /clock topic.'
    )
    declare_rviz = DeclareLaunchArgument(
        'rviz',
        default_value='true',
        description='Launch RViz with the demo configuration.'
    )
    declare_slam = DeclareLaunchArgument(
        'slam',
        default_value='false',
        description='Launch SLAM Toolbox (online async mapping).'
    )
    declare_nav2 = DeclareLaunchArgument(
        'nav2',
        default_value='true',
        description='Launch the Nav2 navigation stack (only if installed).'
    )
    declare_ekf = DeclareLaunchArgument(
        'ekf',
        default_value='false',
        description='Enable EKF multi-sensor fusion localization (robot_localization).'
    )
    declare_gps = DeclareLaunchArgument(
        'gps',
        default_value='true',
        description='When ekf:=true, also enable navsat_transform_node + map-frame EKF.',
    )
    
    # Custom launch arguments for this specific launch file
    declare_nav2_controller = DeclareLaunchArgument(
        'nav2_controller',
        default_value='dwb',
        description='Nav2 controller type: mppi (MPPI) or dwb (DWB - simpler, recommended for diff-drive)',
    )
    declare_use_teleop = DeclareLaunchArgument(
        'teleop',
        default_value='false',
        description='Enable teleoperation node for manual control',
    )
    declare_environment = DeclareLaunchArgument(
        'environment',
        default_value='outdoor',
        description='Environment type: outdoor (EKF+GPS+Nav2), indoor (EKF+SLAM+Nav2), warehouse (EKF+Nav2, map-based)',
        choices=['outdoor', 'indoor', 'warehouse'],
    )
    declare_map_file = DeclareLaunchArgument(
        'map_file',
        default_value=os.path.join(pkg_bringup, 'maps', 'warehouse_map.yaml'),
        description='Path to map file for AMCL (warehouse environment)'
    )

    # Environment-specific configuration
    use_gps_env = PythonExpression([
        "'true' if '", environment, "' == 'outdoor' else 'false'"
    ])
    use_slam_env = PythonExpression([
        "'true' if '", environment, "' == 'indoor' else 'false'"
    ])
    use_amcl_env = PythonExpression([
        "'true' if '", environment, "' == 'warehouse' else 'false'"
    ])

    # 1. Simulation (Gazebo + AGV + ros_gz_bridge + robot_state_publisher)
    sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(sim_launch),
        launch_arguments={
            'use_sim_time': use_sim_time,
        }.items(),
    )

    # 2. Safety stack with Nav2 remappings using modular component
    safety_stack = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_bringup, 'launch', 'safety_stack.launch.py')),
        launch_arguments={
            'safety_params': safety_params,
            'use_sim_time': use_sim_time,
            'include_remappings': 'true',  # Enable Nav2 cmd_vel remappings
        }.items()
    )

    # 3. EKF localization stack using modular component
    ekf_stack = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_bringup, 'launch', 'ekf_stack.launch.py')),
        launch_arguments={
            'ekf_config': ekf_config,
            'use_sim_time': use_sim_time,
            'enable_gps': use_gps_env,  # Environment-specific GPS enablement
        }.items(),
        condition=IfCondition(use_ekf),
    )

    # 4. SLAM Toolbox (optional - for indoor environments)
    slam = GroupAction(
        actions=[
            Node(
                package='slam_toolbox',
                executable='async_slam_toolbox_node',
                name='slam_toolbox',
                output='screen',
                parameters=[slam_params, {'use_sim_time': use_sim_time}],
            )
        ],
        condition=IfCondition(use_slam_env),
    )

    # 5. AMCL + Map Server (optional - for warehouse environments with pre-existing map)
    map_file = LaunchConfiguration('map_file')
    
    amcl_stack = GroupAction(
        actions=[
            Node(
                package='nav2_map_server',
                executable='map_server',
                name='map_server',
                output='screen',
                parameters=[{'yaml_filename': map_file, 'use_sim_time': use_sim_time}],
            ),
            Node(
                package='nav2_lifecycle_manager',
                executable='lifecycle_manager',
                name='lifecycle_manager_map',
                output='screen',
                parameters=[{'use_sim_time': use_sim_time, 'autostart': True, 'node_names': ['map_server']}],
            ),
            Node(
                package='nav2_amcl',
                executable='amcl',
                name='amcl',
                output='screen',
                parameters=[amcl_config, {'use_sim_time': use_sim_time}],
            ),
            Node(
                package='nav2_lifecycle_manager',
                executable='lifecycle_manager',
                name='lifecycle_manager_localization',
                output='screen',
                parameters=[{'use_sim_time': use_sim_time, 'autostart': True, 'node_names': ['amcl']}],
            ),
        ],
        condition=IfCondition(use_amcl_env),
    )

    # 6. Nav2 stack (delayed startup, configured for odometry-based localization)
    nav2_actions = []
    try:
        nav2_bringup_pkg = get_package_share_directory('nav2_bringup')
        nav2_launch = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(nav2_bringup_pkg, 'launch', 'navigation_launch.py')
            ),
            launch_arguments={
                'use_sim_time': use_sim_time,
                'params_file': nav2_params,
                'autostart': 'true',
                'use_composition': 'False',
            }.items(),
            condition=IfCondition(PythonExpression(["'", environment, "' != 'warehouse'"])),
        )
        nav2_actions.append(TimerAction(period=5.0, actions=[nav2_launch]))
    except Exception as exc:
        nav2_actions.append(
            LogInfo(
                msg=(
                    'Skipping Nav2 launch — nav2_bringup not installed. '
                    'Install with `apt install ros-jazzy-navigation2 '
                    'ros-jazzy-nav2-bringup` to enable autonomous navigation. '
                    f'Reason: {exc}'
                )
            )
        )

    # 7. RViz with environment-specific config using modular component
    rviz = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_bringup, 'launch', 'rviz_component.launch.py')),
        launch_arguments={
            'rviz_config': rviz_config,
            'use_sim_time': use_sim_time,
            'enable_rviz': use_rviz,
            'delay_s': '3.0',
        }.items()
    )

    return LaunchDescription([
        declare_use_sim_time,
        declare_rviz,
        declare_slam,
        declare_nav2,
        declare_ekf,
        declare_gps,
        declare_nav2_controller,
        declare_use_teleop,
        declare_environment,
        declare_map_file,
        sim,
        safety_stack,
        ekf_stack,
        slam,
        amcl_stack,
        *nav2_actions,
        rviz,
    ])