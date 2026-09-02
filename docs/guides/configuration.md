# Runtime Configuration

The application loads defaults from ROMFS and then checks for an optional file
on the SD card:

```text
SD:/3ds/ros2_3ds_interface/config.ini
```

The SD-card file overrides only the keys it contains. Configuration changes do
not require rebuilding the application.

## Options

| Key | Default | Description |
| --- | --- | --- |
| `peer_ip` | empty | Optional physical LAN IPv4 address of a DDS peer |
| `port` | `17650` | Independent diagnostic probe port |
| `send_interval_ms` | `1000` | Automatic chatter publication interval |
| `domain_id` | `0` | DDS/ROS domain, from 0 through 232 |
| `dds_enabled` | `1` | Enables or disables the DDS runtime |
| `imu_enabled` | `1` | Enables the motion sensors and IMU writer |
| `imu_publish_hz` | `50` | IMU publication rate, from 1 through 100 Hz |
| `imu_accel_mps2_per_count` | `0.01915361328125` | Accelerometer calibration scale |
| `camera_enabled` | `0` | Enables the JPEG camera stream |
| `camera_source` | `inner` | `inner`, `outer_left`, or `outer_right` |
| `camera_resolution` | `qqvga` | `qqvga` or `qvga` |
| `camera_fps` | `5` | `5`, `10`, or `15` FPS |
| `camera_jpeg_quality` | `70` | JPEG quality, from 40 to 85 |

Leave `peer_ip` empty for automatic multicast and subnet-broadcast discovery.

## Static Peer Example

Use a static peer only when the LAN blocks automatic discovery:

```ini
peer_ip=192.0.2.10
port=17650
send_interval_ms=1000
domain_id=0
dds_enabled=1
imu_enabled=1
imu_publish_hz=50
imu_accel_mps2_per_count=0.01915361328125
camera_enabled=0
camera_source=inner
camera_resolution=qqvga
camera_fps=5
camera_jpeg_quality=70
```

Replace `192.0.2.10` with the physical LAN address of one ROS 2 host. Do not
use a WSL2 NAT or virtual-adapter address.

Configure the host in the opposite direction using
`config/cyclonedds-static-peer.example.xml`:

1. Replace `192.0.2.10` with the host LAN address.
2. Replace `192.0.2.20` with the 3DS LAN address shown on screen.
3. Set `CYCLONEDDS_URI` to the edited file.
4. Restart the ROS daemon and ROS processes.

PowerShell example:

```powershell
$path = (Resolve-Path .\config\cyclonedds-static-peer.example.xml).Path -replace '\\', '/'
$env:CYCLONEDDS_URI = "file:///$path"
ros2 daemon stop
```

## Host Requirements

All peers must use the same `ROS_DOMAIN_ID`. On Windows, classify the LAN as
private and allow inbound ROS 2 UDP traffic. A firewall rule can be created from
an elevated PowerShell session:

```powershell
New-NetFirewallRule -DisplayName "ROS 2 DDS LAN" `
  -Direction Inbound -Action Allow -Protocol UDP `
  -LocalPort 7400-7500 -Profile Private
```

For domains whose calculated RTPS ports fall outside this range, adjust the
rule accordingly.