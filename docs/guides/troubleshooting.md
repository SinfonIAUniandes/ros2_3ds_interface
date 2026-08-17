# Diagnostics and Troubleshooting

## Success Criteria

A complete bidirectional test shows:

1. `DDS RUNNING` with return code `0`.
2. `REM` increases, proving remote DDS packets reached the 3DS.
3. `MATCH W` or `MATCH R` becomes greater than zero.
4. The host receives `Hello from 3DS`.
5. The 3DS logs `ROS REMOTE RX` for host data.

## Top-Screen Fields

| Field | Meaning |
| --- | --- |
| `ROS TX/RX` | Successfully written and received chatter samples |
| `GRAPH P/M` | Graph publications and matched graph readers |
| `MATCH W/R` | Matched chatter readers and writers |
| `UDP M/U/RX` | Independent probe counters |
| `RTPS TXM` | DDS multicast datagrams sent |
| `RTPS TXU` | DDS unicast or subnet-broadcast datagrams sent |
| `RTPS RX` | All DDS datagrams received |
| `RTPS REM` | DDS datagrams received from another IPv4 address |
| `ERR TX/RX` | Last non-transient DDS socket errno |
| `MIF` | Result of the platform multicast-interface option |
| `QW/QR` | Writer and reader incompatible-QoS counts |

An `MIF` warning can be expected on libctru. The backend falls back to the
active Wi-Fi route; it is not fatal when the participant still starts.

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

### `TXM` increases but `RX` remains zero

The 3DS is transmitting but receives no DDS traffic. Check the host firewall,
LAN privacy profile, AP client isolation, and selected host interface. In
automatic mode, `TXU` should also increase because the app sends subnet-
broadcast SPDP.

### `TXU` increases but `REM` remains zero

Broadcast left the 3DS, but no host replied. Confirm that a ROS 2 participant is
running in the same domain. If the LAN filters broadcast, use static peers on
both ends as described in the configuration guide.

### `REM` increases but matches stay at zero

Transport works, but discovery or endpoint compatibility failed. Check
`ROS_DOMAIN_ID`, `QW/QR`, RMW selection, topic type, and Cyclone DDS logs.

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