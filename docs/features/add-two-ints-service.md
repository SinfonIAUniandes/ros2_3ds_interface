# AddTwoInts Service

The application provides a ROS 2 server for the standard
`example_interfaces/srv/AddTwoInts` service.

## Interface

| Property | Value |
| --- | --- |
| ROS service | `/add_two_ints` |
| Type | `example_interfaces/srv/AddTwoInts` |
| Request | `int64 a`, `int64 b` |
| Response | `int64 sum` |
| QoS | Reliable, volatile, keep last 10 |

The Services tab shows server state, matched request/reply endpoints, total
requests handled, and the latest calculation.

## Call It

Use native Ubuntu with ROS 2 Jazzy and Cyclone DDS as the reference setup:

```sh
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
unset CYCLONEDDS_URI
ros2 pkg prefix rmw_cyclonedds_cpp
ros2 daemon stop
ros2 doctor --report | grep -Ei 'middleware|rmw'
ros2 service list -t
ros2 service type /add_two_ints
ros2 service call /add_two_ints example_interfaces/srv/AddTwoInts "{a: 2, b: 3}"
```

If `ros2 service list` sees the server but a client reports it as unavailable,
the graph participant is visible while one or both service endpoints are not
matched. Check the 3DS session log for both `request match count=1` and
`response match count=1`. A response-only match indicates incomplete SEDP
discovery, not a service implementation or shell syntax failure.

If multicast is filtered, configure a static peer in both directions using
`config/cyclonedds-static-peer.example.xml` on Ubuntu and `peer_ip=<Ubuntu LAN
IPv4>` in the 3DS SD-card configuration. Avoid WSL2 NAT for this test.

Expected response:

```text
sum: 5
```

## Required RMW

The service wire mapping currently supports `rmw_cyclonedds_cpp`. Topic and
graph discovery alone do not prove that the host process uses Cyclone DDS.

In a 3DS session log, a rejected request from `vendor 1.15` identifies eProsima
Fast DDS; Eclipse Cyclone DDS uses `vendor 1.16`. Fast DDS sends the
`AddTwoInts` body as a 20-byte payload and correlates requests through RTPS
sample identity. The current server expects Cyclone's private 16-byte request
header in the serialized payload, so Fast DDS calls cannot receive a response.

On PowerShell, set and verify the implementation before stopping the daemon and
starting the client:

```powershell
$env:RMW_IMPLEMENTATION = "rmw_cyclonedds_cpp"
$env:ROS_DOMAIN_ID = "0"
Remove-Item Env:CYCLONEDDS_URI -ErrorAction SilentlyContinue
ros2 pkg prefix rmw_cyclonedds_cpp
ros2 daemon stop
ros2 doctor --report | Select-String -Pattern "middleware|rmw"
ros2 service call /add_two_ints example_interfaces/srv/AddTwoInts "{a: 2, b: 3}"
```

If `ros2 pkg prefix rmw_cyclonedds_cpp` fails, that environment does not contain
the required RMW. The reference fallback is native Ubuntu with ROS 2 Jazzy and
the `ros-jazzy-rmw-cyclonedds-cpp` package installed.

## Diagnosing a Call

The Services tab exposes the request/reply data plane:

| Field | Interpretation |
| --- | --- |
| `Request match` / `Reply match` | DDS endpoint matching with the client |
| `Taken` | Valid requests deserialized by the 3DS server |
| `Replies` | Responses accepted by the 3DS DDS writer |
| `Take rc` / `Write rc` | Most recent Cyclone DDS return codes |

If the command prints `making request` but `Taken` stays at zero, the request is
not reaching or deserializing at the 3DS endpoint. If `Taken` increases but
`Replies` stays at zero, inspect `Write rc`. If both increase but the CLI still
waits, save the session log: it includes the correlation `guid` and `seq` used
for the reply.

The server saturates on signed 64-bit overflow: values above the positive range
return `INT64_MAX`, and values below the negative range return `INT64_MIN`.

## DDS Mapping

For compatibility with ROS 2 Jazzy `rmw_cyclonedds_cpp`, the server uses:

| Direction | DDS topic | DDS type |
| --- | --- | --- |
| Request | `rq/add_two_intsRequest` | `example_interfaces::srv::dds_::AddTwoInts_Request_` |
| Reply | `rr/add_two_intsReply` | `example_interfaces::srv::dds_::AddTwoInts_Response_` |

`rmw_cyclonedds_cpp` serializes a private 16-byte request header before the
service body:

```text
uint64 guid
int64  seq
```

The 3DS server preserves both values exactly in its reply, allowing the client
to correlate the response with the original request. The generated descriptors
intentionally omit XTypes metadata because the RMW injects this header outside
the ROS service body and matches these service types by name.

## ROS Graph

The service request reader and reply writer are included in
`ros_discovery_info`, so the service should be visible to `ros2 service list`
and `ros2 node info /ros2_3ds_interface`.

## Limits

This is a server-only implementation. Service clients and additional service
types can be added as separate runtime modules following the same pattern.