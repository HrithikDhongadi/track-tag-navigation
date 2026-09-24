# TrackTag Navigation — line-following AMR with QR checkpoints

TrackTag Navigation is a complete **Ignition Gazebo Fortress** simulation of a small autonomous mobile robot (AMR) that follows a black floor line, reads QR checkpoint markers, and can travel between named stations on a configured track.

Use it to build a track visually, place QR checkpoints beside the line, generate a runnable Gazebo world, then send the robot to a destination or through a mission queue. The robot follows the line from five downward-facing cameras; QR codes tell the controller which checkpoint it has just passed, allowing it to update its route and choose a configured exit at a junction.

This is intentionally a lightweight, track-based navigation system—not a general-purpose robot autonomy stack. It uses no ROS, SLAM, EKF, sensor fusion, obstacle avoidance, or QR-based localization. A QR is a visual checkpoint event, not a pose estimate.

![TrackTag Navigation running in Ignition Fortress](assets/media/TrackTagSim.png)

▶ [Watch the TrackTag Navigation demo](assets/media/TrackTagDemo.mp4)

## Features

- **Closed-loop line following:** five floor cameras drive a native Ignition Transport controller on `/amr/cmd_vel`. It slows for larger tracking errors and safely stops when camera data is stale or the line is lost.
- **QR checkpoint events:** a dedicated QR camera reads station and track IDs without putting markers over the line. Checkpoints update the active route; they are not used for localization.
- **Directed track routing:** choose a named goal or run an ordered mission. The controller finds the lowest-cost route through the map graph and uses configured left, right, or straight junction maneuvers.
- **Map-to-world workflow:** use the local browser editor to place QR markers, set the robot start pose, and edit graph edges; generate QR textures and a new SDF world from the exported JSON.
- **Native dashboard:** optionally view front and QR cameras, mission state, controller/transport diagnostics, and send explicit mission commands from a GLFW/Dear ImGui console.
- **Modular and performant simulation:** 50 Hz Fortress physics, Ogre2 with shadows disabled, reusable robot models, and headless operation for real-time-factor testing.

The default world uses five 16×16 line cameras at 15 Hz, a 640×480 QR camera at 5 Hz, and a 320×240 forward camera at 5 Hz. The modular robot used by generated worlds raises the forward camera to 1920×1080.

## Requirements

Install Ignition Gazebo Fortress with Ogre2 support, then install dependencies:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
  libignition-transport11-dev libignition-msgs8-dev \
  libopencv-dev libzbar-dev python3-qrcode python3-pil
```
For the optional dashboard, also install `libglfw3-dev` and provide Dear ImGui in `third_party/imgui`, `~/Downloads/imgui`, or through `-DIMGUI_DIR=/path/to/imgui` when configuring.

For NVIDIA GPU rendering, confirm the driver is active:

```bash
nvidia-smi
```

## Build

```bash
git clone <your-repository-url> track-tag-navigation
cd track-tag-navigation
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

`build/track-tag-navigation` is the supported launcher. It starts Fortress and the native controller; with `--ui`, it also starts the optional dashboard executable.

## Run the included world

```bash
./build/track-tag-navigation
```

Press **Play** in Fortress, or start immediately:

```bash
./build/track-tag-navigation --run
```

For an initial QR visibility check:

```bash
./build/track-tag-navigation --run --view
```

Use `Ctrl+C` in the launcher terminal to stop; it sends a zero velocity command. Omit `--view` for normal operation because the QR display window adds overhead.
For the robot heading view, add `--front-view`. It may be combined with `--view`; both windows add display overhead, so omit them for headless or real-time-factor runs.
For a junction-control test, use `--turn left`, `--turn right`, or `--turn straight`. Without `--turn`, the tested line controller remains unchanged.

### Common launch modes

```text
./build/track-tag-navigation --run
./build/track-tag-navigation --headless
./build/track-tag-navigation --run --world sdf/my_map.sdf
./build/track-tag-navigation --run --ui --world sdf/junction_track.sdf --map maps/junction_track.json --goal "Station B"
./build/track-tag-navigation --run --ui   --world sdf/junction_track.sdf   --map maps/junction_track.json   --config configs/junction_track.json   --robot amr_1
```

`--headless` implies `--run`, retains off-screen camera sensors, and removes the Fortress GUI. It is useful for real-time-factor tests and the desktop dashboard can still run with `--ui`.

On hybrid NVIDIA systems, force the discrete GPU if needed:

```bash
__NV_PRIME_RENDER_OFFLOAD=1 \
__GLX_VENDOR_LIBRARY_NAME=nvidia \
./build/track-tag-navigation --run
```

## Baseline behavior

The default world is `sdf/track_with_qr.sdf`.

