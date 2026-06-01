#include "llpm/llpm_cache_port_adapter.hh"

#include <algorithm>
#include <dlfcn.h>

#include "base/logging.hh"

namespace gem5
{

namespace
{

constexpr uint64_t LlpmCacheRoleData = 1;
constexpr uint64_t LlpmCacheRoleInstruction = 2;
constexpr uint64_t LlpmGem5CommandRead = 1;
constexpr uint64_t LlpmGem5CommandWrite = 2;
constexpr uint64_t MaxBlockingAtomicCycles = 1000000;

uint64_t
byteMaskForSize(unsigned size)
{
    if (size == 0) {
        return 0;
    }
    if (size >= 8) {
        return ~uint64_t(0);
    }
    return (uint64_t(1) << size) - 1;
}

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

LLPMCachePortAdapter::CpuSidePort::CpuSidePort(
    const std::string &name, LLPMCachePortAdapter &owner)
    : ResponsePort(name), owner(owner)
{
}

void
LLPMCachePortAdapter::CpuSidePort::recvFunctional(PacketPtr pkt)
{
    owner.recvCpuFunctional(pkt);
}

Tick
LLPMCachePortAdapter::CpuSidePort::recvAtomic(PacketPtr pkt)
{
    return owner.recvCpuAtomic(pkt);
}

bool
LLPMCachePortAdapter::CpuSidePort::recvTimingReq(PacketPtr pkt)
{
    return owner.recvCpuTimingReq(pkt);
}

void
LLPMCachePortAdapter::CpuSidePort::recvRespRetry()
{
    owner.recvCpuRespRetry();
}

AddrRangeList
LLPMCachePortAdapter::CpuSidePort::getAddrRanges() const
{
    return owner.getCpuAddrRanges();
}

LLPMCachePortAdapter::MemSidePort::MemSidePort(
    const std::string &name, LLPMCachePortAdapter &owner)
    : RequestPort(name), owner(owner)
{
}

bool
LLPMCachePortAdapter::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return owner.recvMemTimingResp(pkt);
}

void
LLPMCachePortAdapter::MemSidePort::recvReqRetry()
{
    owner.recvMemReqRetry();
}

void
LLPMCachePortAdapter::MemSidePort::recvRangeChange()
{
    owner.recvMemRangeChange();
}

LLPMCachePortAdapter::LLPMCachePortAdapter(
    const LLPMCachePortAdapterParams &params)
    : SimObject(params),
      cpuSidePort(name() + ".cpu_side", *this),
      memSidePort(name() + ".mem_side", *this),
      component(params.component),
      libraryPath(params.library_path),
      resetCycles(params.reset_cycles),
      jsonConfig(params.json_config),
      cacheRole(params.cache_role),
      resultsDir(params.results_dir)
{
    loadComponentLibrary();
}

LLPMCachePortAdapter::~LLPMCachePortAdapter()
{
    destroyComponentLibrary();
}

Port &
LLPMCachePortAdapter::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "cpu_side") {
        return cpuSidePort;
    }
    if (if_name == "mem_side") {
        return memSidePort;
    }
    return SimObject::getPort(if_name, idx);
}

void
LLPMCachePortAdapter::recvCpuFunctional(PacketPtr pkt)
{
    recvCpuAtomic(pkt);
}

Tick
LLPMCachePortAdapter::recvCpuAtomic(PacketPtr pkt)
{
    if (pkt->getSize() > 8 || (!pkt->isRead() && !pkt->isWrite())) {
        if (memSidePort.isConnected()) {
            return memSidePort.sendAtomic(pkt);
        }
        fatal("LLPMCachePortAdapter cannot bypass unsupported packet without "
              "a connected mem_side port");
    }

    LlpmComponentRequest request = requestFromPacket(pkt);
    int32_t rc = submitFn(componentHandle, &request);
    if (rc < 0) {
        fatal("LLPM component submit failed for request %llu",
              static_cast<unsigned long long>(request.request_id));
    }
    ++adapterCrossings;

    for (uint64_t cycle = 0; cycle < MaxBlockingAtomicCycles; ++cycle) {
        rc = stepFn(componentHandle, 1);
        if (rc < 0) {
            fatal("LLPM component step failed");
        }
        LlpmComponentResponse response = {};
        rc = popResponseFn(componentHandle, &response);
        if (rc == 0) {
            if (response.ok == 0) {
                fatal("LLPM component returned error code %d",
                      response.error_code);
            }
            Tick memoryLatency = 0;
            if (memSidePort.isConnected()) {
                memoryLatency = memSidePort.sendAtomic(pkt);
            } else {
                applyResponseToPacket(pkt, response);
            }
            ++adapterCrossings;
            return memoryLatency + static_cast<Tick>(response.latency_cycles);
        }
        if (rc < 0) {
            fatal("LLPM component pop_response failed");
        }
    }
    fatal("LLPM component did not respond within atomic cycle budget");
    return 0;
}

