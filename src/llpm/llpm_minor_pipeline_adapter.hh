#ifndef __LLPM_MINOR_PIPELINE_ADAPTER_HH__
#define __LLPM_MINOR_PIPELINE_ADAPTER_HH__

#include <string>

#include "mem/port.hh"
#include "params/LLPMMinorPipelineAdapter.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class LLPMMinorPipelineAdapter : public SimObject
{
  private:
    class MemSidePort : public RequestPort
    {
      private:
        LLPMMinorPipelineAdapter &owner;
        const std::string role;

      public:
        MemSidePort(const std::string &name,
                    LLPMMinorPipelineAdapter &owner,
                    const std::string &role);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;
        void recvRangeChange() override;
    };

    MemSidePort ifetchSidePort;
    MemSidePort dataSidePort;

  public:
    LLPMMinorPipelineAdapter(const LLPMMinorPipelineAdapterParams &params);

    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    bool recvMemTimingResp(const std::string &role, PacketPtr pkt);
    void recvMemReqRetry(const std::string &role);
    void recvMemRangeChange(const std::string &role);

    const std::string component;
    const std::string libraryPath;
    const unsigned resetCycles;
    const std::string jsonConfig;
    const std::string resultsDir;
};

} // namespace gem5

#endif // __LLPM_MINOR_PIPELINE_ADAPTER_HH__
