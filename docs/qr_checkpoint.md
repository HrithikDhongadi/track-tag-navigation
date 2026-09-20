# One QR checkpoint, Fortress, no ROS

This adds Station_A to a COPY of your currently tuned world. It does not
regenerate the robot or change physics, cameras, line controller, or texture.
The QR reader sends no drive commands. It uses OpenCV to prepare/display images
and ZBar for decoding, independent of OpenCV's optional QR decoder backend.

## Install and build
From the repository root:
```bash
sudo apt update
sudo apt install python3-qrcode python3-pil libopencv-dev libzbar-dev pkg-config build-essential cmake libignition-transport11-dev libignition-msgs8-dev
python3 scripts/add_checkpoint.py sdf/track_with_robot.sdf
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Close the old simulation and launch `sdf/track_with_qr.sdf` with the same resource
path and Ogre2 setting used previously. Press Play. Start the QR reader BEFORE
the line follower so it catches the first pass:

```bash
./build/track_tag_navigation_controller --view
```

The root launcher starts the combined controller. Alternatively run
`./build/track_tag_navigation_controller` in another terminal.
At the first marker expect `Location: Station_A`. Duplicate readings are
suppressed while visible; after 3 seconds without decoding it, a new pass can
print again. Last checkpoint remains Station_A between markers, not a continuous
position claim. Ctrl+C stops the reader only, not the robot. Stop the line
follower separately. Omit --view after checking the image to reduce GUI load.

## Placement and visibility

Marker is at x=0.75, y=-1.915, z=0.001 metres, beside the initial straight
at y=-2. Size is 0.05 m INCLUDING the four-module white border. It occupies
60–110 mm left of the line, outside the nominal line strip footprint (about
45 mm each side). QR camera nominal footprint at 0.10 m height is approximately
252 mm laterally × 189 mm longitudinally. Thus it should fit with a centred
robot, but actual view and tracking error must be checked. At 0.10 m/s, it takes
about 1.4 simulated seconds to traverse the full-code visibility window,
roughly 7 frames at 5 FPS. No rate increases are needed for the initial test.

If no reading appears, use --view and check: entire QR including white border
visible; robot on the expected straight; no hardware obscuring the image.
If placement differs, regenerate from the ORIGINAL robot world:
`python3 scripts/add_checkpoint.py sdf/track_with_robot.sdf --x 0.75 --y -1.915 --force`.
Do not place the QR across the line or underneath the line-sensor strip.

## Validation limits

Generator/XML preservation and marker decoding are checked when possible in
the authoring environment. Full C++ build and live Fortress visibility require
the user's installed Fortress/OpenCV/ZBar environment. This first run is the
end-to-end test; it is not a claim of validated live detection.

References:
https://zbar.sourceforge.net/api/classzbar_1_1ImageScanner.html
https://gazebosim.org/api/transport/11/classignition_1_1transport_1_1Node.html
