# llpm_minor_pipeline_adapter.hh / llpm_minor_pipeline_adapter.cc

These files implement the gem5-side Phase 3 Minor-pipeline adapter.

## Interface

`LLPMMinorPipelineAdapter` is a gem5 `SimObject` with two request ports:
`ifetch_side` and `data_side`. In `se-atomic` interchangeable rows the LLPM
config attaches both ports to gem5's memory bus and keeps gem5 as the workload
launcher, memory owner, device owner, and statistics host.

The adapter loads the `rtl-minor-pipeline` Verilated C ABI library with
`dlopen`, checks the shared ABI version and component id, creates/resets the
component, and schedules a small recurring sidecar event during gem5
simulation. Each event submits pipeline reset/step commands, drains ready C ABI
responses, and converts emitted instruction-fetch/data memory request events
into gem5 atomic memory packets. The returned data is submitted back to the RTL
component with `LLPM_OP_MEMORY_RESPONSE`.

## Internal Helpers

The implementation uses the same C ABI function-pointer pattern as
`LLPMCachePortAdapter` and `LLPMPHT2Conditional`. `processSidecarEvent()` owns
the bounded recurring smoke execution. `submitAndStep()` sends one C ABI
request and advances the Verilated model. `drainReadyResponses()` handles
pipeline responses and memory-request events. `sendAtomicMemoryRequest()` and
`submitMemoryResponse()` bridge one RTL memory request through gem5's atomic
memory system. `refreshComponentStats()` imports the component's exported C ABI
stats into gem5 statistics.

## Data Structures

`LLPMMinorPipelineAdapterStats` exports `system.llpm_minor_pipeline.*` stats
for adapter crossings, Verilated cycles, component requests/responses, resets,
pipeline steps, instruction-fetch requests, data-load requests, data-store
requests, memory responses, halts, and faults.

This is a live sidecar proof of gem5-hosted Minor C ABI stepping and
instruction/data request surfacing. It is not yet a full replacement for
gem5's CPU execution loop, syscall handling, retirement accounting, or
architectural end-state ownership.
