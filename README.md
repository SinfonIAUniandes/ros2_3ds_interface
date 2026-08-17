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

On a desktop ROS 2 installation using the same `ROS_DOMAIN_ID` as the 3DS, run:

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
log entry is the proof that the listener received external peer data. Chatter
has been validated bidirectionally with ROS 2 Jazzy and `rmw_cyclonedds_cpp`.

The top-screen `RTPS TXM:<n> TXU:<n> RX:<n> REM:<n>` row reports low-level
Cyclone DDS socket traffic: multicast sends, unicast sends, all received DDS
datagrams, and datagrams whose source IPv4 differs from the 3DS address.
`REM>0` proves that a remote DDS datagram reached the 3DS socket, while
`MATCH>0` proves that DDS parsed discovery data and matched an endpoint. The
`ERR TX:<n> RX:<n>` row exposes the latest non-transient socket errors, and
`QW:<n> QR:<n>` reports incompatible QoS rejections for the chatter writer and
reader. A nonzero QoS count is logged with the rejected policy ID.

## Network discovery

The distributed build contains no host address. It uses standard DDS multicast
discovery on `239.255.0.1` and also sends an undirected SPDP announcement to the
IPv4 subnet broadcast address calculated by `SOCU_GetIPInfo`. The broadcast uses
the domain metatraffic port (`7400 + 250 * domain_id`), which normal multicast-
enabled DDS participants already bind. A participant that receives it can reply
to the 3DS unicast locator. This allows ROS 2 machines on the same IPv4 LAN and
domain to join without changing or rebuilding the application, including on
networks that suppress multicast delivery to the 3DS.

Remove any previous `SD:/3ds/ros2_3ds_interface/config.ini`, or leave `peer_ip`
blank, to use automatic LAN discovery. In this mode, `TXU>0` confirms subnet
broadcast attempts and `REM>0` confirms that another machine replied.

On a normal host, clear old Cyclone overrides before testing:

```powershell
$env:ROS_DOMAIN_ID = "0"
Remove-Item Env:CYCLONEDDS_URI -ErrorAction SilentlyContinue
ros2 daemon stop
ros2 topic echo /chatter std_msgs/msg/String
```

Windows must classify the LAN as private and permit inbound ROS 2 UDP traffic.
Machines with VPN, Hyper-V, Docker, or WSL adapters may also require their RMW
configuration to select the physical LAN interface. That is host configuration
and never requires rebuilding the 3DS application.

The broadcast bootstrap is IPv4 and local-subnet only. It does not cross routers
or VLANs, and access points with client isolation can still block it.

## Static peer fallback

Some access points or host firewalls block multicast. In that case, create or
edit `SD:/3ds/ros2_3ds_interface/config.ini`; changing it requires no rebuild.
Set `peer_ip` to one physical ROS 2 host address. Invalid values are ignored and
the application falls back to multicast discovery.

When `peer_ip` is set, the 3DS uses `ParticipantIndex=auto` so discovery uses
the predictable RTPS unicast ports beginning at `7410`. A peer without an
explicit port cannot discover a participant using arbitrary unicast ports.

For example, put this in `SD:/3ds/ros2_3ds_interface/config.ini`:

```ini
peer_ip=192.0.2.10
port=17650
send_interval_ms=1000
domain_id=0
dds_enabled=1
```

Replace the documentation address with the host's current physical LAN IPv4.
Configure the host in the opposite direction using
`config/cyclonedds-static-peer.example.xml`: replace `192.0.2.10` with the host
address and `192.0.2.20` with the current 3DS address, then set
`CYCLONEDDS_URI` to that file. The predictable participant ports are enabled
automatically whenever `peer_ip` is set. This is direct SPDP discovery, not a
bridge or agent. WSL2 NAT addresses are not suitable peers.

## ROS graph discovery

The application publishes `ros_discovery_info` using the ROS 2
`ParticipantEntitiesInfo` type. Its single node entry describes
`/ros2_3ds_interface` and both `/chatter` endpoint GIDs, so `/chatter` should
be visible in `ros2 topic list` on a desktop ROS 2 host in the same domain.
The graph sample is published once when DDS starts and refreshed every five
seconds; `GRAPH P:<count>` reports successful graph publications and `GRAPH M:<count>`
is the remote `ros_discovery_info` subscriber match count.

The graph and chatter paths are validated with `rmw_cyclonedds_cpp`. Other ROS
2 RMW implementations should interoperate through RTPS, but remain separate
compatibility targets because discovery metadata and type compatibility are
implementation-sensitive.

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

Hardware logs must show build ID `20260816-subnet-discovery` exactly; otherwise,
the SD card contains a stale `.3dsx`.

For post-run diagnosis, open this SD card path in a file manager or on a PC:

```text
/3ds/ros2_3ds_interface/logs/YYYYMMDD/
```

The gate passes when the screen reports a successful multicast membership and
the transmit counter increases without socket errors. Bidirectional chatter,
static-peer discovery, and ROS graph discovery have been validated with a
desktop Cyclone DDS RMW.

## Scope

ROS 2 services remain pending. Communication remains direct DDS/RTPS without
a Micro XRCE-DDS Agent or another bridge.


https://github.com/SinfonIAUniandes/cyclone3dds