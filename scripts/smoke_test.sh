#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd "$script_dir/.." && pwd)
gem5_root=${GEM5_ROOT:-$project_root/gem5}
gem5_bin=${GEM5_BIN:-$gem5_root/build/NULL/gem5.opt}
config=${GEM5_CONFIG:-$gem5_root/configs/example/garnet_synth_traffic.py}
smoke_out=$(mktemp -d /tmp/twistroute-smoke.XXXXXX)
trap 'rm -rf -- "$smoke_out"' EXIT

[[ -x "$gem5_bin" ]] || {
    echo "gem5 binary not found: $gem5_bin" >&2
    echo "Run scripts/setup.sh first." >&2
    exit 2
}

"$gem5_bin" --outdir="$smoke_out" "$config" \
    --network=garnet --num-cpus=128 --num-dirs=128 \
    --topology=TwistedTorus --torus-x=8 --torus-y=4 --torus-z=4 \
    --twist-mode=custom --twist-y-shift=3 --twist-z-shift=4 \
    --routing-algorithm=2 --twist-routing-policy=flex \
    --synthetic=uniform_random --injectionrate=0.05 --inj-vnet=0 \
    --precision=5 --sim-cycles=200 --sys-clock=2GHz --ruby-clock=2GHz \
    --link-latency=1 --router-latency=1 --link-width-bits=128 \
    --vcs-per-vnet=4

grep -E 'packets_(injected|received)::total|average_packet_latency|flex_lateral_selections|escape_activations' \
    "$smoke_out/stats.txt"
echo "TwistRoute smoke test passed."
