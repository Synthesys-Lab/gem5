#include "llpm/llpm_minor_pipeline_adapter.hh"

#include <algorithm>
#include <cstring>
#include <dlfcn.h>
#include <memory>

#include "base/logging.hh"
#include "mem/packet.hh"
#include "mem/request.hh"

namespace gem5
{

namespace
{

constexpr uint64_t MaxDrainResponses = 1024;
constexpr uint64_t MinorStateHalted = 3;
constexpr uint64_t SidecarBurstCount = 100000;
constexpr uint64_t SidecarPeriodTicks = 10;
constexpr uint64_t StepsPerBurst = 4;
constexpr uint64_t StepsBeforeReset = 16;

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

uint64_t
dataFromPacket(PacketPtr pkt)
{
    if (!pkt->hasData()) {
        return 0;
    }
    uint64_t data = 0;
    const uint8_t *bytes = pkt->getConstPtr<uint8_t>();
    const unsigned count = std::min<unsigned>(pkt->getSize(), 8);
    for (unsigned index = 0; index < count; ++index) {
        data |= uint64_t(bytes[index]) << (8 * index);
    }
    return data;
}

} // anonymous namespace

LLPMMinorPipelineAdapter::MemSidePort::MemSidePort(
    const std::string &name,
    LLPMMinorPipelineAdapter &owner,
    const std::string &role)
    : RequestPort(name), owner(owner), role(role)
{
}

bool
LLPMMinorPipelineAdapter::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return owner.recvMemTimingResp(role, pkt);
}

void
LLPMMinorPipelineAdapter::MemSidePort::recvReqRetry()
{
    owner.recvMemReqRetry(role);
}

void
LLPMMinorPipelineAdapter::MemSidePort::recvRangeChange()
{
    owner.recvMemRangeChange(role);
}

LLPMMinorPipelineAdapter::LLPMMinorPipelineAdapterStats::
LLPMMinorPipelineAdapterStats(statistics::Group *parent)
    : statistics::Group(parent),
      ADD_STAT(adapter_crossings, statistics::units::Count::get(),
               "LLPM Minor pipeline adapter request/response crossings"),
      ADD_STAT(verilated_cycles, statistics::units::Cycle::get(),
               "Component-reported Verilated Minor pipeline cycles"),
      ADD_STAT(component_requests, statistics::units::Count::get(),
               "LLPM Minor pipeline C ABI requests submitted from gem5"),
      ADD_STAT(component_responses, statistics::units::Count::get(),
               "LLPM Minor pipeline C ABI responses observed by gem5"),
      ADD_STAT(resets, statistics::units::Count::get(),
               "LLPM Minor pipeline reset operations"),
      ADD_STAT(pipeline_steps, statistics::units::Count::get(),
               "LLPM Minor pipeline step operations"),
      ADD_STAT(ifetch_requests, statistics::units::Count::get(),
               "Instruction-fetch memory requests emitted by LLPM Minor"),
      ADD_STAT(data_load_requests, statistics::units::Count::get(),
               "Data-load memory requests emitted by LLPM Minor"),
      ADD_STAT(data_store_requests, statistics::units::Count::get(),
               "Data-store memory requests emitted by LLPM Minor"),
      ADD_STAT(memory_responses, statistics::units::Count::get(),
               "Memory responses returned from gem5 to LLPM Minor"),
      ADD_STAT(halts, statistics::units::Count::get(),
               "LLPM Minor pipeline halt events"),
      ADD_STAT(faults, statistics::units::Count::get(),
               "LLPM Minor pipeline fault events")
{
    adapter_crossings.scalar(adapterCrossings);
    verilated_cycles.scalar(verilatedCycles);
    component_requests.scalar(componentRequests);
    component_responses.scalar(componentResponses);
    resets.scalar(resetsCount);
    pipeline_steps.scalar(pipelineSteps);
    ifetch_requests.scalar(ifetchRequests);
    data_load_requests.scalar(dataLoadRequests);
    data_store_requests.scalar(dataStoreRequests);
    memory_responses.scalar(memoryResponses);
    halts.scalar(haltCount);
    faults.scalar(faultCount);
}

LLPMMinorPipelineAdapter::LLPMMinorPipelineAdapter(
    const LLPMMinorPipelineAdapterParams &params)
    : SimObject(params),
      ifetchSidePort(name() + ".ifetch_side", *this, "instruction"),
      dataSidePort(name() + ".data_side", *this, "data"),
      stats(this),
      sidecarEvent([this]{ processSidecarEvent(); }, name() + ".sidecar"),
      component(params.component),
      libraryPath(params.library_path),
      resetCycles(params.reset_cycles),
      jsonConfig(params.json_config),
      resultsDir(params.results_dir)
{
    if (component != "rtl-minor-pipeline") {
        fatal("LLPMMinorPipelineAdapter only supports rtl-minor-pipeline");
    }
    loadComponentLibrary();
}

