from m5.params import *
from m5.objects.SimpleMemory import SimpleMemory
from m5.objects.AbstractMemory import ResponsePort

# BASED ON OWN IMPLEMENTATION (SimpleMemory)

# ORIGINAL Simple memory: 30ns and 0ns. Bandwith=12.8GB/s (DDR3-1600)
class ScratchpadMemory(SimpleMemory):
   type = 'ScratchpadMemory'
   cxx_header = "mem/spm_mem.hh"
   cxx_class = "gem5::memory::ScratchpadMemory"
   port = ResponsePort("Slave ports")
   #latency=Param.Latency('1ns',"the latency")
   latency_write = Param.Latency('10ns', "Write latency in SPM")
   latency_write_var = Param.Latency('0ns', "Write latency in SPM variable")
   # Modeling energy
   energy_read = Param.Float('300', "Energy for each reading (pJ)");
   energy_write = Param.Float('430', "Energy for each writting (pJ)");
   energy_overhead = Param.Float('100', "Overhead energy (pJ)");

   # This parameter is defined as the acceptance rate of request. Not very clear...
   # In the config.ini file is the inverse, e.g. BW=12.8GB/s, bandwidth = 73.0 ps/b
   #
   # In some papers it is described as the maximum transfer rate, description that makes sense
   bandwidth = Param.MemoryBandwidth('64GiB/s',
                                     "Combined read and write bandwidth")