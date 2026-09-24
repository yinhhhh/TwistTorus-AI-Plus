#!/usr/bin/env bash
# Reproduce every Lab 4 report experiment with injection restricted to VNet 0.
#
# Prerequisite: overlay ../source onto a gem5 tree and rebuild gem5.opt.
# Usage: GEM5_ROOT=/path/to/patched/gem5 JOBS=4 ./run_report_vnet0_eval.sh
# Optional: RESULTS_DIR=/where/to/write/results RATES='0.025 0.050 ... 1.000'
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd "$script_dir/.." && pwd)
default_gem5_root=$project_root/gem5
gem5_root=${GEM5_ROOT:-$default_gem5_root}
gem5_bin=${GEM5_BIN:-$gem5_root/build/NULL/gem5.opt}
config=${GEM5_CONFIG:-$gem5_root/configs/example/garnet_synth_traffic.py}
result_root=${RESULTS_DIR:-$script_dir/results/rerun_report_vnet0}
cycles=${CYCLES:-10000}
jobs=${JOBS:-4}

[[ -x "$gem5_bin" ]] || { echo "gem5 binary not found: $gem5_bin" >&2; exit 2; }
[[ -f "$config" ]] || { echo "configuration not found: $config" >&2; exit 2; }
grep -q -- '--twist-routing-policy' "$config" || {
    echo "Lab 4 source has not been overlaid onto GEM5_ROOT; see ../source/README.md" >&2
    exit 2
}
export GEM5_ROOT="$gem5_root"
# The course workspace builds gem5 against this virtual environment.  On a
# normal gem5 installation this directory is absent and the user's environment
# is left unchanged; GEM5_RUNTIME_LIB may explicitly name another runtime.
runtime_lib=${GEM5_RUNTIME_LIB:-$project_root/.lab0-env/lib}
if [[ -d "$runtime_lib" ]]; then
    export LD_LIBRARY_PATH="$runtime_lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

# The report uses 0.025, 0.050, ..., 1.000 packet/node/cycle.
rates() {
    if [[ -n ${RATES:-} ]]; then printf '%s\n' $RATES
    else seq 1 40 | awk '{printf "%.3f\n", $1/40}'
    fi
}

make_cases() {
    local rate design policy p q traffic
    # Section IV-A: topology comparison under uniform random traffic.
    for design in '000 adaptive 0 0' '400 adaptive 4 0' \
                  '440 adaptive 4 4' '340 adaptive 3 4'; do
        read -r policy p q <<<"${design#* }"
        for rate in $(rates); do
            printf 'uniform_topology|%s|%s|%s|%s|uniform_random|%s\n' \
                "${design%% *}" "$policy" "$p" "$q" "$rate"
        done
    done
    # Section IV-B: routing ablation on the fixed (3,4) topology.
    for policy in deterministic random adaptive flex; do
        for rate in $(rates); do
            printf 'routing_ablation|340_%s|%s|3|4|uniform_random|%s\n' \
                "$policy" "$policy" "$rate"
        done
    done
    # Section IV-C.1: the three permutation patterns from the PTT/PDTT paper.
    for design in '000 adaptive 0 0' '400 adaptive 4 0' \
                  '440 adaptive 4 4' '340 adaptive 3 4' '340 flex 3 4'; do
        read -r policy p q <<<"${design#* }"
        for traffic in bit_complement bit_reverse perfect_shuffle; do
            for rate in $(rates); do
                printf 'permutation|%s_%s|%s|%s|%s|%s|%s\n' \
                    "${design%% *}" "$policy" "$policy" "$p" "$q" "$traffic" "$rate"
            done
        done
    done
    # Section IV-C.2: directional traffic patterns.
    for design in '000 adaptive 0 0' '400 adaptive 4 0' \
                  '440 adaptive 4 4' '340 adaptive 3 4' '340 flex 3 4'; do
        read -r policy p q <<<"${design#* }"
        for traffic in diagonal_3d tornado_x_3d tornado_y; do
            for rate in $(rates); do
                printf 'directional|%s_%s|%s|%s|%s|%s|%s\n' \
                    "${design%% *}" "$policy" "$policy" "$p" "$q" "$traffic" "$rate"
            done
        done
    done
}

