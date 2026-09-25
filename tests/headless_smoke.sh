#!/usr/bin/env bash
# Launch a generated-world route headlessly and verify that the simulator and
# controller expose their expected transport topics. This is intentionally an
# opt-in CTest because it needs a working Fortress/Ogre runtime.
set -euo pipefail

launcher=$1
project_root=$2
log_file=$(mktemp)
export IGN_PARTITION="track_tag_smoke_${RANDOM}_$$"

cleanup() {
  # The launcher starts both the controller and `ign gazebo`; isolate that
  # process tree so a successful smoke test can always stop it promptly.
  kill -- "-${launcher_pid:-}" 2>/dev/null || true
  wait "${launcher_pid:-}" 2>/dev/null || true
  rm -f "$log_file"
}
trap cleanup EXIT INT TERM

setsid "$launcher" --headless --world "$project_root/sdf/junction_track.sdf" \
  --map "$project_root/maps/junction_track.json" --goal "Station B" >"$log_file" 2>&1 &
launcher_pid=$!

for _ in $(seq 1 30); do
  topics=$(ign topic -l 2>/dev/null || true)
  if grep -Fxq "/amr/telemetry" <<<"$topics" && grep -Fxq "/amr/qr/image" <<<"$topics"; then
    echo "Headless smoke test observed /amr/telemetry and /amr/qr/image."
    exit 0
  fi
  sleep 1
done

cat "$log_file" >&2
echo "Timed out waiting for expected headless transport topics." >&2
exit 1
