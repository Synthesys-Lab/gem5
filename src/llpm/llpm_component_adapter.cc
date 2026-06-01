#include "llpm/llpm_component_adapter.hh"

namespace gem5
{

LLPMComponentAdapter::LLPMComponentAdapter(
    const LLPMComponentAdapterParams &params)
    : SimObject(params),
      component(params.component),
      libraryPath(params.library_path),
      resetCycles(params.reset_cycles),
      jsonConfig(params.json_config),
      resultsDir(params.results_dir)
{
}

} // namespace gem5
