# Camera Streaming

The 3DS can publish JPEG-compressed camera frames and companion camera info on:

| Property | Value |
| --- | --- |
| ROS topic | `/nintendo_3ds/camera/image_raw/compressed` (default) |
| DDS topic | `rt/nintendo_3ds/camera/image_raw/compressed` |
| Type | `sensor_msgs/msg/CompressedImage` |
| QoS | Best effort, volatile, keep last 1 |
| Format | `bgr8; jpeg compressed bgr8` |
| CameraInfo topic | `/nintendo_3ds/camera/camera_info` (default) |
| CameraInfo DDS topic | `rt/nintendo_3ds/camera/camera_info` |
| CameraInfo type | `sensor_msgs/msg/CameraInfo` |
| CameraInfo QoS | Best effort, volatile, keep last 1 |

The topics are prefixed with the configured `ros_namespace` (`/nintendo_3ds` by default). The `format` field follows the standard `compressed_image_transport` contract, so ROS tools can decode the JPEG stream directly.

## Configuration

Camera streaming is disabled by default. Set these values in
`SD:/3ds/ros2_3ds_interface/config.ini` to enable it:

```ini
camera_enabled=1
camera_source=inner
camera_resolution=qqvga
camera_fps=5
camera_jpeg_quality=70
```

Supported sources are `inner`, `outer_left`, and `outer_right`. Supported
resolutions are `qqvga` (160x120) and `qvga` (320x240). Supported frame rates
are `5`, `10`, and `15`. JPEG quality is constrained to `40` through `85`.

QVGA at 15 FPS is experimental. The publisher uses best-effort delivery and
latest-frame behavior; a lost DDS fragment discards that JPEG frame instead of
delaying later frames.

## View It

Use a native Ubuntu Jazzy terminal with Cyclone DDS:

```sh
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
unset CYCLONEDDS_URI
ros2 daemon stop
ros2 topic info -v /nintendo_3ds/camera/image_raw/compressed
ros2 run rqt_image_view rqt_image_view --ros-args \
  -r image:=/nintendo_3ds/camera/image_raw \
  -p image_transport:=compressed
```

The Topics view on the 3DS exposes the stream state, published frame count,
latest JPEG size, and DDS match count. Its activate control pauses or resumes
an initialized camera publisher.

## Limits

The camera pipeline publishes JPEG-compressed frames and periodic `sensor_msgs/msg/CameraInfo` metadata (every 5 seconds). Camera publication is intentionally limited to approximately 1 FPS to protect Wi-Fi and socket stability, while local capture and bottom-screen preview continue at the configured frame rate. It does not publish raw `sensor_msgs/msg/Image`, true stereo calibration data, or synchronized stereo pairs.
