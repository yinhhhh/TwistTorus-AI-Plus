# Source overlay

This directory contains every gem5 file modified or added by TwistRoute. Its
paths are relative to the root of gem5 v23.0.0.1 at commit
`af72b9ba580546ac12ce05bfaac3fd53fa8699f4`.

Use `../scripts/apply_overlay.sh` to copy the overlay into a gem5 checkout.
The overlay includes the configurable twisted topology, synthetic traffic
patterns, AdaptiveRoute, FlexRoute, the spanning-tree escape VC, and their
statistics and command-line parameters.
