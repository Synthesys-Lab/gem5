#ifndef __LLPM_PHT2_BPRED_ADAPTER_HH__
#define __LLPM_PHT2_BPRED_ADAPTER_HH__

#include <string>

#include "params/LLPMPHT2BPredAdapter.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class LLPMPHT2BPredAdapter : public SimObject
{
  public:
    LLPMPHT2BPredAdapter(const LLPMPHT2BPredAdapterParams &params);

    const std::string component;
    const std::string libraryPath;
    const unsigned resetCycles;
    const std::string jsonConfig;
    const unsigned entries;
    const unsigned initialCounter;
    const std::string resultsDir;
};

} // namespace gem5

#endif // __LLPM_PHT2_BPRED_ADAPTER_HH__
