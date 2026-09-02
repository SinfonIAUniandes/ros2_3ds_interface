# ROS 2 3DS Interface

A native ROS 2 publisher and subscriber for Nintendo 3DS homebrew. The app
connects directly to DDS over Wi-Fi, publishes and receives
`std_msgs/msg/String` on `/chatter`, streams `sensor_msgs/msg/Imu` on
`/imu/data_raw`, and appears in the ROS 2 graph without an agent or bridge.

The primary compatibility target is native Ubuntu with ROS 2 Jazzy and
`rmw_cyclonedds_cpp`. Native Windows remains supported on a best-effort basis,
but it is not the reference environment for discovery debugging.

## Requirements

- A Nintendo 3DS with access to the Homebrew Launcher
- Both devices connected to the same IPv4 LAN
- A ROS 2 installation on the computer
- Matching `ROS_DOMAIN_ID` values; the default is `0`
- Inbound ROS 2 UDP traffic allowed by the host firewall

The 3DS build uses the Nintendo 3DS Cyclone DDS port from
[SinfonIAUniandes/cyclone3dds](https://github.com/SinfonIAUniandes/cyclone3dds).
That specific fork is required when building from source.

## Install

1. Copy `ros2_3ds_interface.3dsx` to:

   ```text
   SD:/3ds/ros2_3ds_interface/ros2_3ds_interface.3dsx
   ```

2. Launch **ROS 2 3DS Interface** from the Homebrew Launcher.
3. Confirm that the top screen reports `DDS RUNNING`.
4. Start a ROS 2 process on a computer connected to the same LAN.

Ubuntu reference setup:

```sh
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
unset CYCLONEDDS_URI
ros2 daemon stop
ros2 topic echo /nintendo_3ds/chatter std_msgs/msg/String
```

PowerShell, best effort:

```powershell
$env:ROS_DOMAIN_ID = "0"
$env:RMW_IMPLEMENTATION = "rmw_cyclonedds_cpp"
Remove-Item Env:CYCLONEDDS_URI -ErrorAction SilentlyContinue
ros2 pkg prefix rmw_cyclonedds_cpp
ros2 daemon stop
ros2 doctor --report | Select-String -Pattern "middleware|rmw"
ros2 topic echo /nintendo_3ds/chatter std_msgs/msg/String
```

Run every ROS command from that same PowerShell session. If
`rmw_cyclonedds_cpp` is unavailable in the Pixi environment, use native Ubuntu
for service testing rather than falling back to Fast DDS.

Press **A** on the 3DS. The computer should receive a `Hello from 3DS`
message.

To send data to the 3DS:

```sh
ros2 topic pub --rate 1 /nintendo_3ds/chatter std_msgs/msg/String '{data: Hello from ROS 2}'
```

The 3DS log should display `ROS REMOTE RX`.

To inspect the motion sensors:

```sh
ros2 topic echo /nintendo_3ds/imu/data_raw sensor_msgs/msg/Imu
ros2 topic hz /nintendo_3ds/imu/data_raw
```

The optional JPEG camera stream publishes on `/camera/image_raw/compressed`.
Configure it on the SD card and view it with standard ROS image tools; see
[Camera streaming](docs/features/camera-streaming.md).

To call the built-in service:

```sh
ros2 service list -t
ros2 service call /nintendo_3ds/add_two_ints example_interfaces/srv/AddTwoInts "{a: 2, b: 3}"
```

## Controls

| Default binding | Action |
| --- | --- |
| A | Publish every enabled topic once from Home; enable or disable the selected Topic |
| B | Start or stop all enabled publishers from Home; return from Menu subviews |
| Y | Toggle the subscriber from Home |
| X | Send a diagnostic UDP probe from Home |
| L / R | Change main tab |
| Up / Down | Select Menu items or Topics |
| START | Exit |

Bindings are editable before building in `romfs/ui/controls.ini`, or can be
overridden from `SD:/3ds/ros2_3ds_interface/controls.ini`.

The Topics tab controls publisher availability independently. Select a topic
with Up/Down and use the activation binding (A by default) to enable or disable
its publisher. Disabling a topic keeps its DDS endpoint available for discovery
but stops its samples from being published.

## Network Setup

No computer address is compiled into the app. It automatically uses DDS
multicast and local-subnet broadcast discovery. Most machines on the same LAN
only need the correct domain ID.

If automatic discovery is blocked by the network, configure a static peer on
the SD card without rebuilding the app. See the
[configuration guide](docs/guides/configuration.md).

The same SD-card configuration selects the DDS domain with `domain_id` and the
ROS graph namespace with `ros_namespace`. The built-in defaults are domain `0`
and namespace `/nintendo_3ds`. The namespace prefixes every ROS endpoint, so
the default topics include `/nintendo_3ds/chatter`,
`/nintendo_3ds/imu/data_raw`, and
`/nintendo_3ds/camera/image_raw/compressed`.

WSL2 in its default NAT mode is not expected to participate directly in LAN
DDS discovery. Use native Windows ROS 2, mirrored networking, or a native Linux
host instead.

Fast DDS can discover the node and topics, but its ROS service request/reply
mapping is not currently implemented by the 3DS server. Service tests require
`rmw_cyclonedds_cpp` on the host.

## Build From Source

Building requires devkitPro/devkitARM, libctru, CMake, and the
[cyclone3dds fork](https://github.com/SinfonIAUniandes/cyclone3dds). Follow the
[building guide](docs/guides/building.md).

## Documentation

Start with the [documentation index](docs/README.md).

- [Architecture overview](docs/architecture/overview.md)
- [Network discovery](docs/architecture/network-discovery.md)
- [ROS 2 integration](docs/architecture/ros-integration.md)
- [UI system](docs/architecture/ui-system.md)
- [Runtime configuration](docs/guides/configuration.md)
- [Building from source](docs/guides/building.md)
- [Diagnostics and troubleshooting](docs/guides/troubleshooting.md)

## Current Scope

- Namespaced chatter publisher and subscriber using `std_msgs/msg/String`
- Namespaced IMU publisher using `sensor_msgs/msg/Imu`
- Namespaced JPEG camera publisher using `sensor_msgs/msg/CompressedImage`
- Namespaced AddTwoInts server using `example_interfaces/srv/AddTwoInts`
- ROS 2 graph publication for the 3DS node and endpoints
- IPv4 UDP transport on the local network
- Multicast, subnet-broadcast, and optional static-peer discovery
- Persistent diagnostic logs on the SD card

See [IMU streaming](docs/features/imu-streaming.md) for units, frame semantics,
frequency, and calibration details.

See [Camera streaming](docs/features/camera-streaming.md) for camera settings
and host-side viewing.

See [AddTwoInts service](docs/features/add-two-ints-service.md) for the service
wire mapping and validation steps.

Actions, parameters, lifecycle nodes, IPv6, DDS Security, and routed discovery
are not currently implemented.