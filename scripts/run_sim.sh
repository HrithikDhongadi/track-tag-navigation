#!/usr/bin/env bash
# Compatibility wrapper. The generated TrackTag Navigation launcher is the supported entry.
set -euo pipefail
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$project_root/build/track-tag-navigation" "$@"
