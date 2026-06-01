from m5.params import *
from m5.SimObject import SimObject


class LLPMMinorPipelineAdapter(SimObject):
    type = 'LLPMMinorPipelineAdapter'
    cxx_header = 'llpm/llpm_minor_pipeline_adapter.hh'
    cxx_class = 'gem5::LLPMMinorPipelineAdapter'

    ifetch_side = RequestPort('Instruction fetch request port to gem5 memory')
    data_side = RequestPort('Data load/store request port to gem5 memory')
    component = Param.String('rtl-minor-pipeline', 'Selected LLPM pipeline component')
    library_path = Param.String('', 'Path to the LLPM pipeline shared library')
    reset_cycles = Param.Unsigned(2, 'Component reset cycles')
    json_config = Param.String('', 'JSON configuration passed to the LLPM component')
    results_dir = Param.String('', 'Directory for LLPM adapter result artifacts')
