# AMR line follower — Ignition Fortress, no ROS

The current controller combines the line follower and QR reader in one C++17
process, with one thread for each. It uses five 16×16 RGB floor cameras for
line following and a separate camera for QR checkpoints. Line following itself
does not use OpenCV, robot ground truth, odometry, pose estimation, SLAM, or a
preprogrammed trajectory.

This page describes direct, standalone controller use. For the supported
end-to-end launcher, map generation, QR checkpoints, junction turns, routing,
missions, and the dashboard, see the root `README.md`.

## Build (Ubuntu with Fortress already installed)

```bash
sudo apt update
sudo apt install build-essential cmake libignition-transport11-dev libignition-msgs8-dev libopencv-dev libzbar-dev pkg-config
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Run these commands from the repository root. For normal operation use
`./build/track-tag-navigation`; it launches Fortress and this combined
controller. If running the controller binary directly, launch a compatible
Fortress world separately, press Play, and do not run another velocity
publisher at the same time.

## Inspect, then drive

```bash
./build/track_tag_navigation_controller --monitor
```

Monitor is read-only: it sends no velocity, including on exit. Stop any previous
motion first. The five bits are left to right. Near the initial straight,
expect 00100 or neighboring detections depending on exact placement.
Mean intensity near zero is black and near 255 is white. Dark is the fraction
of pixels below the threshold. The displayed bits use 50% dark; steering uses
continuous dark fractions, so partial line coverage still contributes.

Ctrl+C to exit monitor, then:

```bash
./build/track_tag_navigation_controller
```

Default speed is 0.10 m/s. Steering uses a weighted centroid with weights
2,1,0,-1,-2. A line to the left produces positive angular Z. It slows for
larger errors, caps angular speed at 0.8 rad/s, and sends zero velocity if
line coverage is insufficient or any camera has invalid/stale data.
It resumes automatically when valid line observations return.
Ctrl+C sends several stop messages. A forced kill/crash cannot send a stop;
Fortress DiffDrive may retain its last command, so use the manual stop below.

```bash
ign topic -t /amr/cmd_vel -m ignition.msgs.Twist -p 'linear: {x: 0.0}, angular: {z: 0.0}'
```

## Troubleshooting

- WAITING: press Play; confirm all five image topics; terminal and simulation
  must use the same IGN_PARTITION if set. Invalid image format/size also gives WAITING.
- NO LINE with light readings: robot may be off the line. Restart the world
  or reposition so the sensor strip is over a straight and the robot faces along it.
- All readings dark: inspect camera views for occlusion before changing threshold.
- Oscillation: try `--speed 0.06 --kp 0.30`.
- Drifts outward in curves: inspect readings first; try `--speed 0.06 --kp 0.60`.
- Threshold may need adjustment: `--threshold 130`. Set between observed
  line and floor brightness, not blindly.
- Very slow simulation: `--timeout 5` permits a larger wall-time frame gap.
- The base line follower stops on lost line and does not search for a line.
  When started with `--turn` or a navigation map, the current controller also
  has a separate broad-junction commit and exit-reacquisition state machine.

## Validation

Controller math and the project build should be checked locally; live
loop-following and junction gains still need validation in the actual Fortress
simulation and for each physical map/profile.

References:
- https://gazebosim.org/docs/fortress/moving_robot/
- https://gazebosim.org/api/transport/11/classignition_1_1transport_1_1Node.html
