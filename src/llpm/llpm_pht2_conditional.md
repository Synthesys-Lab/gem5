# llpm_pht2_conditional.hh / llpm_pht2_conditional.cc

These files implement the live gem5 conditional-predictor hook for the Phase 3
`rtl-pht2-bpred` boundary. The class inherits gem5's
`branch_prediction::ConditionalPredictor`, so it can replace only the
direction predictor while gem5 keeps the branch predictor unit, BTB, RAS,
target selection, frontend redirects, workload launch, memory objects, and
statistics host.

The implementation loads the selected Verilated PHT2 shared library through
the LLPM C ABI, validates ABI version and component id, resets the component,
and sends blocking `LLPM_OP_BPRED_QUERY` / `LLPM_OP_BPRED_UPDATE` requests.
Lookup creates a small history token used by gem5's later update/squash path.
Update trains the Verilated component at commit time, while squash only
discards the pending token because this PHT2 boundary has no speculative
history to restore.

The exported stats are adapter crossings, component-reported Verilated cycles,
component requests, predictions, updates, predicted-taken count, direction
correct count, and direction mispredictions.
