# Camera Streaming

The 3DS supports two independent, toggleable camera topics for front (inner) and back (outer) cameras:

| Stream | ROS topic | DDS topic | Type | Optical Frame | Default |
| --- | --- | --- | --- | --- | --- |
| **Front Image** | `/nintendo_3ds/camera/front/image_raw/compressed` | `rt/nintendo_3ds/camera/front/image_raw/compressed` | `sensor_msgs/msg/CompressedImage` | `3ds_inner_camera_optical_frame` | **OFF** |
| **Front Info** | `/nintendo_3ds/camera/front/camera_info` | `rt/nintendo_3ds/camera/front/camera_info` | `sensor_msgs/msg/CameraInfo` | `3ds_inner_camera_optical_frame` | **OFF** |
| **Back Image** | `/nintendo_3ds/camera/back/image_raw/compressed` | `rt/nintendo_3ds/camera/back/image_raw/compressed` | `sensor_msgs/msg/CompressedImage` | `3ds_outer_left_camera_optical_frame` | **OFF** |
| **Back Info** | `/nintendo_3ds/camera/back/camera_info` | `rt/nintendo_3ds/camera/back/camera_info` | `sensor_msgs/msg/CameraInfo` | `3ds_outer_left_camera_optical_frame` | **OFF** |

The topics are prefixed with the configured `ros_namespace` (`/nintendo_3ds` by default). Both image topics use QoS **Best effort, volatile, keep last 1**, and the `format` field follows the standard `compressed_image_transport` contract (`bgr8; jpeg compressed bgr8`) so standard ROS tools can decode the JPEG streams directly.

## Configuration

Both camera streams are disabled by default. Configure them in `SD:/3ds/ros2_3ds_interface/config.ini`:

```ini
camera_front_enabled=0
camera_back_enabled=0
camera_source=outer_left
camera_resolution=qqvga
camera_fps=5
camera_jpeg_quality=70
```

- `camera_front_enabled`: Enables the front (inner) camera stream at startup (`0` or `1`, default `0`).
- `camera_back_enabled`: Enables the back (outer) camera stream at startup (`0` or `1`, default `0`).
- `camera_source`: Back camera sensor selection: `outer_left` (default) or `outer_right`.
- `camera_resolution`: `qqvga` (160x120) or `qvga` (320x240).
- `camera_fps`: Hardware capture rate (`5`, `10`, or `15`).
- `camera_jpeg_quality`: JPEG compression quality (`40` through `85`, default `70`).

## In-App Controls & Live Preview

The **Topics** view lists both camera streams. Selecting a camera row:
- Shows the current publishing status (`ON` or `OFF`).
- Renders live capture, encoded, and match metrics on the bottom screen.
- Displays a real-time **LIVE FEED** preview on the bottom screen from the selected camera's separate preview buffer.
- Pressing `A` (Activate) toggles that specific camera stream on or off independently.

## Hardware Port Sharing Note

In the Nintendo 3DS hardware architecture (`cam:u`), the inner front camera (`IN1`) and outer back camera (`OUT1`) share physical DMA port `PORT_CAM1`. When only one camera is enabled, capture runs continuously from that sensor. When both camera streams are toggled ON simultaneously, the runtime alternates capture frames between the front and back sensors. When both streams are OFF, hardware capture is stopped to conserve battery and CPU resources.

## View It

Use a native Ubuntu Jazzy terminal with Cyclone DDS:

### View Front Camera
```sh
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
unset CYCLONEDDS_URI
ros2 daemon stop
ros2 topic info -v /nintendo_3ds/camera/front/image_raw/compressed
ros2 run rqt_image_view rqt_image_view --ros-args \
  -r image:=/nintendo_3ds/camera/front/image_raw \
  -p image_transport:=compressed
```

### View Back Camera
```sh
ros2 topic info -v /nintendo_3ds/camera/back/image_raw/compressed
ros2 run rqt_image_view rqt_image_view --ros-args \
  -r image:=/nintendo_3ds/camera/back/image_raw \
  -p image_transport:=compressed
```

## Limits

The camera pipeline publishes JPEG-compressed frames and periodic `sensor_msgs/msg/CameraInfo` metadata (every 5 seconds). Camera publication is intentionally limited to approximately 1 FPS to protect Wi-Fi and socket stability, while local capture and bottom-screen preview continue at the configured frame rate. It does not publish raw `sensor_msgs/msg/Image`, true stereo calibration data, or synchronized stereo pairs.
