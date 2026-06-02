# LLPMPHT2Conditional.py

`LLPMPHT2Conditional.py` declares the gem5 SimObject used for the live Phase 3
PHT2 boundary. Unlike `LLPMPHT2BPredAdapter`, this class inherits gem5's
`ConditionalPredictor`, so it can be installed as
`BranchPredictor(conditionalBranchPred=...)` while gem5 keeps BTB, RAS,
frontend redirect, workload launch, memory, and statistics ownership.

The exposed parameters are the LLPM component name, Verilated shared-library
path, reset-cycle count, JSON config string, PHT entry count, initial 2-bit
counter value, and result directory. The C++ implementation validates and uses
these parameters when loading the shared C ABI component.
