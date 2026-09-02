# ROS 2 Integration

The application implements the DDS-facing parts needed for a small ROS 2 node
without linking the full ROS client library stack on the 3DS. It keeps the
runtime modules narrow and reuses shared DDS helpers for QoS setup and endpoint
creation.

## Chatter Topic

| ROS concept | DDS representation |
| --- | --- |
| Topic | `/chatter` |
| DDS topic | `rt/chatter` |
| ROS type | `std_msgs/msg/String` |
| DDS type | `std_msgs::msg::dds_::String_` |

The writer and reader use:

- `KEEP_LAST(10)` history
- `RELIABLE` reliability
- `VOLATILE` durability

The reader ignores samples from its own participant. A `ROS REMOTE RX` entry
therefore proves that data came from another DDS participant.

## Generated Type Support

The generated directory contains the Cyclone DDS descriptors and type metadata
for the message and service payloads used by the app. The stable application
boundary is `include/ros2_types.h`, which exposes a small set of aliases and
ROS topic constants while keeping the generated files themselves out of the
runtime logic.

The target does not run ROS IDL generators at runtime. Descriptor generation is
performed in the host build pipeline and the generated C files are committed to
the project as a generated layer.

## ROS Graph

ROS 2 command-line tools build their graph from the
`ros_discovery_info` topic. The application publishes a
`ParticipantEntitiesInfo` sample containing:

- node name `/nintendo_3ds/ros2_3ds_interface` by default
- participant GID
- `/chatter` writer GID
- `/chatter` reader GID
- `/imu/data_raw` writer GID
- optional `/camera/image_raw/compressed` writer GID
- `/add_two_ints` request reader GID
- `/add_two_ints` reply writer GID

The graph writer uses transient-local durability. The sample is published at
startup and refreshed every five seconds.

## Interoperability

Bidirectional chatter and graph discovery are validated with ROS 2 Jazzy and
`rmw_cyclonedds_cpp`. Other RMW implementations use RTPS and may interoperate,
but type metadata and graph behavior remain implementation-specific validation
targets.

## Current Boundary

The project currently provides one publisher and subscriber for chatter, a
best-effort `sensor_msgs/msg/Imu` publisher, and an
`example_interfaces/srv/AddTwoInts` server. Actions, parameters, lifecycle
nodes, service clients, and the full `rcl` API are outside the implemented
runtime.