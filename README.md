# ROS 2 3DS Interface

Native ROS 2/DDS experiments for Nintendo 3DS homebrew. The application links
the local Cyclone DDS 3DS port and creates a native DDS participant after
initializing `soc:u`.

## Current probe

The application currently verifies these capabilities:

- `socInit` and local IPv4 discovery
- UDP socket creation and non-blocking receive
- bind to a separate UDP probe port, default `7410`
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

libctru currently exposes `IP_ADD_MEMBERSHIP`, `IP_DROP_MEMBERSHIP`,
`IP_MULTICAST_LOOP`, and `IP_MULTICAST_TTL`, but not `IP_MULTICAST_IF`. The probe
selects the local interface through `ip_mreq.imr_interface` when joining.

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
directly on the 3DS. Press A to transmit an independent UDP probe immediately.

For post-run diagnosis, open this SD card path in a file manager or on a PC:

```text
/3ds/ros2_3ds_interface/logs/YYYYMMDD/
```

The gate passes when the screen reports a successful multicast membership and
the transmit counter increases without socket errors. Receiving multicast and
validating an optional peer remain hardware/network integration checks.

## Scope

This is not yet a ROS 2 topic or service client. The next layer will add the
ROS 2 name mapping and generated message types over the participant already
created by this application. Communication remains direct DDS/RTPS without a
Micro XRCE-DDS Agent or another bridge.
