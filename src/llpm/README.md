# LLPM gem5 Namespace

`src/llpm/` contains gem5-side SimObject and C++ adapter glue for LLPM Phase 3
component interchangeability. The parent LLPM repository owns adapter policy,
RTL generation, Verilator control, trace normalization, and reporting.

The first scaffold provides a minimal `LLPMComponentAdapter` SimObject for
non-cache components and an `LLPMCachePortAdapter` SimObject with one CPU-side
response port and one memory-side request port. The cache-port adapter is the
initial `rtl-dcache`/`rtl-split-cache` hook; it loads ABI-compatible LLPM
component libraries through `dlopen` for atomic blocking smoke tests.
The scaffold also declares component-specific `LLPMPHT2BPredAdapter` and
`LLPMMinorPipelineAdapter` SimObjects so gem5 configs can select every fixed
Phase 3 target through a typed hook. The live PHT2 conditional hook attaches
under gem5's `BranchPredictor` wrapper. The Minor-pipeline hook is currently a
live sidecar proof of C ABI stepping and instruction/data request surfacing:
gem5 still owns CPU retirement and architectural end state while the
Verilated Minor component is stepped inside gem5 and exports
`system.llpm_minor_pipeline.*` stats. Timing-mode cache integration and full
Minor CPU replacement are later extensions inside this namespace.
