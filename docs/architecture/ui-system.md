# UI System

The interface is an immediate-mode Citro2D UI inspired by 3DS CodEdit. It uses
a restrained two-level layout: the top screen presents the selected view and
the bottom screen provides navigation, controls, or deeper context.

## Views

| View | Purpose |
| --- | --- |
| Home | Connection health, DDS state, chatter totals, and primary actions |
| Topics | Registered topics, endpoint matches, QoS, and sample counters |
| Details | Network, discovery, RTPS, probe, QoS, and error diagnostics |
| Logs | Recent structured events and the persistent SD log path |

Use `L` and `R` or touch the bottom tabs to change views. Runtime actions remain
available through `A`, `B`, `Y`, and `X`; the Home view also exposes touch
buttons for them.

## File Layout

```text
include/ui/
  app_ui.h       Public snapshot, actions, views, and lifecycle API
  ui_theme.h     Semantic theme roles

source/ui/
  app_ui.c             Citro2D lifecycle, navigation, and view registry
  ui_draw.c            Shared text, panel, badge, metric, and button primitives
  ui_theme.c           Defaults and INI theme loader
  ui_internal.h        Private view contracts
  view_overview.c      Home dashboard
  view_topics.c        Topic list and topic detail
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

Navigation and touch tabs are generated from the registry and `UI_VIEW_COUNT`.

## Adding Topics or Services

Runtime ownership should remain outside the UI:

1. Implement the topic or service in its own runtime module.
2. Add only the required summary fields to `ui_snapshot`.
3. Expose operations as new `ui_action` bits.
4. Render the module in the Topics view or add a dedicated registered view.

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