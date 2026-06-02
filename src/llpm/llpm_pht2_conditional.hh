#ifndef __LLPM_PHT2_CONDITIONAL_HH__
#define __LLPM_PHT2_CONDITIONAL_HH__

#include <cstdint>
#include <string>

#include "base/statistics.hh"
#include "cpu/pred/conditional.hh"
#include "llpm/llpm_gem5_verilator_abi.h"
#include "params/LLPMPHT2Conditional.hh"

namespace gem5
{

namespace branch_prediction
{

class LLPMPHT2Conditional : public ConditionalPredictor
{
  private:
    struct PHT2History
    {
        uint64_t token = 0;
        bool predictedTaken = false;
    };

    struct LLPMPHT2ConditionalStats : public statistics::Group
    {
        LLPMPHT2ConditionalStats(statistics::Group *parent);

        statistics::Scalar adapter_crossings;
        statistics::Scalar verilated_cycles;
        statistics::Scalar component_requests;
        statistics::Scalar predictions;
        statistics::Scalar updates;
        statistics::Scalar predicted_taken;
        statistics::Scalar correct;
        statistics::Scalar mispredictions;
    };

    using AbiVersionFn = uint32_t (*)();
    using ComponentIdFn = uint32_t (*)();
    using ComponentNameFn = const char *(*)();
    using CreateFn = void *(*)(const LlpmComponentConfig *);
    using DestroyFn = void (*)(void *);
    using ResetFn = int32_t (*)(void *, uint32_t);
    using SubmitFn = int32_t (*)(void *, const LlpmComponentRequest *);
    using StepFn = int32_t (*)(void *, uint32_t);
    using PopResponseFn = int32_t (*)(void *, LlpmComponentResponse *);

    void *libraryHandle = nullptr;
    void *componentHandle = nullptr;
    AbiVersionFn abiVersionFn = nullptr;
    ComponentIdFn componentIdFn = nullptr;
    ComponentNameFn componentNameFn = nullptr;
    CreateFn createFn = nullptr;
    DestroyFn destroyFn = nullptr;
    ResetFn resetFn = nullptr;
    SubmitFn submitFn = nullptr;
    StepFn stepFn = nullptr;
    PopResponseFn popResponseFn = nullptr;
    uint64_t nextRequestId = 1;

    LLPMPHT2ConditionalStats stats;

    void loadComponentLibrary();
    void destroyComponentLibrary();
    LlpmComponentResponse submitBlocking(const LlpmComponentRequest &request);
    Addr fallthroughAddr(Addr pc) const;

  public:
    LLPMPHT2Conditional(const LLPMPHT2ConditionalParams &params);
    ~LLPMPHT2Conditional() override;

    bool lookup(ThreadID tid, Addr pc, void * &bp_history) override;
    void branchPlaceholder(ThreadID tid, Addr pc, bool uncond,
                           void * &bp_history) override;
    void updateHistories(ThreadID tid, Addr pc, bool uncond, bool taken,
                         Addr target, const StaticInstPtr &inst,
                         void * &bp_history) override;
    void update(ThreadID tid, Addr pc, bool taken, void * &bp_history,
                bool squashed, const StaticInstPtr &inst, Addr target) override;
    void squash(ThreadID tid, void * &bp_history) override;

    const std::string component;
    const std::string libraryPath;
    const unsigned resetCycles;
    const std::string jsonConfig;
    const unsigned entries;
    const unsigned initialCounter;
    const std::string resultsDir;
};

} // namespace branch_prediction
} // namespace gem5

#endif // __LLPM_PHT2_CONDITIONAL_HH__
