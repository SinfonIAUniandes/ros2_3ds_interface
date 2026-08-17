# Network Discovery

The application combines standard DDS discovery with an IPv4 LAN bootstrap so
the distributed `.3dsx` does not contain a computer address.

## Automatic Discovery

At startup, the 3DS obtains its IPv4 configuration through `SOCU_GetIPInfo`.
Cyclone DDS then sends SPDP announcements to:

- the standard DDS multicast group `239.255.0.1`
- the calculated IPv4 subnet broadcast address
- the calculated subnet broadcast on participant-index discovery ports

The subnet-broadcast destination uses the DDS metatraffic multicast port:

$$
7400 + 250 \times domain\_id
$$

A multicast-enabled DDS participant normally binds this port. When it receives
the undirected SPDP announcement, it can reply to the unicast locator advertised
by the 3DS. SEDP and user data then travel over unicast.

The automatic peer is also configured without an explicit port. Cyclone expands
it across participant indexes 0 through 9 (`7410`, `7412`, and so on for domain
0). This discovers short-lived processes such as `ros2 service call` even when a
separate ROS daemon already occupies participant index 0 or multicast delivery
is asymmetric.

On Nintendo 3DS, the Cyclone port also learns the IPv4 source of every accepted
LAN participant. The host is retained as a persistent SPDP peer and expanded
across the same participant-index ports. Once a long-lived ROS daemon or node is
discovered, later short-lived CLI and service-client processes on that computer
can be discovered by unicast without embedding the computer address in the
application.

The broadcast bootstrap is an IPv4 local-subnet compatibility mechanism, not a
routed DDS discovery protocol. It does not cross routers or VLANs.

## Static Peer Fallback

If multicast and broadcast are filtered, set `peer_ip` in the SD-card
configuration. The runtime adds that address as a Cyclone DDS peer and switches
to `ParticipantIndex=auto`, enabling predictable unicast discovery ports.

The host must also know the 3DS address. Use
`config/cyclonedds-static-peer.example.xml` as a template. Static peers are a
runtime configuration and do not require rebuilding the `.3dsx`.

## Port Usage

For domain 0, relevant default ports include:

| Purpose | Port |
| --- | --- |
| SPDP multicast/broadcast | `7400` |
| User-data multicast | `7401` |
| Participant 0 discovery unicast | `7410` |
| Participant 0 data unicast | `7411` |

Domain IDs offset these ports by `250 * domain_id`.

The independent diagnostic probe uses UDP port `17650`, outside the RTPS port
range used by the demo.

## Platform Behavior

libctru may reject `IP_MULTICAST_IF`. The 3DS socket backend records the errno
as `MIF` and continues using the only active Wi-Fi route. Group membership is
still requested separately through `IP_ADD_MEMBERSHIP`.

The Cyclone DDS waitset uses a timed `select` loop because the 3DS platform does
not provide the pipe/socketpair mechanism used to wake the upstream waitset.

## Network Limitations

- IPv4 and UDP only
- Same broadcast domain required for automatic discovery
- AP client isolation can block all peer-to-peer traffic
- Firewalls must allow ROS 2 UDP traffic
- WSL2 NAT does not normally expose DDS multicast or broadcast to the LAN
- VPN and virtual adapters can cause the host RMW to choose the wrong interface