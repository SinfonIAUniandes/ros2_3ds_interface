# Diagnostics and Troubleshooting

## Success Criteria

A complete bidirectional test shows:

1. `DDS RUNNING` with return code `0`.
2. `REM` increases, proving remote DDS packets reached the 3DS.
3. `MATCH W` or `MATCH R` becomes greater than zero.
4. The host receives `Hello from 3DS`.
5. The 3DS logs `ROS REMOTE RX` for host data.

## On-Screen Diagnostic Fields

The user interface organizes status indicators across the **Home** dashboard and the **Details** diagnostic view (accessible via **Menu** -> **Details**):

### Home View (Top Screen)

| Card | Field | Meaning |
| --- | --- | --- |
| `CONNECTION` | IP & Badge | Local IPv4 address and `ONLINE` / `OFFLINE` status |
| `CONNECTION` | Domain / Mode | Configured DDS domain ID and static peer vs automatic discovery |
| `DDS RUNTIME` | Status & Badge | Participant lifecycle state (`READY` / `STOPPED`) |
| `DDS RUNTIME` | Result / Graph | Last DDS return code and matched graph readers |
| `CHATTER PUBLISHER` | SENT / RECEIVED | Successfully transmitted and received chatter sample counts |
| `CHATTER PUBLISHER` | pub / sub | Matched chatter publisher (writer) and subscriber (reader) count |
| `PUBLISHING` | Status Badges | Global periodic publisher (`ALL ON`/`ALL OFF`) and subscriber state |
| `PUBLISHING` | IMU / RTPS | IMU enabled state and incoming remote RTPS packet count |

### Details View (Menu -> Details)

| Panel | Field | Meaning |
| --- | --- | --- |
| `NETWORK & DISCOVERY` | Local IP / Netmask / Broadcast | Active IPv4 network configuration from `soc:u` |
| `NETWORK & DISCOVERY` | Peer / Config | Static peer IPv4 address or `automatic`, and config source (SD or ROMFS) |
| `NETWORK & DISCOVERY` | Probe port | UDP diagnostic probe port (default 17650) |
| `RTPS TRANSPORT` | TX multicast / TX unicast | Datagrams transmitted via multicast or unicast/broadcast |
| `RTPS TRANSPORT` | RX total / RX remote | Total received DDS datagrams and datagrams from remote peers |
| `RTPS TRANSPORT` | Socket errno | Last non-transient socket send/receive errno |
| `RTPS TRANSPORT` | Mcast IF | Result errno of setting `IP_MULTICAST_IF` (libctru fallback is normal) |
| `PROBE & ENDPOINTS` | Probe packets | Probe multicast, unicast, and received packet counts |
| `PROBE & ENDPOINTS` | Last sender / payload | IPv4 address and snippet of last received diagnostic probe |
| `PROBE & ENDPOINTS` | QoS policy | Last offered / requested incompatible QoS policy IDs |
| `PROBE & ENDPOINTS` | Bind/join/loop | Probe socket bind, group membership, and loopback error codes |

## Persistent Logs

Session logs are written to:

```text
SD:/3ds/ros2_3ds_interface/logs/YYYYMMDD/session-HHMMSS-mmm.log
```

Each error also creates a snapshot containing recent events:

```text
SD:/3ds/ros2_3ds_interface/logs/YYYYMMDD/errors/
```

The startup line contains a build ID. Check it first when behavior does not
match the source tree; a different ID means the SD card contains an older
`.3dsx`.

## Common Failures

### `TX multicast` increases but `RX total` remains zero

The 3DS is transmitting but receives no DDS traffic. Check the host firewall,
LAN privacy profile, AP client isolation, and selected host interface. In
automatic mode, `TX unicast` should also increase because the app sends subnet-
broadcast SPDP.

### `TX unicast` increases but `RX remote` remains zero

Broadcast left the 3DS, but no host replied. Confirm that a ROS 2 participant is
running in the same domain. If the LAN filters broadcast, use static peers on
both ends as described in the configuration guide.

### `RX remote` increases but matches stay at zero

Transport works, but discovery or endpoint compatibility failed. Check
`ROS_DOMAIN_ID`, QoS policy rejections (`QoS policy` in Details), RMW selection,
topic type, and Cyclone DDS logs.

### Matches appear and disappear

ROS command-line processes create short-lived DDS participants. Match counts
can return to zero when `ros2 topic echo`, `ros2 topic pub`, or the ROS daemon
exits or restarts.

### Service endpoints match but requests do not deserialize

Check the vendor ID in the 3DS session log. `vendor 1.15` is eProsima Fast DDS;
`vendor 1.16` is Eclipse Cyclone DDS. The current AddTwoInts server supports the
`rmw_cyclonedds_cpp` request/reply mapping only. Fast DDS may still discover the
node, topics, and service graph even though service calls are incompatible.

Verify the selected RMW in the same shell that launches the client:

```sh
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
ros2 pkg prefix rmw_cyclonedds_cpp
ros2 daemon stop
ros2 doctor --report | grep -Ei 'middleware|rmw'
```

### WSL2 cannot discover the 3DS

Default WSL2 networking is NATed and does not expose DDS multicast or broadcast
as a normal LAN interface. Use native Windows ROS 2, WSL mirrored networking,
or a native Linux host.

## Packet Capture

On a native Linux host, capture domain-0 discovery and initial unicast ports:

```sh
sudo tcpdump -ni <lan-interface> 'udp port 7400 or udp portrange 7410-7429'
```

For other domain IDs, account for the `250 * domain_id` port offset.