bool
LLPMCachePortAdapter::recvCpuTimingReq(PacketPtr pkt)
{
    warn("LLPMCachePortAdapter timing path is not wired yet; use atomic mode");
    return false;
}

void
LLPMCachePortAdapter::recvCpuRespRetry()
{
}

AddrRangeList
LLPMCachePortAdapter::getCpuAddrRanges() const
{
    return memSidePort.getAddrRanges();
}

bool
LLPMCachePortAdapter::recvMemTimingResp(PacketPtr pkt)
{
    warn("LLPMCachePortAdapter memory timing path is not wired yet");
    return false;
}

void
LLPMCachePortAdapter::recvMemReqRetry()
{
}

void
LLPMCachePortAdapter::recvMemRangeChange()
{
    cpuSidePort.sendRangeChange();
}

void
LLPMCachePortAdapter::loadComponentLibrary()
{
    if (libraryPath.empty()) {
        fatal("LLPMCachePortAdapter requires a non-empty library_path");
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
    const uint32_t expectedComponent = componentIdForName(component);
    if (componentIdFn() != expectedComponent) {
        fatal("LLPM component id mismatch: %s exports %s",
              libraryPath.c_str(), componentNameFn());
    }

    LlpmComponentConfig config = {};
    config.abi_version = LLPM_GEM5_VERILATOR_ABI_VERSION;
    config.component_id = expectedComponent;
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
LLPMCachePortAdapter::destroyComponentLibrary()
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

LlpmComponentRequest
LLPMCachePortAdapter::requestFromPacket(PacketPtr pkt)
{
    if (pkt->getSize() > 8) {
        fatal("LLPMCachePortAdapter atomic path supports up to 8-byte packets");
    }
    LlpmComponentRequest request = {};
    request.request_id = nextRequestId++;
    request.operation = operationForPacket(pkt);
    request.size = pkt->getSize();
    request.addr = pkt->getAddr();
    request.byte_mask = byteMaskForSize(pkt->getSize());
    request.metadata0 = cacheRoleCode();
    request.metadata1 = pkt->isWrite() ? LlpmGem5CommandWrite
                                       : LlpmGem5CommandRead;

    if (pkt->isWrite() && pkt->hasData()) {
        const uint8_t *bytes = pkt->getConstPtr<uint8_t>();
        const unsigned count = std::min<unsigned>(pkt->getSize(), 8);
        for (unsigned index = 0; index < count; ++index) {
            request.data |= uint64_t(bytes[index]) << (8 * index);
        }
    }
    return request;
}

void
LLPMCachePortAdapter::applyResponseToPacket(
    PacketPtr pkt, const LlpmComponentResponse &response)
{
    if (response.ok == 0) {
        fatal("LLPM component returned error code %d", response.error_code);
    }
    if (pkt->isRead()) {
        uint8_t bytes[8] = {};
        const unsigned count = std::min<unsigned>(pkt->getSize(), 8);
        for (unsigned index = 0; index < count; ++index) {
            bytes[index] = uint8_t((response.data >> (8 * index)) & 0xff);
        }
        pkt->setData(bytes);
    }
    pkt->makeResponse();
}

uint64_t
LLPMCachePortAdapter::cacheRoleCode() const
{
    if (cacheRole == "data") {
        return LlpmCacheRoleData;
    }
    if (cacheRole == "instruction") {
        return LlpmCacheRoleInstruction;
    }
    fatal("unsupported LLPM cache role %s", cacheRole.c_str());
    return 0;
}

uint32_t
LLPMCachePortAdapter::componentIdForName(const std::string &component)
{
    if (component == "rtl-dcache") {
        return LLPM_COMPONENT_RTL_DCACHE;
    }
    if (component == "rtl-split-cache") {
        return LLPM_COMPONENT_RTL_SPLIT_CACHE;
    }
    fatal("unsupported LLPM cache component %s", component.c_str());
    return 0;
}

uint32_t
LLPMCachePortAdapter::operationForPacket(PacketPtr pkt)
{
    if (pkt->isRead()) {
        return LLPM_OP_CACHE_LOAD;
    }
    if (pkt->isWrite()) {
        return LLPM_OP_CACHE_STORE;
    }
    fatal("LLPMCachePortAdapter only supports read/write packets");
    return 0;
}

} // namespace gem5
