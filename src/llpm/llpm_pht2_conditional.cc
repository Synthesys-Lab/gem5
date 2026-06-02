#include "llpm/llpm_pht2_conditional.hh"

#include <dlfcn.h>

#include "base/logging.hh"

namespace gem5
{

namespace branch_prediction
{

namespace
{

constexpr uint64_t MaxBlockingPredictorCycles = 1000000;

template <typename Fn>
Fn
loadRequiredSymbol(void *handle, const char *symbol)
{
    void *raw = dlsym(handle, symbol);
    if (raw == nullptr) {
        fatal("missing LLPM component ABI symbol %s", symbol);
    }
    return reinterpret_cast<Fn>(raw);
}

} // anonymous namespace

LLPMPHT2Conditional::LLPMPHT2ConditionalStats::LLPMPHT2ConditionalStats(
    statistics::Group *parent)
    : statistics::Group(parent),
      ADD_STAT(adapter_crossings, statistics::units::Count::get(),
               "LLPM PHT2 predictor query/update boundary crossings"),
      ADD_STAT(verilated_cycles, statistics::units::Cycle::get(),
               "Component-reported Verilated cycles spent in LLPM PHT2"),
      ADD_STAT(component_requests, statistics::units::Count::get(),
               "LLPM PHT2 requests submitted from gem5"),
      ADD_STAT(predictions, statistics::units::Count::get(),
               "Conditional branch direction queries submitted to LLPM PHT2"),
      ADD_STAT(updates, statistics::units::Count::get(),
               "Conditional branch direction updates submitted to LLPM PHT2"),
      ADD_STAT(predicted_taken, statistics::units::Count::get(),
               "LLPM PHT2 predictions that returned taken"),
      ADD_STAT(correct, statistics::units::Count::get(),
               "Committed LLPM PHT2 direction predictions that were correct"),
      ADD_STAT(mispredictions, statistics::units::Count::get(),
               "Committed LLPM PHT2 direction predictions that were incorrect")
{
}

LLPMPHT2Conditional::LLPMPHT2Conditional(
    const LLPMPHT2ConditionalParams &params)
    : ConditionalPredictor(params),
      stats(this),
      component(params.component),
      libraryPath(params.library_path),
      resetCycles(params.reset_cycles),
      jsonConfig(params.json_config),
      entries(params.entries),
      initialCounter(params.initial_counter),
      resultsDir(params.results_dir)
{
    if (component != "rtl-pht2-bpred") {
        fatal("LLPMPHT2Conditional only supports rtl-pht2-bpred");
    }
    if (initialCounter > 3) {
        fatal("LLPMPHT2Conditional initial_counter must be a 2-bit value");
    }
    loadComponentLibrary();
}

LLPMPHT2Conditional::~LLPMPHT2Conditional()
{
    destroyComponentLibrary();
}

bool
LLPMPHT2Conditional::lookup(ThreadID tid, Addr pc, void * &bp_history)
{
    if (bp_history != nullptr) {
        fatal("LLPMPHT2Conditional lookup received non-null history");
    }

    LlpmComponentRequest request = {};
    request.request_id = nextRequestId++;
    request.operation = LLPM_OP_BPRED_QUERY;
    request.addr = pc;
    request.metadata0 = fallthroughAddr(pc);
    request.metadata1 = 0;

    LlpmComponentResponse response = submitBlocking(request);
    auto *history = new PHT2History();
    history->token = response.metadata0 != 0 ? response.metadata0
                                             : request.request_id;
    history->predictedTaken = response.data != 0;
    bp_history = history;

    ++stats.predictions;
    if (history->predictedTaken) {
        ++stats.predicted_taken;
    }
    return history->predictedTaken;
}

void
LLPMPHT2Conditional::branchPlaceholder(ThreadID tid, Addr pc, bool uncond,
                                       void * &bp_history)
{
    bp_history = nullptr;
}

void
LLPMPHT2Conditional::updateHistories(ThreadID tid, Addr pc, bool uncond,
                                     bool taken, Addr target,
                                     const StaticInstPtr &inst,
                                     void * &bp_history)
{
}

void
LLPMPHT2Conditional::update(ThreadID tid, Addr pc, bool taken,
                            void * &bp_history, bool squashed,
                            const StaticInstPtr &inst, Addr target)
{
    if (squashed) {
        return;
    }
    auto *history = static_cast<PHT2History *>(bp_history);
    if (history == nullptr) {
        return;
    }

    LlpmComponentRequest request = {};
    request.request_id = nextRequestId++;
    request.operation = LLPM_OP_BPRED_UPDATE;
    request.addr = pc;
    request.metadata0 = history->token;
    request.metadata1 = taken ? 1 : 0;
    request.metadata2 = taken ? target : fallthroughAddr(pc);

    submitBlocking(request);
    ++stats.updates;
    if (history->predictedTaken == taken) {
        ++stats.correct;
    } else {
        ++stats.mispredictions;
    }

    delete history;
    bp_history = nullptr;
}

void
LLPMPHT2Conditional::squash(ThreadID tid, void * &bp_history)
{
    auto *history = static_cast<PHT2History *>(bp_history);
    delete history;
    bp_history = nullptr;
}

void
LLPMPHT2Conditional::loadComponentLibrary()
{
    if (libraryPath.empty()) {
        fatal("LLPMPHT2Conditional requires a non-empty library_path");
    }
    libraryHandle = dlopen(libraryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (libraryHandle == nullptr) {
        fatal("failed to load LLPM component library %s: %s",
              libraryPath.c_str(), dlerror());
    }

    abiVersionFn = loadRequiredSymbol<AbiVersionFn>(
        libraryHandle, "llpm_component_abi_version");
    componentIdFn = loadRequiredSymbol<ComponentIdFn>(
        libraryHandle, "llpm_component_id");
    componentNameFn = loadRequiredSymbol<ComponentNameFn>(
        libraryHandle, "llpm_component_name");
    createFn = loadRequiredSymbol<CreateFn>(
        libraryHandle, "llpm_component_create");
    destroyFn = loadRequiredSymbol<DestroyFn>(
        libraryHandle, "llpm_component_destroy");
    resetFn = loadRequiredSymbol<ResetFn>(
        libraryHandle, "llpm_component_reset");
    submitFn = loadRequiredSymbol<SubmitFn>(
        libraryHandle, "llpm_component_submit");
    stepFn = loadRequiredSymbol<StepFn>(
        libraryHandle, "llpm_component_step");
    popResponseFn = loadRequiredSymbol<PopResponseFn>(
        libraryHandle, "llpm_component_pop_response");

    if (abiVersionFn() != LLPM_GEM5_VERILATOR_ABI_VERSION) {
        fatal("LLPM component ABI version mismatch for %s", libraryPath.c_str());
    }
    if (componentIdFn() != LLPM_COMPONENT_RTL_PHT2_BPRED) {
        fatal("LLPM component id mismatch: %s exports %s",
              libraryPath.c_str(), componentNameFn());
    }

    LlpmComponentConfig config = {};
    config.abi_version = LLPM_GEM5_VERILATOR_ABI_VERSION;
    config.component_id = LLPM_COMPONENT_RTL_PHT2_BPRED;
    config.reset_cycles = resetCycles;
    config.json_config = jsonConfig.c_str();
    componentHandle = createFn(&config);
    if (componentHandle == nullptr) {
        fatal("LLPM component create failed for %s", component.c_str());
    }
    if (resetFn(componentHandle, resetCycles) < 0) {
        fatal("LLPM component reset failed for %s", component.c_str());
    }
}

void
LLPMPHT2Conditional::destroyComponentLibrary()
{
    if (componentHandle != nullptr && destroyFn != nullptr) {
        destroyFn(componentHandle);
        componentHandle = nullptr;
    }
    if (libraryHandle != nullptr) {
        dlclose(libraryHandle);
        libraryHandle = nullptr;
    }
}

LlpmComponentResponse
LLPMPHT2Conditional::submitBlocking(const LlpmComponentRequest &request)
{
    int32_t rc = submitFn(componentHandle, &request);
    if (rc < 0) {
        fatal("LLPM PHT2 submit failed for request %llu",
              static_cast<unsigned long long>(request.request_id));
    }
    ++stats.component_requests;
    ++stats.adapter_crossings;

    for (uint64_t cycle = 0; cycle < MaxBlockingPredictorCycles; ++cycle) {
        rc = stepFn(componentHandle, 1);
        if (rc < 0) {
            fatal("LLPM PHT2 step failed");
        }
        LlpmComponentResponse response = {};
        rc = popResponseFn(componentHandle, &response);
        if (rc == 0) {
            if (response.ok == 0) {
                fatal("LLPM PHT2 returned error code %d",
                      response.error_code);
            }
            ++stats.adapter_crossings;
            stats.verilated_cycles += response.latency_cycles;
            return response;
        }
        if (rc < 0) {
            fatal("LLPM PHT2 pop_response failed");
        }
    }
    fatal("LLPM PHT2 did not respond within predictor cycle budget");
    return {};
}

Addr
LLPMPHT2Conditional::fallthroughAddr(Addr pc) const
{
    const Addr step = Addr(1) << instShiftAmt;
    return pc + (step == 0 ? Addr(1) : step);
}

} // namespace branch_prediction
} // namespace gem5
