# ROS 2 3DS Interface

Native ROS 2/DDS experiments for Nintendo 3DS homebrew. The application links
the local Cyclone DDS 3DS port and creates a native DDS participant after
initializing `soc:u`.

## ROS 2 chatter

This build creates a ROS 2-compatible `/chatter` publisher/subscriber endpoint.
It maps to the DDS topic `rt/chatter` with the exact type
`std_msgs::msg::dds_::String_` (`std_msgs/msg/String`). It uses the generated
`std_msgs_string` Cyclone DDS IDL descriptor, including its XTypes metadata,
rather than a hand-written descriptor. The endpoint uses
KEEP_LAST(10), RELIABLE, and VOLATILE QoS. The reader intentionally ignores
data from its local DDS participant, so ROS RX messages always represent data
from an external peer rather than the 3DS publishing to itself.

Controls on the 3DS are:

- A: publish one `Hello from 3DS: <counter>` message
- B: toggle automatic chatter publishing at 1 Hz
- Y: toggle the ROS listener
- X: send the existing UDP diagnostic probe immediately
- START: exit

For a future desktop ROS 2 interoperability check using the same
`ROS_DOMAIN_ID` as the 3DS, run:

```sh
export ROS_DOMAIN_ID=0
ros2 topic echo /chatter std_msgs/msg/String
ros2 topic pub /chatter std_msgs/msg/String '{data: hello}'
```

For an explicit remote listener test, start the app with the listener enabled,
then run on the desktop:

```sh
ros2 topic pub --rate 1 /chatter std_msgs/msg/String '{data: desktop to 3ds}'
```

The 3DS log should show `ROS REMOTE RX`. `MATCH W:<n> R:<n>` reports DDS
endpoint discovery; matching alone is not listener proof. A `ROS REMOTE RX`
log entry is the proof that the listener received external peer data. Desktop
ROS discovery has not been validated by this project.

The top-screen `RTPS TXM:<n> TXU:<n> RX:<n> REM:<n>` row reports low-level
Cyclone DDS socket traffic: multicast sends, unicast sends, all received DDS
datagrams, and datagrams whose source IPv4 differs from the 3DS address.
`REM>0` proves that a remote DDS datagram reached the 3DS socket, while
`MATCH>0` proves that DDS parsed discovery data and matched an endpoint. The
`ERR TX:<n> RX:<n>` row exposes the latest non-transient socket errors, and
`QW:<n> QR:<n>` reports incompatible QoS rejections for the chatter writer and
reader. A nonzero QoS count is logged with the rejected policy ID.

## Static peer fallback

The runtime always creates a generic Cyclone DDS domain configured for RTPS
2.1 compatibility. The built-in `romfs/config.ini` supplies defaults. After
the first launch, create or edit `SD:/3ds/ros2_3ds_interface/config.ini` to
override them; changing this external file requires no rebuild. No `peer_ip`
is needed on normal LANs: leave it blank to use standard multicast discovery.
Set `peer_ip` only when network equipment filters multicast discovery, using
the physical LAN IPv4 address of a DDS peer, such as the Windows adapter
address `192.168.1.6`. This adds that address as a direct Cyclone DDS SPDP
discovery peer and continues to use it for the existing UDP probe. Invalid
values are ignored and multicast discovery is used instead.

When `peer_ip` is set, the 3DS uses `ParticipantIndex=auto` so discovery uses
the predictable RTPS unicast ports beginning at `7410`. A peer without an
explicit port cannot discover a participant using arbitrary unicast ports.

For example, put this in `SD:/3ds/ros2_3ds_interface/config.ini`:

```ini
peer_ip=192.168.1.6
port=17650
send_interval_ms=1000
domain_id=0
dds_enabled=1
```

Do not use a WSL `192.168.184.x` address. Run `ipconfig` on Windows and enter
the physical Wi-Fi IPv4 address into `peer_ip`. This is direct SPDP peer
discovery between DDS participants, not a bridge or agent.

For the Windows Wi-Fi addresses used by this build, set the same configuration
inline from PowerShell before starting any ROS process. This avoids Windows/WSL
file URI conversion:

```powershell
$env:RMW_IMPLEMENTATION = "rmw_cyclonedds_cpp"
$env:ROS_DOMAIN_ID = "0"
$env:CYCLONEDDS_URI = '<CycloneDDS><Domain Id="any"><General><Interfaces><NetworkInterface address="192.168.1.6" /></Interfaces><AllowMulticast>false</AllowMulticast></General><Discovery><ParticipantIndex>auto</ParticipantIndex><MaxAutoParticipantIndex>9</MaxAutoParticipantIndex><Peers><Peer Address="192.168.1.2" /></Peers></Discovery></Domain></CycloneDDS>'
ros2 daemon stop
ros2 topic echo /chatter std_msgs/msg/String
```

