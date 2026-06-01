# LLPM gem5 Fork

This sub-repository hosts gem5-side glue for LLPM Phase 3
component-interchangeability.

- Keep LLPM-specific SimObjects and C++ adapter glue under `src/llpm/`.
- Keep LLPM-specific Python config helpers under `configs/llpm/`.
- Do not modify broad gem5 core files unless a narrow interface hook is
  explicitly justified in the LLPM plan and review.
- gem5 remains the command-line and Python configuration entry point.
- LLPM/Assassyn logic, RTL generation, Verilator control, trace parsing, and
  report generation remain in the parent LLPM repository.
