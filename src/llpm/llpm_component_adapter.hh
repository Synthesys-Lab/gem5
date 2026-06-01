#ifndef __LLPM_COMPONENT_ADAPTER_HH__
#define __LLPM_COMPONENT_ADAPTER_HH__

#include <string>

#include "params/LLPMComponentAdapter.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class LLPMComponentAdapter : public SimObject
{
  public:
    LLPMComponentAdapter(const LLPMComponentAdapterParams &params);

    const std::string component;
    const std::string libraryPath;
    const unsigned resetCycles;
    const std::string jsonConfig;
    const std::string resultsDir;
};

} // namespace gem5

#endif // __LLPM_COMPONENT_ADAPTER_HH__
