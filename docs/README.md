# Documentation

This documentation is organized by task. The root
[README](../README.md) contains the shortest path to installing and running the
demo; the pages here explain how each subsystem works.

## Architecture

Read these pages to understand the implementation:

1. [Architecture overview](architecture/overview.md) explains the runtime,
   source layout, and lifecycle.
2. [Network discovery](architecture/network-discovery.md) explains multicast,
   subnet broadcast, static peers, and RTPS ports.
3. [ROS 2 integration](architecture/ros-integration.md) explains `/chatter`,
   generated type descriptors, QoS, and graph publication.
4. [UI system](architecture/ui-system.md) explains views, snapshots, actions,
   theme configuration, and extension points.

## Guides

Use these pages when operating or changing the project:

- [Runtime configuration](guides/configuration.md): domain ID, static peers,
  host setup, and SD-card overrides
- [Building from source](guides/building.md): toolchain, Cyclone DDS fork, and
  build commands
- [Diagnostics and troubleshooting](guides/troubleshooting.md): screen fields,
  logs, common failures, and validation gates

## Suggested Paths

**First-time user:** root README -> runtime configuration -> troubleshooting.

**Developer:** architecture overview -> network discovery -> ROS 2 integration
-> building from source.

**Network debugging:** network discovery -> troubleshooting.

## Implementation Dependency

The target runtime is built against the Nintendo 3DS Cyclone DDS fork at
[SinfonIAUniandes/cyclone3dds](https://github.com/SinfonIAUniandes/cyclone3dds),
not an unmodified upstream Cyclone DDS checkout.