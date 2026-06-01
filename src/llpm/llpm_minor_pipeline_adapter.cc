#include "llpm/llpm_minor_pipeline_adapter.hh"

#include "base/logging.hh"

namespace gem5
{

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

LLPMMinorPipelineAdapter::LLPMMinorPipelineAdapter(
    const LLPMMinorPipelineAdapterParams &params)
    : SimObject(params),
      ifetchSidePort(name() + ".ifetch_side", *this, "instruction"),
      dataSidePort(name() + ".data_side", *this, "data"),
      component(params.component),
      libraryPath(params.library_path),
      resetCycles(params.reset_cycles),
      jsonConfig(params.json_config),
      resultsDir(params.results_dir)
{
    if (component != "rtl-minor-pipeline") {
        fatal("LLPMMinorPipelineAdapter only supports rtl-minor-pipeline");
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
    warn("LLPMMinorPipelineAdapter %s response path is not wired yet",
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

} // namespace gem5
