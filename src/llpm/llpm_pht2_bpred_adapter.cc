#include "llpm/llpm_pht2_bpred_adapter.hh"

#include "base/logging.hh"

namespace gem5
{

LLPMPHT2BPredAdapter::LLPMPHT2BPredAdapter(
    const LLPMPHT2BPredAdapterParams &params)
    : SimObject(params),
      component(params.component),
      libraryPath(params.library_path),
      resetCycles(params.reset_cycles),
      jsonConfig(params.json_config),
      entries(params.entries),
      initialCounter(params.initial_counter),
      resultsDir(params.results_dir)
{
    if (component != "rtl-pht2-bpred") {
        fatal("LLPMPHT2BPredAdapter only supports rtl-pht2-bpred");
    }
    if (initialCounter > 3) {
        fatal("LLPMPHT2BPredAdapter initial_counter must be a 2-bit value");
    }
}

} // namespace gem5
