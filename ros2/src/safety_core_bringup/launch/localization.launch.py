"""Stand-alone EKF + navsat_transform localization stack.

Useful for:
  * Bench testing the localization pipeline against ROS bags or a physical AGV
    that already publishes /odom, /imu, and /gps.
  * Tuning the EKF / navsat_transform parameters without paying the
    Gazebo / Nav2 / RViz startup cost.

This launch file now uses the modular ekf_stack.launch.py component for
reusability and maintainability.

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
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    """Generate launch description for EKF localization stack."""
    pkg_bringup = get_package_share_directory('safety_core_bringup')
    ekf_config = os.path.join(pkg_bringup, 'config', 'ekf_config.yaml')

    use_sim_time = LaunchConfiguration('use_sim_time')
    use_gps = LaunchConfiguration('gps')

    # Declare launch arguments
    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time', 
        default_value='true',
        description='Use simulation clock from /clock topic.'
    )
    declare_gps = DeclareLaunchArgument(
        'gps',
        default_value='true',
        description='Enable navsat_transform_node + global ekf_filter_node_map.',
    )

    # Include the modular EKF stack component
    ekf_stack = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_bringup, 'launch', 'ekf_stack.launch.py')),
        launch_arguments={
            'ekf_config': ekf_config,
            'use_sim_time': use_sim_time,
            'enable_gps': use_gps,
        }.items()
    )

    return LaunchDescription([
        declare_use_sim_time,
        declare_gps,
        ekf_stack,
    ])
