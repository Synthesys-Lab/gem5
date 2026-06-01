from m5.params import *
from m5.SimObject import SimObject


class LLPMCachePortAdapter(SimObject):
    type = 'LLPMCachePortAdapter'
    cxx_header = 'llpm/llpm_cache_port_adapter.hh'
    cxx_class = 'gem5::LLPMCachePortAdapter'

    cpu_side = ResponsePort('CPU-side port receiving gem5 cache requests')
    mem_side = RequestPort('Memory-side port forwarding requests to gem5 memory')
    component = Param.String('rtl-dcache', 'Selected LLPM cache component name')
    library_path = Param.String('', 'Path to the LLPM cache shared library')
    reset_cycles = Param.Unsigned(2, 'Component reset cycles')
    json_config = Param.String('', 'JSON configuration passed to the LLPM component')
    cache_role = Param.String('data', 'Cache role: instruction or data')
    results_dir = Param.String('', 'Directory for LLPM adapter result artifacts')