LLPMMinorPipelineAdapter::~LLPMMinorPipelineAdapter()
{
    destroyComponentLibrary();
}

void
LLPMMinorPipelineAdapter::startup()
{
    if (!sidecarEvent.scheduled()) {
        schedule(sidecarEvent, curTick());
    }
}

Port &
LLPMMinorPipelineAdapter::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "ifetch_side") {
        return ifetchSidePort;
    }
    if (if_name == "data_side") {
        return dataSidePort;
    }
    return SimObject::getPort(if_name, idx);
}

bool
LLPMMinorPipelineAdapter::recvMemTimingResp(
    const std::string &role, PacketPtr pkt)
{
    warn("LLPMMinorPipelineAdapter %s timing response path is not wired yet",
         role.c_str());
    return false;
}

void
LLPMMinorPipelineAdapter::recvMemReqRetry(const std::string &role)
{
}

void
LLPMMinorPipelineAdapter::recvMemRangeChange(const std::string &role)
{
}

void
LLPMMinorPipelineAdapter::loadComponentLibrary()
{
    if (libraryPath.empty()) {
        fatal("LLPMMinorPipelineAdapter requires a non-empty library_path");
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
    statCountFn = loadRequiredSymbol<StatCountFn>(
        libraryHandle, "llpm_component_stat_count");
    statFn = loadRequiredSymbol<StatFn>(
        libraryHandle, "llpm_component_stat");

    if (abiVersionFn() != LLPM_GEM5_VERILATOR_ABI_VERSION) {
        fatal("LLPM component ABI version mismatch for %s", libraryPath.c_str());
    }
    if (componentIdFn() != LLPM_COMPONENT_RTL_MINOR_PIPELINE) {
        fatal("LLPM component id mismatch: %s exports %s",
              libraryPath.c_str(), componentNameFn());
    }

    LlpmComponentConfig config = {};
    config.abi_version = LLPM_GEM5_VERILATOR_ABI_VERSION;
    config.component_id = LLPM_COMPONENT_RTL_MINOR_PIPELINE;
    config.reset_cycles = resetCycles;
    config.json_config = jsonConfig.c_str();
    componentHandle = createFn(&config);
    if (componentHandle == nullptr) {
        fatal("LLPM component create failed for %s", component.c_str());
    }
    if (resetFn(componentHandle, resetCycles) < 0) {
        fatal("LLPM component reset failed for %s", component.c_str());
    }
    refreshComponentStats();
}

void
LLPMMinorPipelineAdapter::destroyComponentLibrary()
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

void
LLPMMinorPipelineAdapter::processSidecarEvent()
{
    if (componentHandle == nullptr || sidecarBursts >= SidecarBurstCount) {
        return;
    }

    if (componentHalted || stepsSinceReset >= StepsBeforeReset) {
        LlpmComponentRequest reset = {};
        reset.request_id = nextRequestId++;
        reset.operation = LLPM_OP_PIPELINE_RESET;
        reset.size = 1;
        submitAndStep(reset);
        componentHalted = false;
        stepsSinceReset = 0;
    }

    for (uint64_t index = 0; index < StepsPerBurst; ++index) {
        LlpmComponentRequest step = {};
        step.request_id = nextRequestId++;
        step.operation = LLPM_OP_PIPELINE_STEP;
        step.size = 1;
        submitAndStep(step);
        ++stepsSinceReset;
        if (componentHalted) {
            break;
        }
    }

    refreshComponentStats();
    ++sidecarBursts;
    schedule(sidecarEvent, curTick() + SidecarPeriodTicks);
}

void
LLPMMinorPipelineAdapter::submitAndStep(LlpmComponentRequest request)
{
    int32_t rc = submitFn(componentHandle, &request);
    if (rc < 0) {
        fatal("LLPM Minor pipeline submit failed for request %llu",
              static_cast<unsigned long long>(request.request_id));
    }
    ++stats.adapterCrossings;

    rc = stepFn(componentHandle, 1);
    if (rc < 0) {
        fatal("LLPM Minor pipeline step failed");
    }
    drainReadyResponses();
}

void
LLPMMinorPipelineAdapter::drainReadyResponses()
{
    for (uint64_t index = 0; index < MaxDrainResponses; ++index) {
        LlpmComponentResponse response = {};
        int32_t rc = popResponseFn(componentHandle, &response);
        if (rc == 1) {
            return;
        }
        if (rc < 0) {
            fatal("LLPM Minor pipeline pop_response failed");
        }

        ++stats.adapterCrossings;
        if (response.operation == LLPM_OP_IFETCH_REQUEST ||
            response.operation == LLPM_OP_DATA_LOAD_REQUEST ||
            response.operation == LLPM_OP_DATA_STORE_REQUEST) {
            serviceMemoryRequest(response);
            continue;
        }

        if (response.operation == LLPM_OP_PIPELINE_STEP &&
            response.metadata0 == MinorStateHalted) {
            componentHalted = true;
        }
        if (response.ok == 0) {
            componentHalted = true;
        }
    }
    fatal("LLPM Minor pipeline produced too many ready responses");
}

void
LLPMMinorPipelineAdapter::serviceMemoryRequest(
    const LlpmComponentResponse &response)
{
    const uint64_t data = sendAtomicMemoryRequest(response);
    submitMemoryResponse(response, data);
}

uint64_t
LLPMMinorPipelineAdapter::sendAtomicMemoryRequest(
    const LlpmComponentResponse &response)
{
    RequestPort *port = nullptr;
    Request::Flags flags = 0;
    if (response.operation == LLPM_OP_IFETCH_REQUEST) {
        port = &ifetchSidePort;
        flags.set(Request::INST_FETCH);
    } else {
        port = &dataSidePort;
    }

    const Addr addr = static_cast<Addr>(response.metadata2);
    const unsigned size = std::min<unsigned>(
        std::max<unsigned>(static_cast<unsigned>(response.metadata3), 1),
        8);
    const bool isWrite = response.operation == LLPM_OP_DATA_STORE_REQUEST;
    const MemCmd command = isWrite ? MemCmd::WriteReq : MemCmd::ReadReq;

    if (port == nullptr || !port->isConnected()) {
        return isWrite ? response.data : 0;
    }

    RequestPtr request = std::make_shared<Request>(
        addr, size, flags, Request::funcRequestorId);
    PacketPtr pkt = new Packet(request, command);
    pkt->allocate();
    if (isWrite) {
        uint8_t bytes[8] = {};
        for (unsigned index = 0; index < size; ++index) {
            bytes[index] = uint8_t((response.data >> (8 * index)) & 0xff);
        }
        pkt->setData(bytes);
    }

    port->sendAtomic(pkt);
    const uint64_t data = isWrite ? response.data : dataFromPacket(pkt);
    delete pkt;
    return data;
}

void
LLPMMinorPipelineAdapter::submitMemoryResponse(
    const LlpmComponentResponse &response,
    uint64_t data)
{
    LlpmComponentRequest request = {};
    request.request_id = response.request_id;
    request.operation = LLPM_OP_MEMORY_RESPONSE;
    request.size = 1;
    request.data = data;
    request.metadata0 = 0;
    submitAndStep(request);
}

void
LLPMMinorPipelineAdapter::refreshComponentStats()
{
    if (componentHandle == nullptr || statCountFn == nullptr || statFn == nullptr) {
        return;
    }
    const uint32_t count = statCountFn(componentHandle);
    for (uint32_t index = 0; index < count; ++index) {
        LlpmComponentStat stat = {};
        if (statFn(componentHandle, index, &stat) < 0 || stat.name == nullptr) {
            fatal("LLPM Minor pipeline stat query failed at index %u", index);
        }
        const uint64_t value = stat.value_u64;
        if (std::strcmp(stat.name, "requests") == 0) {
            stats.componentRequests = value;
        } else if (std::strcmp(stat.name, "responses") == 0) {
            stats.componentResponses = value;
        } else if (std::strcmp(stat.name, "cycles") == 0) {
            stats.verilatedCycles = value;
        } else if (std::strcmp(stat.name, "resets") == 0) {
            stats.resetsCount = value;
        } else if (std::strcmp(stat.name, "steps") == 0) {
            stats.pipelineSteps = value;
        } else if (std::strcmp(stat.name, "ifetch_requests") == 0) {
            stats.ifetchRequests = value;
        } else if (std::strcmp(stat.name, "data_load_requests") == 0) {
            stats.dataLoadRequests = value;
        } else if (std::strcmp(stat.name, "data_store_requests") == 0) {
            stats.dataStoreRequests = value;
        } else if (std::strcmp(stat.name, "memory_responses") == 0) {
            stats.memoryResponses = value;
        } else if (std::strcmp(stat.name, "halts") == 0) {
            stats.haltCount = value;
        } else if (std::strcmp(stat.name, "faults") == 0) {
            stats.faultCount = value;
        }
    }
}

} // namespace gem5
