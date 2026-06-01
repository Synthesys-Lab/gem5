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
Phase 3 target through a typed hook. Full branch-predictor attachment,
Minor-pipeline stepping, timing-mode cache integration, and Verilator pin
binding are added incrementally inside this namespace.
