#ifndef __LLPM_MINOR_PIPELINE_ADAPTER_HH__
#define __LLPM_MINOR_PIPELINE_ADAPTER_HH__

#include <cstdint>
#include <string>

#include "base/statistics.hh"
#include "llpm/llpm_gem5_verilator_abi.h"
#include "mem/port.hh"
#include "params/LLPMMinorPipelineAdapter.hh"
#include "sim/eventq.hh"
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

    struct LLPMMinorPipelineAdapterStats : public statistics::Group
    {
        LLPMMinorPipelineAdapterStats(statistics::Group *parent);

        uint64_t adapterCrossings = 0;
        uint64_t verilatedCycles = 0;
        uint64_t componentRequests = 0;
        uint64_t componentResponses = 0;
        uint64_t resetsCount = 0;
        uint64_t pipelineSteps = 0;
        uint64_t ifetchRequests = 0;
        uint64_t dataLoadRequests = 0;
        uint64_t dataStoreRequests = 0;
        uint64_t memoryResponses = 0;
        uint64_t haltCount = 0;
        uint64_t faultCount = 0;

        statistics::Value adapter_crossings;
        statistics::Value verilated_cycles;
        statistics::Value component_requests;
        statistics::Value component_responses;
        statistics::Value resets;
        statistics::Value pipeline_steps;
        statistics::Value ifetch_requests;
        statistics::Value data_load_requests;
        statistics::Value data_store_requests;
        statistics::Value memory_responses;
        statistics::Value halts;
        statistics::Value faults;
    };

    MemSidePort ifetchSidePort;
    MemSidePort dataSidePort;
    LLPMMinorPipelineAdapterStats stats;
    EventFunctionWrapper sidecarEvent;
    void *libraryHandle = nullptr;
    void *componentHandle = nullptr;
    uint64_t nextRequestId = 1;
    uint64_t stepsSinceReset = 0;
    uint64_t sidecarBursts = 0;
    bool componentHalted = false;

    using AbiVersionFn = uint32_t (*)();
    using ComponentIdFn = uint32_t (*)();
    using ComponentNameFn = const char *(*)();
    using CreateFn = void *(*)(const LlpmComponentConfig *);
    using DestroyFn = void (*)(void *);
    using ResetFn = int32_t (*)(void *, uint32_t);
    using SubmitFn = int32_t (*)(void *, const LlpmComponentRequest *);
    using StepFn = int32_t (*)(void *, uint32_t);
    using PopResponseFn = int32_t (*)(void *, LlpmComponentResponse *);
    using StatCountFn = uint32_t (*)(void *);
    using StatFn = int32_t (*)(void *, uint32_t, LlpmComponentStat *);

    AbiVersionFn abiVersionFn = nullptr;
    ComponentIdFn componentIdFn = nullptr;
    ComponentNameFn componentNameFn = nullptr;
    CreateFn createFn = nullptr;
    DestroyFn destroyFn = nullptr;
    ResetFn resetFn = nullptr;
    SubmitFn submitFn = nullptr;
    StepFn stepFn = nullptr;
    PopResponseFn popResponseFn = nullptr;
    StatCountFn statCountFn = nullptr;
    StatFn statFn = nullptr;

    void loadComponentLibrary();
    void destroyComponentLibrary();
    void processSidecarEvent();
    void submitAndStep(LlpmComponentRequest request);
    void drainReadyResponses();
    void serviceMemoryRequest(const LlpmComponentResponse &response);
    uint64_t sendAtomicMemoryRequest(const LlpmComponentResponse &response);
    void submitMemoryResponse(const LlpmComponentResponse &response,
                              uint64_t data);
    void refreshComponentStats();

  public:
    LLPMMinorPipelineAdapter(const LLPMMinorPipelineAdapterParams &params);
    ~LLPMMinorPipelineAdapter() override;

    void startup() override;

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
