from m5.params import *
from m5.SimObject import SimObject


class LLPMComponentAdapter(SimObject):
    type = 'LLPMComponentAdapter'
    cxx_header = 'llpm/llpm_component_adapter.hh'
    cxx_class = 'gem5::LLPMComponentAdapter'

    component = Param.String('none', 'Selected LLPM component name')
    library_path = Param.String('', 'Path to the LLPM component shared library')
    reset_cycles = Param.Unsigned(2, 'Component reset cycles')
    json_config = Param.String('', 'JSON configuration passed to the LLPM component')
    results_dir = Param.String('', 'Directory for LLPM adapter result artifacts')
