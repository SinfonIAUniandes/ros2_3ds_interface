# Joystick Streaming

The application publishes the Nintendo 3DS hardware controls (Circle Pad, New 3DS C-Stick, D-Pad, face buttons, shoulder buttons, and touch screen) using the standard ROS 2 message `sensor_msgs/msg/Joy`.

## ROS Interface

| Property | Value |
| --- | --- |
| ROS topic | `/nintendo_3ds/joy` (default) |
| DDS topic | `rt/nintendo_3ds/joy` |
| Type | `sensor_msgs/msg/Joy` |
| Frame ID | `3ds_joy_link` |
| QoS | Best effort, volatile, keep last 1 |
| Default rate | 20 Hz |

The topic is prefixed with the configured `ros_namespace` (`/nintendo_3ds` by default).

The generated Cyclone DDS descriptor includes XTypes TypeInformation and TypeMapping for `Joy_`, `Header_`, and `Time_`.

---

## Input Mappings

### Axes (`sequence<float> axes`, 8 axes)

Stick and D-pad axes follow standard ROS conventions (left is positive, up is positive):

| Index | Input | Range | Description / Conventions |
| :--- | :--- | :--- | :--- |
| `0` | **Circle Pad X** | `[-1.0, 1.0]` | Pushing Left is `+1.0`, Right is `-1.0` (standard ROS angular yaw) |
| `1` | **Circle Pad Y** | `[-1.0, 1.0]` | Pushing Up is `+1.0`, Down is `-1.0` (standard ROS linear x) |
| `2` | **C-Stick X** | `[-1.0, 1.0]` | Pushing Left is `+1.0`, Right is `-1.0` (New 3DS / CPP, `0.0` if absent) |
| `3` | **C-Stick Y** | `[-1.0, 1.0]` | Pushing Up is `+1.0`, Down is `-1.0` (New 3DS / CPP, `0.0` if absent) |
| `4` | **D-Pad X** | `{-1.0, 0.0, 1.0}` | Left is `+1.0`, Right is `-1.0`, Neutral is `0.0` |
| `5` | **D-Pad Y** | `{-1.0, 0.0, 1.0}` | Up is `+1.0`, Down is `-1.0`, Neutral is `0.0` |
| `6` | **Touch Screen X** | `[-1.0, 1.0]` | Normalized touch X coordinate (`0.0` when screen not touched) |
| `7` | **Touch Screen Y** | `[-1.0, 1.0]` | Normalized touch Y coordinate (`0.0` when screen not touched) |

A hardware deadzone filter (~7.5% of full throw) is applied around stick neutral positions to eliminate drift.

### Buttons (`sequence<int32> buttons`, 15 buttons)

Button values are `1` when pressed / held and `0` when released:

| Index | Button | libctru Source | Description |
| :--- | :--- | :--- | :--- |
| `0` | **A** | `KEY_A` | Right action button |
| `1` | **B** | `KEY_B` | Bottom action button |
| `2` | **X** | `KEY_X` | Top action button |
| `3` | **Y** | `KEY_Y` | Left action button |
| `4` | **L** | `KEY_L` | Left shoulder button |
| `5` | **R** | `KEY_R` | Right shoulder button |
| `6` | **ZL** | `KEY_ZL` | Left secondary shoulder (New 3DS / CPP) |
| `7` | **ZR** | `KEY_ZR` | Right secondary shoulder (New 3DS / CPP) |
| `8` | **Select** | `KEY_SELECT` | Select button |
| `9` | **Start** | `KEY_START` | Start button |
| `10` | **Touch** | `KEY_TOUCH` | Touch screen pressed |
| `11` | **D-Pad Up** | `KEY_DUP` | Directional pad up |
| `12` | **D-Pad Down** | `KEY_DDOWN` | Directional pad down |
| `13` | **D-Pad Left** | `KEY_DLEFT` | Directional pad left |
| `14` | **D-Pad Right** | `KEY_DRIGHT` | Directional pad right |

---

## Hardware Detection & Lifecycle

- **Circle Pad, D-Pad, Buttons, Touch:** Read using libctru HID service (`hidCircleRead`, `hidKeysHeld`, `hidTouchRead`).
- **C-Stick & ZL/ZR:** The application attempts to initialize the `ir:rst` service via `irrstInit()` at startup when running on a New 3DS console (detected via `APT_CheckNew3DS`) or when a Circle Pad Pro accessory is attached to an original 3DS console. If unavailable, C-Stick axes default to `0.0` and standard buttons remain fully operational.

---

## Configuration

In `romfs/config.ini` or `sdmc:/3ds/ros2_3ds_interface/config.ini`:

```ini
joy_enabled=1
joy_publish_hz=20
```

- `joy_enabled`: Set to `1` to enable joystick streaming (default) or `0` to disable the publisher and writer.
- `joy_publish_hz`: Sets the periodic publish rate in Hz (1–100, default: 20).

---

## In-App User Interface

In the **Topics** tab:
1. Navigate to the `/joy` topic row.
2. The indicator shows **ON** when enabled and active, or **OFF** when disabled.
3. Press **A** to toggle the publisher on or off live.
4. The detail pane on the bottom screen displays:
   - Configured publish rate (`joy_publish_hz`)
   - Transmitted message counter (`TX`) and matched ROS 2 subscriber count (`MATCH`)
   - Real-time Circle Pad and C-Stick normalized values (`X`, `Y`)
   - Real-time D-Pad and Touch coordinates
   - Active held buttons badges (`[A]`, `[B]`, `[L]`, etc.)

---

## ROS 2 Integration & Teleoperation Example

### 1. Echoing Joystick Data

Verify the stream on any computer in the same ROS 2 domain:

```bash
ros2 topic list -t
ros2 topic info -v /nintendo_3ds/joy
ros2 topic echo /nintendo_3ds/joy
ros2 topic hz /nintendo_3ds/joy
```

### 2. Robot Teleoperation (`teleop_twist_joy`)

The 3DS can directly drive robots using ROS 2 `teleop_twist_joy`. Create a configuration file `3ds_teleop.yaml`:

```yaml
teleop_twist_joy_node:
  ros__parameters:
    axis_linear:
      x: 1                # Circle Pad Y -> Linear forward/backward
    axis_angular:
      yaw: 0              # Circle Pad X -> Angular yaw turn
    scale_linear:
      x: 0.5              # Maximum speed in m/s
    scale_angular:
      yaw: 1.0            # Maximum turn rate in rad/s
    enable_button: 4      # L button held to enable driving (deadman switch)
    enable_turbo_button: 5 # R button for turbo mode
    scale_linear_turbo:
      x: 1.2
```

Launch the teleoperation node:

```bash
ros2 run teleop_twist_joy teleop_twist_joy_node \
  --ros-args --remap joy:=/nintendo_3ds/joy \
  --params-file 3ds_teleop.yaml
```

