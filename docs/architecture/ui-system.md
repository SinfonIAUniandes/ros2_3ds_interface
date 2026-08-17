# UI System

The interface is an immediate-mode Citro2D UI inspired by 3DS CodEdit. It uses
a restrained two-level layout: the top screen presents the selected view and
the bottom screen provides navigation, controls, or deeper context.

## Views

| View | Purpose |
| --- | --- |
| Home | Connection health, DDS state, chatter totals, and primary actions |
| Topics | Registered topics, endpoint matches, QoS, counters, and message previews |
| Services | Reserved workspace for future clients and servers |
| Menu | Entry point for Details, Logs, and Settings |
| Details | Network, discovery, RTPS, probe, QoS, and error diagnostics |
| Logs | Recent structured events and the persistent SD log path |
| Settings | Active button bindings and theme/control configuration paths |

The bottom navigation exposes the four primary tabs: Home, Topics, Services,
and Menu. Use the configured previous/next tab buttons or touch a tab. Menu
contains secondary screens and uses configured item navigation and activation.
Runtime actions are intentionally active only from Home, which prevents a
navigation button from triggering a hidden DDS operation on another screen.

## File Layout

```text
include/ui/
  app_ui.h       Public snapshot, actions, views, and lifecycle API
  ui_theme.h     Semantic theme roles

source/ui/
  app_ui.c             Citro2D lifecycle, navigation, and view registry
  ui_draw.c            Shared text, panel, badge, metric, and button primitives
  ui_theme.c           Defaults and INI theme loader
  ui_controls.c        INI controller-binding parser
  ui_topics_registry.c Topic declarations independent of renderers
  ui_internal.h        Private view contracts
  view_overview.c      Home dashboard
  view_topics.c        Topic list and topic detail
  view_navigation.c    Services, Menu, and Settings views
  view_diagnostics.c   Deep runtime diagnostics
  view_logs.c          Structured event log
```

## Data Contract

The UI never calls DDS or sockets. `main.c` owns the runtime and fills a
`ui_snapshot` with display-ready state once per frame. Views only read this
snapshot.

Input follows the opposite direction. `app_ui_handle_input` returns a bitmask of
`ui_action` values, and `main.c` executes the requested DDS or probe operation.
This keeps rendering modules deterministic and makes them easy to test or
replace.

## Adding a View

1. Add a value to `ui_view_id` in `include/ui/app_ui.h`.
2. Create a uniquely named `source/ui/view_*.c` file.
3. Implement top and bottom render functions using the shared primitives.
4. Declare the functions in `source/ui/ui_internal.h`.
5. Add one entry to `ui_views` in `source/ui/app_ui.c`.

Primary tabs are deliberately declared in `app_ui.c`; secondary views are
reachable through Menu. This keeps the bottom navigation stable as features
grow.

## Adding Topics or Services

Runtime ownership should remain outside the UI:

1. Implement the topic or service in its own runtime module.
2. Add a `ui_topic_definition` entry in `ui_topics_registry.c` for each topic.
3. Add only the required counters and message previews to `ui_snapshot`.
4. Expose operations as new `ui_action` bits.
5. Render the module in Topics, Services, or a dedicated registered view.

Avoid calling `dds_write`, `dds_take`, or socket functions from view files.

## Theme Configuration

The built-in theme is stored at:

```text
romfs:/ui/theme.ini
```

Users can override it without rebuilding by creating:

```text
SD:/3ds/ros2_3ds_interface/theme.ini
```

Colors are semantic roles rather than view-specific constants:

```ini
background=F4F6F8
surface=FFFFFF
header=254FAE
toolbar=3367D6
accent=1E88E5
selected=E8F0FE
success=2E7D32
warning=F9A825
danger=C62828
text=202124
muted=68707A
border=D9DEE4
on_color=FFFFFF
```

Values accept `RRGGBB` or `RRGGBBAA`. Unknown entries are ignored, so future
theme roles can be added without invalidating older theme files.

## Controller Configuration

Default bindings are stored in:

```text
romfs:/ui/controls.ini
```

An SD-card override is loaded from:

```text
SD:/3ds/ros2_3ds_interface/controls.ini
```

Supported values are `A`, `B`, `X`, `Y`, `L`, `R`, `UP`, `DOWN`, `LEFT`,
`RIGHT`, `START`, `SELECT`, `ZL`, and `ZR`.

```ini
publish_once=A
toggle_publishing=B
toggle_listener=Y
send_probe=X
next_view=R
previous_view=L
next_item=DOWN
previous_item=UP
activate=A
back=B
exit=START
```

Duplicate bindings are supported when intentional, such as `A` for publishing
on Home and activating Menu items. Keep runtime actions on Home to avoid
ambiguous input in secondary views.