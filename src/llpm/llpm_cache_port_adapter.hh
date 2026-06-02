#ifndef __LLPM_CACHE_PORT_ADAPTER_HH__
#define __LLPM_CACHE_PORT_ADAPTER_HH__

#include <cstdint>
#include <string>

#include "base/statistics.hh"
#include "llpm/llpm_gem5_verilator_abi.h"
#include "mem/port.hh"
#include "params/LLPMCachePortAdapter.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class LLPMCachePortAdapter : public SimObject
{
  private:
    class CpuSidePort : public ResponsePort
    {
      private:
        LLPMCachePortAdapter &owner;

      public:
        CpuSidePort(const std::string &name, LLPMCachePortAdapter &owner);

      protected:
        void recvFunctional(PacketPtr pkt) override;
        Tick recvAtomic(PacketPtr pkt) override;
        bool recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
        AddrRangeList getAddrRanges() const override;
    };

    class MemSidePort : public RequestPort
    {
      private:
        LLPMCachePortAdapter &owner;

      public:
        MemSidePort(const std::string &name, LLPMCachePortAdapter &owner);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;
        void recvRangeChange() override;
    };

    struct LLPMCachePortAdapterStats : public statistics::Group
    {
        LLPMCachePortAdapterStats(statistics::Group *parent);

        statistics::Scalar adapter_crossings;
        statistics::Scalar verilated_cycles;
        statistics::Scalar component_requests;
        statistics::Scalar component_hits;
        statistics::Scalar component_misses;
    };

    CpuSidePort cpuSidePort;
    MemSidePort memSidePort;
    LLPMCachePortAdapterStats stats;
    void *libraryHandle = nullptr;
    void *componentHandle = nullptr;
    uint64_t nextRequestId = 1;

    using AbiVersionFn = uint32_t (*)();
    using ComponentIdFn = uint32_t (*)();
    using ComponentNameFn = const char *(*)();
    using CreateFn = void *(*)(const LlpmComponentConfig *);
    using DestroyFn = void (*)(void *);
    using ResetFn = int32_t (*)(void *, uint32_t);
    using SubmitFn = int32_t (*)(void *, const LlpmComponentRequest *);
    using StepFn = int32_t (*)(void *, uint32_t);
    using PopResponseFn = int32_t (*)(void *, LlpmComponentResponse *);

    AbiVersionFn abiVersionFn = nullptr;
    ComponentIdFn componentIdFn = nullptr;
    ComponentNameFn componentNameFn = nullptr;
    CreateFn createFn = nullptr;
    DestroyFn destroyFn = nullptr;
    ResetFn resetFn = nullptr;
    SubmitFn submitFn = nullptr;
    StepFn stepFn = nullptr;
    PopResponseFn popResponseFn = nullptr;

    void loadComponentLibrary();
    void destroyComponentLibrary();
    LlpmComponentRequest requestFromPacket(PacketPtr pkt);
    void applyResponseToPacket(PacketPtr pkt,
                               const LlpmComponentResponse &response);
    uint64_t cacheRoleCode() const;
    static uint32_t componentIdForName(const std::string &component);
    static uint32_t operationForPacket(PacketPtr pkt);

  public:
    LLPMCachePortAdapter(const LLPMCachePortAdapterParams &params);
    ~LLPMCachePortAdapter() override;

    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    void recvCpuFunctional(PacketPtr pkt);
    Tick recvCpuAtomic(PacketPtr pkt);
    bool recvCpuTimingReq(PacketPtr pkt);
    void recvCpuRespRetry();
    AddrRangeList getCpuAddrRanges() const;
    bool recvMemTimingResp(PacketPtr pkt);
    void recvMemReqRetry();
    void recvMemRangeChange();

    const std::string component;
    const std::string libraryPath;
    const unsigned resetCycles;
    const std::string jsonConfig;
    const std::string cacheRole;
    const std::string resultsDir;
};

} // namespace gem5

#endif // __LLPM_CACHE_PORT_ADAPTER_HH__
