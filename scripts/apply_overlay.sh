#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd "$script_dir/.." && pwd)
gem5_root=${GEM5_ROOT:-$project_root/gem5}

[[ -d "$gem5_root/configs" && -d "$gem5_root/src" ]] || {
    echo "gem5 source tree not found: $gem5_root" >&2
    echo "Run scripts/setup.sh first or set GEM5_ROOT." >&2
    exit 2
}

cp -a "$project_root/source/configs/." "$gem5_root/configs/"
cp -a "$project_root/source/src/." "$gem5_root/src/"
echo "Applied the TwistRoute source overlay to $gem5_root"
