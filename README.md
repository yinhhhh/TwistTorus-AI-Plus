# TwistRoute

We study the joint design of topology and routing for asymmetric 3D torus
networks. In an `8 x 4 x 4` torus, links along the long dimension tend to
carry more traffic, while resources in the two short dimensions can remain
underused. A twisted torus changes the short-dimension wraparound links so that
crossing a boundary also shifts the packet in the long dimension.

We implement configurable twists `(p,q)` in gem5 Garnet and compare
the ordinary `(0,0)` torus, the prior `(4,0)` and `(4,4)` designs, and our
`(3,4)` design. The `(3,4)` twist reduces diameter and average shortest-path
length, but also provides fewer minimal routing choices under some traffic
patterns.

To address this tradeoff, we implement two routing algorithms.
**AdaptiveRoute** selects among all minimal outputs using downstream credits.
**FlexRoute** additionally permits at most one equal-distance lateral hop when
minimal paths are congested, allowing a packet to reach a less congested set of
minimal paths. A dedicated spanning-tree escape VC, with an irreversible
Adaptive-to-Escape transition, provides deadlock freedom.

Please see our report for derivation and experiment details :)

## Contents

| Path | Contents |
|---|---|
| `source/` | Complete source overlay for gem5 v23.0.0.1 |
| `scripts/` | Setup, build, overlay, and smoke-test scripts |
| `reproducibility/` | Experiment drivers, result CSV files, and final figures |
| `report.pdf` | Complete paper with theory, implementation, and evaluation |
| `presentation.pdf` | Eight-minute project presentation |

## Build

Install Git, Python 3, SCons, and the normal gem5 build dependencies. On
Ubuntu, the core packages are:

```bash
sudo apt install build-essential git m4 scons python3-dev \
  libprotobuf-dev protobuf-compiler libgoogle-perftools-dev
```

From the project root, run:

```bash
./scripts/setup.sh
./scripts/smoke_test.sh
```

`setup.sh` checks out gem5 v23.0.0.1 at commit
`af72b9ba580546ac12ce05bfaac3fd53fa8699f4`, applies `source/`, and builds
`build/NULL/gem5.opt` with `Garnet_standalone`. `smoke_test.sh` then runs a
short `(3,4)` FlexRoute simulation. `JOBS` and `GEM5_ROOT` may be used to
override the default build parallelism and gem5 location.

## Reproduce the evaluation

Run all report experiments with:

```bash
JOBS=4 ./reproducibility/run_report_vnet0_eval.sh
JOBS=4 ./reproducibility/run_440_flex_control.sh
```

The first command runs the topology, routing, permutation, and directional
evaluations. The second reproduces the `(4,4)` AdaptiveRoute/FlexRoute control.
Both use the report configuration: an `8 x 4 x 4` network, VNet 0, 10,000
simulation cycles, 2 GHz clocks, four VCs per VNet, and injection rates from
0.025 to 1.000 in steps of 0.025.

For a short reproduction test, select only a few injection rates:

```bash
RATES='0.05 0.10' JOBS=4 ./reproducibility/run_report_vnet0_eval.sh
```