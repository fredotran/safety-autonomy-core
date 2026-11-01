# RViz Fix Guide

Quick fixes for RViz display problems when running the demo through Docker with
X11 forwarding.

## RViz window never appears

1. Verify `DISPLAY` reaches the container:

   ```bash
   docker exec safety-autonomy-demo env | grep DISPLAY
   ```

   Empty → export `DISPLAY` on the host before `docker compose up`.

2. Allow X11 connections from the container:

   ```bash
   xhost +local:docker    # host side, before starting the demo
   ```

3. Check the Xauthority mount is valid. A stale `${XAUTHORITY}` path prevents the
   container from starting at all — see
   [DEMO_TROUBLESHOOTING.md](../DEMO_TROUBLESHOOTING.md).

## RViz opens but shows nothing / "Fixed Frame" error

- Set *Fixed Frame* to `odom` (or `base_link` if TF is not up yet).
- `ros2 run tf2_ros tf2_echo odom base_link` — if it times out, the TF tree is not
  being published; check that `safety_sim.launch.py` (or Gazebo) is running.

## Black viewport / GPU rendering errors

**Symptom** — viewport stays black, log shows `glXChooseFBConfig` / OGRE crashes.

- Software rendering fallback:

  ```bash
  docker exec safety-autonomy-demo bash -c \
      "export LIBGL_ALWAYS_SOFTWARE=1 && rviz2"
  ```

- NVIDIA: uncomment `/dev/dri` in `docker-compose.yml` `devices:` and ensure the
  `nvidia-container-toolkit` is installed on the host.

- Wayland hosts: XWayland must be running (`echo $XDG_SESSION_TYPE`); RViz is an
  X11 app.

## Topics exist but displays show "No messages"

- QoS mismatch: sensor topics use `best_effort`. In each RViz display, set
  *Reliability Policy* to **Best Effort** — the default Reliable subscription
  silently drops best-effort publishers like `/scan` and `/points`.
- `ros2 topic hz /scan` confirms data is actually flowing.

## RViz slow / laggy over X11

- Reduce point cloud density in the display, or downsample via
  `point_cloud_transport` plugins.
- Run RViz on the host against the container's topics (`ROS_DOMAIN_ID` must
  match, network_mode: host already shares the namespace).
