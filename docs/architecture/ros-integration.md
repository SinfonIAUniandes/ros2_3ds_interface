# ROS 2 Integration

The application implements the DDS-facing parts needed for a small ROS 2 node
without linking the full ROS client library stack on the 3DS.

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

`generated/ros_types/std_msgs_string.c` contains the Cyclone DDS descriptor,
XTypes TypeInformation, and TypeMapping for `std_msgs/msg/String`. These values
allow DDS implementations to compare the wire type during endpoint matching.

The target does not run ROS IDL generators. Descriptor generation is a host
build-time operation and the generated C files are committed to the project.

## ROS Graph

ROS 2 command-line tools build their graph from the
`ros_discovery_info` topic. The application publishes a
`ParticipantEntitiesInfo` sample containing:

- node name `/ros2_3ds_interface`
- participant GID
- `/chatter` writer GID
- `/chatter` reader GID

The graph writer uses transient-local durability. The sample is published at
startup and refreshed every five seconds.

## Interoperability

Bidirectional chatter and graph discovery are validated with ROS 2 Jazzy and
`rmw_cyclonedds_cpp`. Other RMW implementations use RTPS and may interoperate,
but type metadata and graph behavior remain implementation-specific validation
targets.

## Current Boundary

The project currently provides one publisher, one subscriber, and graph
metadata. Services, actions, parameters, lifecycle nodes, and the full `rcl`
API are outside the implemented runtime.