- Physics: 50 Hz (`max_step_size = 0.02`)
- Five line cameras: 15 FPS, 16×16 RGB
- QR camera: 5 FPS, 640×480 RGB
- Front heading camera: 5 FPS, 320×240 RGB
- Line-controller loop: approximately 30 Hz; terminal logging: 2 Hz

The controller stops when the line is lost or camera data is stale. It does not search for a line or estimate pose. When a map route or mission is active, confirmed QR checkpoints drive route progress and station arrival behavior.

## Create a custom map

Keep QR codes separate from the floor PNG. A 50 mm QR baked into a 3000×3000, 6×6 m floor texture is too low-resolution for reliable decoding. This project generates a high-resolution QR texture for each visual-only SDF marker.

### 1. Add a floor PNG

Create or export a floor texture from Inkscape, then add it to the project:

```bash
cp /path/to/my_track.png assets/track/my_track.png
```

### 2. Open the map editor

```bash
python3 scripts/map_editor_server.py
```

Open `http://127.0.0.1:8000/tools/map_editor/`.

![TrackTag Navigation map editor](assets/media/TrackTagMapEditor.png)

1. Select the floor PNG for preview.
2. Set **Project floor path** to `assets/track/my_track.png`.
3. Add checkpoint IDs and place their QR images beside the line.
4. Drag the blue robot footprint to set its starting position.
5. Set robot yaw in the six-value start-pose field.
6. Download the JSON and save it under `maps/`.

Editor controls:

| Action | Control |
|---|---|
| Zoom / pan | Mouse wheel / drag empty map |
| Multi-select QRs | Ctrl/Cmd + click |
| Rotate selected QR(s) or robot | `R`; `Shift+R` reverses |
| Nudge selected object(s) | Arrow keys; Shift = 5 cm |
| Copy / paste QRs | Ctrl/Cmd + `C`, Ctrl/Cmd + `V` |
| Rename a selected QR | Edit **Checkpoint ID**, press Enter |
| Delete selected QR(s) | Delete |

### Navigation graph

QR checkpoints can also be edited as directed A* graph nodes. In the **Navigation graph** panel, choose a default start, then add each **From → To** edge with a cost and maneuver. Use `follow line` for ordinary segments and `turn left`, `turn right`, or `go straight` for an edge leaving a junction.

The editor draws directional arrows over the map. Select an arrow in the edge list to highlight it; use **Validate navigation graph** before downloading. Renaming or removing a QR updates/removes its connected graph edges. The exported schema is `navigation.schema_version = 1`, with modular directed `edges`; it is ignored by SDF rendering but validated by `generate_map.py` for checkpoint references, costs, duplicate edges, and valid maneuvers.

For a complete example, load `maps/junction_track.json`. Its 40 directed edges model the Track_v2 outer loop, centre spine, and both three-way junctions.


Keep QR markers beside—not across—the line or beneath the line-sensor strip. Use `--view` for the first run of every new map.

### 3. Generate and run the world

```bash
python3 scripts/generate_map.py maps/my_map.json --output sdf/my_map.sdf
./build/track-tag-navigation --run --view --world sdf/my_map.sdf
```

After validation:

```bash
./build/track-tag-navigation --run --world sdf/my_map.sdf
```

To regenerate the same output after editing JSON:

```bash
python3 scripts/generate_map.py maps/my_map.json --output sdf/my_map.sdf --force
```

`--force` replaces only that generated world and overwrites QR textures for its current checkpoints. It does not remove PNGs left by checkpoints removed from a prior version of the same map. The baseline `sdf/track_with_qr.sdf` remains unchanged.

## Modular robot models

Generated worlds refer to a reusable SDF model rather than inlining all robot links, joints, cameras, and plugins:

```xml
<include>
  <uri>model://amr</uri>
  <pose>0 -2 0.005 0 0 0</pose>
</include>
```

The supplied model package is `models/amr/`. To export an `amr` model from another tuned world:

```bash
python3 scripts/export_robot_model.py path/to/robot_world.sdf --output models/my_robot
```

Then set the map JSON or editor robot URI to `model://my_robot`. The supplied modular `amr` model has a 1920×1080 forward camera; this differs from the 320×240 camera in the inline robot of the default baseline world.

## Native topics

| Topic | Message type | Purpose |
|---|---|---|
| `/amr/cmd_vel` | `ignition.msgs.Twist` | Velocity command |
| `/amr/line_0/image` … `/amr/line_4/image` | `ignition.msgs.Image` | Line cameras |
| `/amr/qr/image` | `ignition.msgs.Image` | QR camera |
| `/amr/front/image` | `ignition.msgs.Image` | Forward heading camera |
| `/amr/telemetry` | `ignition.msgs.StringMsg` | Controller and route status |
| `/amr/checkpoint` | `ignition.msgs.StringMsg` | Decoded checkpoint ID |
| `/mission/command` | `ignition.msgs.StringMsg` | Explicit mission command |
| `/mission/status` | `ignition.msgs.StringMsg` | Controller-owned mission state |
| `/amr/odometry` | Ignition odometry | Diff-drive odometry (published by the model plugin; not consumed by this controller) |

