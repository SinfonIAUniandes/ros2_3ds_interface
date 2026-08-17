# IMU Streaming

The application publishes the Nintendo 3DS accelerometer and gyroscope using
the standard ROS 2 message `sensor_msgs/msg/Imu`.

## ROS Interface

| Property | Value |
| --- | --- |
| ROS topic | `/imu/data_raw` |
| DDS topic | `rt/imu/data_raw` |
| Type | `sensor_msgs/msg/Imu` |
| Frame ID | `3ds_imu_link` |
| QoS | Best effort, volatile, keep last 5 |
| Default rate | 50 Hz |

The generated Cyclone DDS descriptor includes XTypes TypeInformation and
TypeMapping for `Imu`, `Header`, `Time`, `Quaternion`, and `Vector3`.

## Fields and Units

- `angular_velocity` is published in radians per second.
- `linear_acceleration` is published in meters per second squared.
- `orientation` is not estimated. `orientation_covariance[0]` is `-1`, which
  tells ROS consumers to ignore the orientation field.
- Angular velocity and acceleration covariance matrices are zero because their
  covariance is unknown, while the measurements themselves remain valid.

The gyroscope uses the device-specific conversion returned by
`HIDUSER_GetGyroscopeRawToDpsCoefficient`, then converts degrees per second to
radians per second.

The default accelerometer conversion assumes 512 HID counts per standard
gravity:

$$
scale = \frac{9.80665}{512}\;\mathrm{m/s^2/count}
$$

This value can be calibrated per device through runtime configuration.

## Sensor Lifecycle

The IMU module:

1. Enables the accelerometer and gyroscope through `HIDUSER`.
2. Reads the gyroscope conversion coefficient.
3. Creates the DDS topic and best-effort writer.
4. Reads the current HID samples after `hidScanInput`.
5. Publishes at the configured rate.
6. Disables both sensors during shutdown.

The accelerometer and gyroscope HID buffers are updated independently, so a
single message contains the latest available sample from each sensor rather
than a hardware-synchronized pair.

## Configuration

These keys are available in the normal SD-card configuration:

```ini
imu_enabled=1
imu_publish_hz=50
imu_accel_mps2_per_count=0.01915361328125
```

`imu_publish_hz` accepts values from 1 through 100. Set `imu_enabled=0` to omit
the writer and avoid enabling the motion sensors.

## UI

Select `/imu/data_raw` in the Topics tab with the configured previous/next item
controls. The detail pane shows:

- current angular velocity vector in rad/s
- current linear acceleration vector in m/s²
- publication rate
- transmitted sample count
- matched reader count

## Validation

On a ROS 2 host in the same domain:

```sh
ros2 topic list -t
ros2 topic info -v /imu/data_raw
ros2 topic echo /imu/data_raw sensor_msgs/msg/Imu
ros2 topic hz /imu/data_raw
```

At rest, the magnitude of linear acceleration should be close to standard
gravity after calibration. Rotate the console around each physical axis and
verify signs before using the data in control or estimation software; libctru
does not document a complete REP-103 axis transform for the console.