"""Diagnostic launch file to check system status and debug issues.

This launch file starts the safety system and provides diagnostic tools
to help identify why the robot might not be moving or other issues.
"""

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, LogInfo
from launch_ros.actions import Node
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    pkg_bringup = get_package_share_directory("safety_core_bringup")
    pkg_sim = get_package_share_directory("safety_core_sim")
    
    use_sim_time = LaunchConfiguration("use_sim_time")
    safety_params = f"{pkg_bringup}/config/safety_params.yaml"
    sim_launch = f"{pkg_sim}/launch/sim_only.launch.py"
    
    declare_use_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true")
    
    # Simulation
    from launch.launch_description_sources import PythonLaunchDescriptionSource
    sim = Node(
        package="safety_core_sim",
        executable="sim_only",
        parameters=[{"use_sim_time": use_sim_time}],
    )
    
    # Safety nodes
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
    
    # Teleop node for manual testing
    teleop = Node(
        package="safety_core_ros",
        executable="teleop_node",
        name="teleop_node",
        output="screen",
        parameters=[{
            "use_sim_time": use_sim_time,
            "linear_speed": 0.3,
            "angular_speed": 0.3,
        }],
    )
    
    safety_stack = GroupAction(
        actions=[
            LogInfo(msg="=== DIAGNOSTIC MODE ==="),
            LogInfo(msg="This launches simulation + safety system + teleop"),
            LogInfo(msg="Use keyboard controls: w/s (forward/back), a/d (rotate), space (stop), q (quit)"),
            LogInfo(msg="Monitor topics:"),
            LogInfo(msg="  ros2 topic echo /safety/state"),
            LogInfo(msg="  ros2 topic echo /safety/envelope_status"),
            LogInfo(msg="  ros2 topic echo /safety/safe_stop"),
            LogInfo(msg="  ros2 topic echo /cmd_vel_nav"),
            LogInfo(msg="  ros2 topic echo /cmd_vel"),
            LogInfo(msg="========================="),
            sim,
            safety_envelope,
            safety_supervisor,
            safety_drive_bridge,
            teleop,
        ]
    )
    
    return LaunchDescription([
        declare_use_sim_time,
        safety_stack,
    ])
