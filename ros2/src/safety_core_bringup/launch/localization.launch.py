"""Stand-alone EKF + navsat_transform localization stack.

Useful for:
  * Bench testing the localization pipeline against ROS bags or a physical AGV
    that already publishes /odom, /imu, and /gps.
  * Tuning the EKF / navsat_transform parameters without paying the
    Gazebo / Nav2 / RViz startup cost.

Topology::

    /odom        -+
    /imu         -+--> [ekf_filter_node     (odom frame)] --> /odometry/filtered
                  |                                          + odom -> base_link TF
                  |
                  +--> [navsat_transform_node]
                  |          ^
    /gps         -+----------+
                                    |
                                    +-> /odometry/gps    (Odometry in map frame)
                                    +-> /gps/filtered    (NavSatFix back-projection)
                                    |
    /odom, /imu, /odometry/gps -> [ekf_filter_node_map  (map frame)]
                                    -> /odometry/filtered_map
                                    + map -> odom TF

The two-EKF + navsat_transform pattern is the recommended robot_localization
setup for outdoor / mixed indoor-outdoor robots and gives the global EKF
explicit Mahalanobis-gating control over GPS outliers.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    """Generate launch description for EKF localization stack."""
    pkg_bringup = get_package_share_directory("safety_core_bringup")
    ekf_config = os.path.join(pkg_bringup, "config", "ekf_config.yaml")

    use_sim_time = LaunchConfiguration("use_sim_time")
    use_gps = LaunchConfiguration("gps")

    declare_use_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true")
    declare_gps = DeclareLaunchArgument(
        "gps",
        default_value="true",
        description="Enable navsat_transform_node + global ekf_filter_node_map.",
    )

    # Local EKF: odom -> base_link.
    ekf_local = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_config, {"use_sim_time": use_sim_time}],
        remappings=[
            ("odometry/filtered", "/odometry/filtered"),
        ],
    )

    # GPS -> map-frame Odometry.
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
        condition=IfCondition(use_gps),
    )

    # Global EKF: map -> odom (and consumes /odometry/gps from navsat_transform).
    ekf_global = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node_map",
        output="screen",
        parameters=[ekf_config, {"use_sim_time": use_sim_time}],
        remappings=[
            ("odometry/filtered", "/odometry/filtered_map"),
        ],
        condition=IfCondition(use_gps),
    )

    return LaunchDescription(
        [
            declare_use_sim_time,
            declare_gps,
            ekf_local,
            navsat_transform,
            ekf_global,
        ]
    )
