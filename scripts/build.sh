#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd "$script_dir/.." && pwd)
gem5_root=${GEM5_ROOT:-$project_root/gem5}
jobs=${JOBS:-4}

[[ -f "$gem5_root/SConstruct" ]] || {
    echo "gem5 source tree not found: $gem5_root" >&2
    exit 2
}

cd "$gem5_root"
scons build/NULL/gem5.opt PROTOCOL=Garnet_standalone -j"$jobs"