## Layout

```text
assets/    Floor textures and generated QR textures
docs/      Component notes
include/   C++ headers
maps/      Editable map JSON files
models/    Reusable SDF model packages
sdf/       Baseline and generated worlds
scripts/   Launch, map, and model utilities
missions/  Reusable ordered mission JSON files
src/       C++ controller modules
tools/     Browser map editor
```

## Troubleshooting

- `WAITING`: press Play and ensure simulator and controller share `IGN_PARTITION` if you set one.
- QR not decoding: use `--view`; confirm the whole QR plus white border is visible.
- Manual stop command:

  ```bash
  ign topic -t /amr/cmd_vel -m ignition.msgs.Twist \
    -p 'linear: {x: 0.0}, angular: {z: 0.0}'
  ```

## Control profiles

The controller has conservative built-in defaults for the supplied baseline. Validate gains and thresholds in your own Fortress environment and for each map. For a map-specific tuning profile, copy [`configs/default.json`](configs/default.json), edit only the fields you want, and pass it to the launcher:

```bash
./build/track-tag-navigation --run --world sdf/junction_track.sdf \
  --config configs/junction_track.json --turn left
```

The profile is loaded first; `--speed`, `--kp`, `--threshold`, and `--timeout` on the command line override it. The process prints the active profile and key settings at startup. `configs/junction_track.json` is an editable starting profile for the smooth-junction map.

The `junction` section controls broad-junction detection, commit speed/bias/duration, and exit reacquisition. Keep a separate profile per map while tuning. Cost-based routing uses these same physical-turn settings when it arms a `junction_turn` edge.


## Cost-based checkpoint routing

A map `navigation` object defines directed QR-to-QR edges. Each edge has `cost_m` and a maneuver: `follow` or `junction_turn` with `left`, `right`, or `straight`. The controller uses Dijkstra's uniform-cost shortest-path search; despite older wording in this project, it does not currently use an A* heuristic. The supplied `maps/junction_track.json` is a complete example.

Run a single goal-directed route:

```bash
__NV_PRIME_RENDER_OFFLOAD=1 \
__GLX_VENDOR_LIBRARY_NAME=nvidia \
./build/track-tag-navigation --headless --ui \
  --world sdf/junction_track.sdf \
  --map maps/junction_track.json \
  --config configs/junction_track.json \
  --goal "Station B"
```

`--start "Station A"` overrides `navigation.default_start`. A confirmed QR updates the route current checkpoint; if the next graph edge is a junction turn, the controller arms that maneuver. QR checkpoints are not localization.

## Dashboard

Add `--ui` to open the TrackTag console. It has four pages: **Mission**, **Cameras**, **Diagnostics**, and **Settings**. It subscribes to native transport data and only publishes an explicit mission command or the one-shot top-bar **Send stop** command. **Send stop** is not a persistent emergency stop: an active controller can publish a later driving command. Use mission pause/stop, stop the controller, or otherwise disable its motion for a persistent stop.

The Mission page is a live workspace: drag the centre divider to resize mission control versus cameras, drag the camera divider to resize the front and QR views, and use either fullscreen button for a focused camera view.

Create a session-only mission by starting with `--map` and omitting both `--goal` and `--mission`. Add graph checkpoint IDs in the Mission page and choose **Create mission**. The primary button changes with controller state: **Start mission**, **Pause mission**, **Resume mission**, **Next task**, or **Retry task**. **Cancel mission** appears only while relevant.

The dashboard requires Dear ImGui. CMake checks `third_party/imgui`, then `~/Downloads/imgui`; otherwise configure explicitly:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIMGUI_DIR=/path/to/imgui
cmake --build build -j2
```

## Reusable missions

A mission JSON is an ordered queue of `navigate` tasks owned and validated by the controller:

```json
{
  "version": 1,
  "auto_advance": false,
  "tasks": [
    { "id": "task_01", "type": "navigate", "to": "Station B" },
    { "id": "task_02", "type": "navigate", "to": "Station C" }
  ]
}
```

Run the supplied example:

```bash
__NV_PRIME_RENDER_OFFLOAD=1 \
__GLX_VENDOR_LIBRARY_NAME=nvidia \
./build/track-tag-navigation --headless --ui \
  --world sdf/junction_track.sdf \
  --map maps/junction_track.json \
  --config configs/junction_track.json \
  --mission missions/junction_demo.json \
  --robot amr_1
```

Do not pass `--goal` with `--mission`; the active mission task supplies the goal. With `auto_advance: false`, reaching a requested checkpoint stops the robot. Select **Next task** in the Mission page to dispatch the next task.

The mission transport contract is `/mission/command` and `/mission/status`, both using `ignition.msgs.StringMsg`.
