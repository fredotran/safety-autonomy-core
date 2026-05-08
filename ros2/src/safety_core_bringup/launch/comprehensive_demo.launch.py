"""Launch file for comprehensive safety autonomy core demo.

This launch file starts:
- Gazebo Harmonic with industrial warehouse world
- AGV robot with differential drive, 360° lidar, IMU
- ROS 2 Gazebo bridge for sensor/actuator topics
- Safety envelope node (computes safety zones)
- Safety supervisor node (manages state machine)
- Safety drive bridge node (gates commands)
- Optional RViz for visualization

The comprehensive_demo.py script then orchestrates a sequence of maneuvers
that demonstrate all safety capabilities.
"""

import os
from pathlib import Path
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    SetEnvironmentVariable,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_bringup = get_package_share_directory("safety_core_bringup")
    pkg_sim = get_package_share_directory("safety_core_sim")
    pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")
    
    # Configuration files
    safety_params = os.path.join(pkg_bringup, "config", "safety_params.yaml")
    bridge_config = os.path.join(pkg_sim, "config", "ros_gz_bridge.yaml")
    world_file = os.path.join(pkg_sim, "worlds", "industrial_warehouse.sdf")
    
    # Launch arguments
    use_sim_time = LaunchConfiguration("use_sim_time")
    
    declare_use_sim_time = DeclareLaunchArgument(
        "use_sim_time", default_value="true", description="Use simulation time"
    )
    
    # Make our /models path discoverable
    set_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=os.path.join(pkg_sim, "models")
        + os.pathsep
        + os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
    )
    
    # Robot description from xacro
    xacro_file = os.path.join(pkg_sim, "description", "agv.urdf.xacro")
    robot_desc = Command(["xacro ", xacro_file])
    
    # Gazebo Harmonic using gz_sim.launch.py
    gz_sim_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")
        ),
        launch_arguments={
            "gz_args": [
                world_file,
                " -r",  # auto-run physics
                " --render-engine ogre2",
            ],
            "on_exit_shutdown": "True",
        }.items(),
    )
    
    # Robot state publisher
    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        parameters=[{"robot_description": robot_desc, "use_sim_time": use_sim_time}],
        output="screen",
    )
    
    # Spawn robot in Gazebo
    spawn_robot = Node(
        package="ros_gz_sim",
        executable="create",
        arguments=[
            "-name", "safety_core_agv",
            "-topic", "/robot_description",
            "-x", "0.0",
            "-y", "0.0",
            "-z", "0.05",
            "-Y", "0.0",
        ],
        output="screen",
    )
    
    # ROS 2 Gazebo bridge
    parameter_bridge = Node(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        parameters=[{"config_file": bridge_config, "use_sim_time": use_sim_time}],
        output="screen",
    )
    
    # Safety envelope node
    safety_envelope_node = Node(
        package="safety_core_ros",
        executable="safety_envelope_node",
        name="safety_envelope_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
    )
    
    # Safety supervisor node
    safety_supervisor_node = Node(
        package="safety_core_ros",
        executable="safety_supervisor_node",
        name="safety_supervisor_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
    )
    
    # Safety drive bridge node
    safety_drive_bridge_node = Node(
        package="safety_core_ros",
        executable="safety_drive_bridge_node",
        name="safety_drive_bridge_node",
        output="screen",
        parameters=[safety_params, {"use_sim_time": use_sim_time}],
    )
    
    return LaunchDescription(
        [
            declare_use_sim_time,
            set_resource_path,
            gz_sim_launch,
            robot_state_publisher,
            spawn_robot,
            parameter_bridge,
            safety_envelope_node,
            safety_supervisor_node,
            safety_drive_bridge_node,
        ]
    )