The equivalent formatted XML is available in `config/cyclonedds-windows.xml`.
This configuration selects the physical `192.168.1.6` Wi-Fi interface, disables
multicast for the test, and directly peers with the 3DS at `192.168.1.2`. WSL2
remains unsuitable for this direct test because its `192.168.184.x` interface
is behind NAT.

## ROS graph discovery

The application publishes `ros_discovery_info` using the ROS 2
`ParticipantEntitiesInfo` type. Its single node entry describes
`/ros2_3ds_interface` and both `/chatter` endpoint GIDs, so `/chatter` should
be visible in `ros2 topic list` on a desktop ROS 2 host in the same domain.
The graph sample is published once when DDS starts and refreshed every five
seconds; `GRAPH P:<count>` reports successful graph publications and `GRAPH M:<count>`
is the remote `ros_discovery_info` subscriber match count.

This interoperability path still requires validation with a desktop ROS RMW,
because discovery metadata and type compatibility are implementation-sensitive.
WSL addresses in `192.168.184.x` are not on the same LAN as a 3DS at
`192.168.1.x`, so WSL cannot validate LAN DDS discovery. Use the Windows host's
physical Wi-Fi connection or a native Linux host on `192.168.1.x` instead.

## Current probe

The application currently verifies these capabilities:

- `socInit` and local IPv4 discovery
- UDP socket creation and non-blocking receive
- bind to a separate UDP probe port, default `17650`, outside the RTPS port range
- join and leave the RTPS multicast group `239.255.0.1`
- multicast send and receive
- optional unicast send and receive against a configured peer
- Cyclone DDS multi-iovec UDP sends, with fragments concatenated for libctru `sendto`
- persistent application, network, and Cyclone DDS logs on the bottom screen

The top screen is a stable status view refreshed twice per second. The bottom
screen retains only the 16 most recent operational events, so DDS diagnostics
cannot grow the console memory indefinitely. Persistent logs are organized as:

```text
sdmc:/3ds/ros2_3ds_interface/logs/YYYYMMDD/session-HHMMSS-mmm.log
sdmc:/3ds/ros2_3ds_interface/logs/YYYYMMDD/errors/error-<timestamp>-<sequence>.log
```

Every application launch creates a new session log. Each `ERR` event creates an
independent error snapshot containing the error and the recent event context.
At the first launch on a new day, the previous day's log directory is removed.

## Logging Layout

The logging module is isolated from UI, network, and DDS lifecycle code:

```text
include/logging/app_log.h
source/logging/app_log.c
```

It owns the in-memory circular buffer, bottom-screen rendering, SD directory
creation, and persistent file output. The application and DDS runtime only emit
structured events through its public API. Cyclone DDS trace output is not sent
to the console; only normal informational messages, warnings, and errors are
captured, keeping the diagnostics readable.

The Cyclone DDS 3DS socket layer attempts native multicast interface selection
using libctru socket option 32. When the SOC service rejects that option, the
backend records its errno as `MIF` and continues with the only active Wi-Fi
route. Multicast group membership is still independently checked through
`IP_ADD_MEMBERSHIP`. The independent probe selects the local interface through
`ip_mreq.imr_interface` when joining.

## Build

Build `libddsc.a` from the sibling Cyclone port first:

```sh
cd ../cyclonedds_3ds
cmake --build build-3ds --target ddsc
```

Then build the 3DS application:

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
make
```

The output is `ros2_3ds_interface.3dsx`.

## Hardware test

Launch the `.3dsx` on real hardware and verify the socket, bind, and multicast
membership results on screen. The `DDS` line must report `RUNNING` with return
code `0`; this confirms that the static Cyclone runtime created a participant
and its `/chatter` endpoints directly on the 3DS. Press A to publish chatter,
B to toggle 1 Hz chatter, Y to toggle the ROS listener, and X to transmit the
independent UDP probe. The endpoint `MATCH W:<n> R:<n>` counts show DDS
endpoint discovery; `ROS REMOTE RX` confirms that the external listener test
delivered data to the 3DS.

Hardware logs must show build ID `20260816-static-peer` exactly; otherwise,
the SD card contains a stale `.3dsx`.

For post-run diagnosis, open this SD card path in a file manager or on a PC:

```text
/3ds/ros2_3ds_interface/logs/YYYYMMDD/
```

The gate passes when the screen reports a successful multicast membership and
the transmit counter increases without socket errors. Receiving multicast,
validating an optional peer, and validating ROS graph discovery with a desktop
ROS RMW remain hardware/network integration checks.

## Scope

ROS 2 services remain pending. Communication remains direct DDS/RTPS without
a Micro XRCE-DDS Agent or another bridge.