run_case() {
    local spec=$1 study design policy p q traffic rate case_dir row out
    IFS='|' read -r study design policy p q traffic rate <<<"$spec"
    case_dir="$result_root/$study/cases"
    row="$case_dir/${design}_${traffic}_${rate}.csv"
    [[ -s "$row" ]] && return 0
    mkdir -p "$case_dir"
    out=$(mktemp -d "/tmp/lab4-report-${study}-${design}-${traffic}-${rate}.XXXXXX")
    if "$gem5_bin" --outdir="$out" --redirect-stdout --redirect-stderr "$config" \
        --network=garnet --num-cpus=128 --num-dirs=128 \
        --topology=TwistedTorus --torus-x=8 --torus-y=4 --torus-z=4 \
        --twist-mode=custom --twist-y-shift="$p" --twist-z-shift="$q" \
        --routing-algorithm=2 --twist-routing-policy="$policy" \
        --flex-credit-threshold-pct=100 --flex-credit-margin=2 \
        --flex-diversity-weight=1 --flex-score-epsilon=1 \
        --synthetic="$traffic" --injectionrate="$rate" --precision=5 --inj-vnet=0 \
        --sim-cycles="$cycles" --sys-clock=2GHz --ruby-clock=2GHz \
        --link-latency=1 --router-latency=1 --link-width-bits=128 --vcs-per-vnet=4; then
        awk -v study="$study" -v design="$design" -v policy="$policy" \
            -v p="$p" -v q="$q" -v traffic="$traffic" -v rate="$rate" \
            -v cycles="$cycles" '
            $1 == "system.ruby.network.packets_injected::total" {pi=$2}
            $1 == "system.ruby.network.packets_received::total" {pr=$2}
            $1 == "system.ruby.network.average_packet_queueing_latency" {ql=$2}
            $1 == "system.ruby.network.average_packet_network_latency" {nl=$2}
            $1 == "system.ruby.network.average_packet_latency" {lat=$2}
            $1 == "system.ruby.network.average_hops" {hops=$2}
            $1 == "system.ruby.network.flex_lateral_selections" {lateral=$2}
            $1 == "system.ruby.network.escape_activations" {escape=$2}
            END {printf "%s,%s,%s,%s,%s,%s,%s,%s,%.10f,%s,%s,%s,%s,%.10f,%.10f,%.10f\n",
                study,design,policy,p,q,traffic,rate,pi,pr/128/cycles,ql,nl,lat,hops,
                (pi ? lateral/pi : 0),(pr ? lateral/pr : 0),(pr ? escape/pr : 0)}' \
            "$out/stats.txt" >"$row"
        rm -f "${row%.csv}.error.log"
    else
        cp "$out/simerr" "${row%.csv}.error.log" 2>/dev/null || true
    fi
    rm -rf -- "$out"
}

export -f run_case
export gem5_bin config result_root cycles GEM5_ROOT
make_cases | xargs -r -P "$jobs" -I '{}' bash -c 'run_case "$1"' _ '{}'

header='study,design,policy,p,q,traffic,injection_rate,packets_injected,reception_rate,queueing_latency,network_latency,total_latency,average_hops,lateral_per_injected,lateral_per_received,escape_per_received'
for study in uniform_topology routing_ablation permutation directional; do
    case_dir="$result_root/$study/cases"
    mkdir -p "$case_dir"
    { echo "$header"; find "$case_dir" -maxdepth 1 -name '*.csv' -type f -print0 | sort -z | xargs -0 -r cat; } >"$result_root/$study/sweep.csv"
done

actual=$(find "$result_root" -path '*/cases/*.csv' -type f | wc -l)
expected=$(make_cases | wc -l)
failed=$(find "$result_root" -path '*/cases/*.error.log' -type f | wc -l)
if [[ "$failed" -ne 0 ]]; then
    echo "Failed $failed case(s); see *.error.log below $result_root." >&2
    exit 1
fi
echo "Completed $actual/$expected cases. Raw CSV files are under $result_root."
[[ "$actual" -eq "$expected" ]]
