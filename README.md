# ROS 2 3DS Interface

A native ROS 2 publisher and subscriber for Nintendo 3DS homebrew. The app
connects directly to DDS over Wi-Fi, publishes and receives
`std_msgs/msg/String` on `/chatter`, and appears in the ROS 2 graph without an
agent or bridge.

Bidirectional communication has been validated with ROS 2 Jazzy and
`rmw_cyclonedds_cpp` on Windows.

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

PowerShell:

```powershell
$env:ROS_DOMAIN_ID = "0"
$env:RMW_IMPLEMENTATION = "rmw_cyclonedds_cpp"
Remove-Item Env:CYCLONEDDS_URI -ErrorAction SilentlyContinue
ros2 daemon stop
ros2 topic echo /chatter std_msgs/msg/String
```

Linux:

```sh
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
unset CYCLONEDDS_URI
ros2 daemon stop
ros2 topic echo /chatter std_msgs/msg/String
```

Press **A** on the 3DS. The computer should receive a `Hello from 3DS`
message.

To send data to the 3DS:

```sh
ros2 topic pub --rate 1 /chatter std_msgs/msg/String '{data: Hello from ROS 2}'
```

The 3DS log should display `ROS REMOTE RX`.

## Controls

| Button | Action |
| --- | --- |
| A | Publish one message |
| B | Toggle publishing at 1 Hz |
| Y | Toggle the subscriber |
| X | Send a diagnostic UDP probe |
| START | Exit |

## Network Setup

No computer address is compiled into the app. It automatically uses DDS
multicast and local-subnet broadcast discovery. Most machines on the same LAN
only need the correct domain ID.

If automatic discovery is blocked by the network, configure a static peer on
the SD card without rebuilding the app. See the
[configuration guide](docs/guides/configuration.md).

WSL2 in its default NAT mode is not expected to participate directly in LAN
DDS discovery. Use native Windows ROS 2, mirrored networking, or a native Linux
host instead.

## Build From Source

Building requires devkitPro/devkitARM, libctru, CMake, and the
[cyclone3dds fork](https://github.com/SinfonIAUniandes/cyclone3dds). Follow the
[building guide](docs/guides/building.md).

## Documentation

Start with the [documentation index](docs/README.md).

- [Architecture overview](docs/architecture/overview.md)
- [Network discovery](docs/architecture/network-discovery.md)
- [ROS 2 integration](docs/architecture/ros-integration.md)
- [Runtime configuration](docs/guides/configuration.md)
- [Building from source](docs/guides/building.md)
- [Diagnostics and troubleshooting](docs/guides/troubleshooting.md)

## Current Scope

- `/chatter` publisher and subscriber using `std_msgs/msg/String`
- ROS 2 graph publication for the 3DS node and endpoints
- IPv4 UDP transport on the local network
- Multicast, subnet-broadcast, and optional static-peer discovery
- Persistent diagnostic logs on the SD card

Services, actions, IPv6, DDS Security, and routed discovery are not currently
implemented.