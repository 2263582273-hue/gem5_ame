from m5.params import *
from m5.proxy import *
from m5.objects.TickedObject import TickedObject
from m5.SimObject import SimObject

class MemUnitWriteTiming(TickedObject):
    type = 'MemUnitWriteTiming'
    cxx_header = "cpu/minor/AME/MMU/write_timing_unit.hh"
    cxx_class="gem5::MemUnitWriteTiming"
    channel = Param.Unsigned(0, "Channel")
    cacheLineSize = Param.Unsigned(8, "Cache Line Size")
    VRF_LineSize = Param.Unsigned(16, "Reg Line Size")

class MemUnitReadTiming(TickedObject):
    type = 'MemUnitReadTiming'
    cxx_header = "cpu/minor/AME/MMU/read_timing_unit.hh"
    cxx_class="gem5::MemUnitReadTiming"    
    channel = Param.Unsigned(0, "Channel")
    cacheLineSize = Param.Unsigned(8, "Cache Line Size")
    VRF_LineSize = Param.Unsigned(16, "Reg Line Size")

class AMEMemUnit(SimObject):
    type = 'AMEMemUnit'
    cxx_header = "cpu/minor/AME/MMU/ame_mem_unit.hh"
    cxx_class="gem5::AMEMemUnit"    
    memReader = Param.MemUnitReadTiming(MemUnitReadTiming(),"read streaming submodule")
    memWriter = Param.MemUnitWriteTiming(MemUnitWriteTiming(),"write streaming submodule")
