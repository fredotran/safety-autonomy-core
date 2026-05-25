"""Brings up Gazebo Harmonic with the industrial warehouse, spawns the AGV,
   and starts the ros_gz_bridge. Pure simulation — no Nav2, no safety_core."""

import os
from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, SetEnvironmentVariable
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration, PathJoinSubstitution, PythonExpression
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_sim = get_package_share_directory("safety_core_sim")
    pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")

    world_file = LaunchConfiguration("world")
    use_sim_time = LaunchConfiguration("use_sim_time")
    gui = LaunchConfiguration("gui")
    spawn_x = LaunchConfiguration("x")
    spawn_y = LaunchConfiguration("y")
    spawn_yaw = LaunchConfiguration("yaw")

    declare_world = DeclareLaunchArgument(
        "world",
        default_value=os.path.join(pkg_sim, "worlds", "industrial_warehouse.sdf"),
        description="Path to the SDF world file.",
    )
    declare_use_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true", description="Use simulation time from /clock topic.")
    declare_gui = DeclareLaunchArgument(
        "gui", default_value="true", description="Run gz sim with GUI."
    )
    declare_x = DeclareLaunchArgument("x", default_value="-9.0", description="Initial X position of the robot.")
    declare_y = DeclareLaunchArgument("y", default_value="0.0", description="Initial Y position of the robot.")
    declare_yaw = DeclareLaunchArgument("yaw", default_value="0.0", description="Initial yaw orientation of the robot (radians).")

    # Make our /models path discoverable so gz can find local meshes if any.
    set_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=os.path.join(pkg_sim, "models")
        + os.pathsep
        + os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
    )

    # Robot description from xacro.
    xacro_file = os.path.join(pkg_sim, "description", "agv.urdf.xacro")
    robot_desc = Command(["xacro ", xacro_file])

    # Gazebo Harmonic.
    gz_sim_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")
        ),
        launch_arguments={
            "gz_args": PythonExpression([
                "'",
                world_file,
                " -r",  # auto-run physics
                " --render-engine ogre2",
                "'"
            ]),
            "on_exit_shutdown": "True",
        }.items(),
    )

    # Robot state publisher (publishes URDF to /robot_description, broadcasts TF).
    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="screen",
        parameters=[
            {"robot_description": robot_desc, "use_sim_time": use_sim_time}
        ],
    )

    # Spawn AGV in Gazebo using the robot_description topic.
    spawn_entity = Node(
        package="ros_gz_sim",
        executable="create",
        arguments=[
            "-name", "safety_core_agv",
            "-topic", "/robot_description",
            "-x", spawn_x,
            "-y", spawn_y,
            "-z", "0.1",
            "-Y", spawn_yaw,
            "-allow_renaming", "false",
        ],
        output="screen",
    )

    # Bridge.
    bridge = Node(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        parameters=[
            {
                "config_file": os.path.join(pkg_sim, "config", "ros_gz_bridge.yaml"),
                "use_sim_time": use_sim_time,
            }
        ],
        output="screen",
    )

    return LaunchDescription(
        [
            declare_world,
            declare_use_sim_time,
            declare_gui,
            declare_x,
            declare_y,
            declare_yaw,
            set_resource_path,
            gz_sim_launch,
            robot_state_publisher,
            spawn_entity,
            bridge,
        ]
    )
