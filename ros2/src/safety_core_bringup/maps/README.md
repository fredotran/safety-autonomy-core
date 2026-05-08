# Maps Directory

This directory contains map files for AMCL-based localization in warehouse environments.

## Generating a Map

To generate a map for warehouse navigation:

1. Run the system with SLAM enabled in indoor mode:
   ```bash
   ros2 launch safety_core_bringup agv_warehouse.launch.py environment:=indoor
   ```

2. Drive the robot around the warehouse to build a complete map.

3. Save the map using SLAM Toolbox map saver:
   ```bash
   ros2 run nav2_map_server map_saver_cli -f ~/warehouse_map
   ```

4. Copy the generated files to this directory:
   ```bash
   cp ~/warehouse_map.yaml /path/to/safety-autonomy-core/ros2/src/safety_core_bringup/maps/warehouse_map.yaml
   cp ~/warehouse_map.pgm /path/to/safety-autonomy-core/ros2/src/safety_core_bringup/maps/warehouse_map.pgm
   ```

5. Use the warehouse environment with AMCL:
   ```bash
   ros2 launch safety_core_bringup agv_warehouse.launch.py environment:=warehouse
   ```

## Current Status

No map files are currently available. Use SLAM mode to generate a map first.

## Note

The warehouse environment with AMCL is an advanced feature that requires:
- A pre-generated map file (warehouse_map.yaml and warehouse_map.pgm)
- Proper map_server configuration
- AMCL particle filter tuning for your specific environment

For basic demo testing, use the `outdoor` or `indoor` environments which do not require pre-generated maps.
