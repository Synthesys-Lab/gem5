from m5.objects.BranchPredictor import ConditionalPredictor
from m5.params import *


class LLPMPHT2Conditional(ConditionalPredictor):
    type = 'LLPMPHT2Conditional'
    cxx_header = 'llpm/llpm_pht2_conditional.hh'
    cxx_class = 'gem5::branch_prediction::LLPMPHT2Conditional'

    component = Param.String('rtl-pht2-bpred', 'Selected LLPM predictor component')
    library_path = Param.String('', 'Path to the LLPM predictor shared library')
    reset_cycles = Param.Unsigned(2, 'Component reset cycles')
    json_config = Param.String('', 'JSON configuration passed to the LLPM component')
    entries = Param.Unsigned(1024, 'PHT entry count')
    initial_counter = Param.Unsigned(1, 'Initial 2-bit PHT counter value')
    results_dir = Param.String('', 'Directory for LLPM adapter result artifacts')
