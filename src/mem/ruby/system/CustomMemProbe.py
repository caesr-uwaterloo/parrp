from m5.params import *
from m5.SimObject import SimObject
from m5.objects.Probe import *

class CustomMemProbe(ProbeListenerObject):
    type = 'CustomMemProbe'
    cxx_class = 'gem5::ruby::CustomMemProbe'
    cxx_header = 'mem/ruby/system/CustomMemProbe.hh'

    trace_file = Param.String("mem", "Memory trace output file")
    trace_compress = Param.Bool(True, "Enable compression")
    enable_raw_trace = Param.Bool(False, "Enable raw memory trace recording")
    use_traffic_gen = Param.Bool(False, "If traffic generator is in use")
    cpus = VectorParam.BaseSimpleCPU([], "List of cpus in the system")
