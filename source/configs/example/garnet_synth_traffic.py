# Copyright (c) 2016 Georgia Institute of Technology
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Author: Tushar Krishna

import m5
from m5.objects import *
from m5.defines import buildEnv
from m5.util import addToPath
import os, argparse, sys

# Locate gem5's configuration modules independently of where this file is
# invoked from. GEM5_ROOT is supplied by the reproducibility scripts.
config_path = os.path.dirname(os.path.abspath(__file__))
default_m5_root = os.path.dirname(os.path.dirname(config_path))
m5_root = os.environ.get("GEM5_ROOT", default_m5_root)
config_root = os.path.join(m5_root, "configs")
addToPath(config_root)

from common import Options
from ruby import Ruby

parser = argparse.ArgumentParser()
Options.addNoISAOptions(parser)

# Lab 4 TwistedTorus topology and routing parameters. Defining them in this
# standard configuration keeps it directly runnable after applying the Lab 4
# source overlay, instead of relying on an experiment-only wrapper script.
parser.add_argument("--torus-x", type=int, default=8)
parser.add_argument("--torus-y", type=int, default=4)
parser.add_argument("--torus-z", type=int, default=4)
parser.add_argument(
    "--twist-mode",
    choices=("none", "single", "double", "synth", "custom"),
    default="single",
)
parser.add_argument("--twist-y-shift", type=int, default=0)
parser.add_argument("--twist-z-shift", type=int, default=0)
parser.add_argument(
    "--twist-routing-policy",
    choices=("deterministic", "random", "adaptive", "flex", "escape"),
    default="adaptive",
)
parser.add_argument("--flex-credit-threshold-pct", type=int, default=25)
parser.add_argument("--flex-credit-margin", type=int, default=2)
parser.add_argument("--flex-diversity-weight", type=int, default=1)
parser.add_argument("--flex-score-epsilon", type=int, default=1)

parser.add_argument(
    "--synthetic",
    default="uniform_random",
    choices=[
        "uniform_random",
        "tornado",
        "bit_complement",
        "bit_reverse",
        "bit_rotation",
        "neighbor",
        "shuffle",
        "transpose",
        "transpose_yz",
        "diagonal_3d",
        "tornado_y",
        "multi_hot",
        "perfect_shuffle",
        "shuffle_3d",
        "tornado_x_3d",
        "transpose_xy_3d",
    ],
)

parser.add_argument(
    "-i",
    "--injectionrate",
    type=float,
    default=0.1,
    metavar="I",
    help="Injection rate in packets per cycle per node. \
                        Takes decimal value between 0 to 1 (eg. 0.225). \
                        Number of digits after 0 depends upon --precision.",
)

parser.add_argument(
    "--precision",
    type=int,
    default=3,
    help="Number of digits of precision after decimal point\
                        for injection rate",
)

parser.add_argument(
    "--sim-cycles", type=int, default=1000, help="Number of simulation cycles"
)

parser.add_argument(
    "--num-packets-max",
    type=int,
    default=-1,
    help="Stop injecting after --num-packets-max.\
                        Set to -1 to disable.",
)

parser.add_argument(
    "--single-sender-id",
    type=int,
    default=-1,
    help="Only inject from this sender.\
                        Set to -1 to disable.",
)

parser.add_argument(
    "--single-dest-id",
    type=int,
    default=-1,
    help="Only send to this destination.\
                        Set to -1 to disable.",
)

parser.add_argument(
    "--inj-vnet",
    type=int,
    default=-1,
    choices=[-1, 0, 1, 2],
    help="Only inject in this vnet (0, 1 or 2).\
                        0 and 1 are 1-flit, 2 is 5-flit.\
                        Set to -1 to inject randomly in all vnets.",
)

#
# Add the ruby specific and protocol specific options
#
Ruby.define_options(parser)

args = parser.parse_args()

cpus = [
    GarnetSyntheticTraffic(
        num_packets_max=args.num_packets_max,
        single_sender=args.single_sender_id,
        single_dest=args.single_dest_id,
        sim_cycles=args.sim_cycles,
        traffic_type=args.synthetic,
        inj_rate=args.injectionrate,
        inj_vnet=args.inj_vnet,
        precision=args.precision,
        num_dest=args.num_dirs,
        traffic_x=args.torus_x,
        traffic_y=args.torus_y,
        traffic_z=args.torus_z,
    )
    for i in range(args.num_cpus)
]

# create the desired simulated system
system = System(cpu=cpus, mem_ranges=[AddrRange(args.mem_size)])


# Create a top-level voltage domain and clock domain
system.voltage_domain = VoltageDomain(voltage=args.sys_voltage)

system.clk_domain = SrcClockDomain(
    clock=args.sys_clock, voltage_domain=system.voltage_domain
)

Ruby.create_system(args, False, system)

# Create a seperate clock domain for Ruby
system.ruby.clk_domain = SrcClockDomain(
    clock=args.ruby_clock, voltage_domain=system.voltage_domain
)

i = 0
for ruby_port in system.ruby._cpu_ports:
    #
    # Tie the cpu test ports to the ruby cpu port
    #
    cpus[i].test = ruby_port.in_ports
    i += 1

# -----------------------
# run simulation
# -----------------------

root = Root(full_system=False, system=system)
root.system.mem_mode = "timing"

# Lab 4 evaluates one tester/Ruby cycle per Tick at 2 GHz.  This setting is
# intentionally confined to the Lab 4 source overlay and does not affect the
# original Lab 1--3 configuration.
m5.ticks.setGlobalFrequency("2GHz")

# instantiate configuration
m5.instantiate()

# simulate until program terminates
exit_event = m5.simulate(args.abs_max_tick)

print("Exiting @ tick", m5.curTick(), "because", exit_event.getCause())
