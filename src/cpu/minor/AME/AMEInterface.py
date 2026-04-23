from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject
from m5.objects.SystolicArrayCore import SystolicArrayCore
from m5.objects.TickedObject import TickedObject
from m5.objects.MMU import AMEMemUnit



# # 脉动阵列功能单元，增加了 systolic array 相关的参数
# class SystolicArrayCore(SimObject):
#     type = "SystolicArrayCore"
#     cxx_header = "cpu/minor/systolicArray/systolic_array_core.hh"
#     cxx_class = "gem5::SystolicArrayCore"

#     # opClasses = minorMakeOpClassSet(["SystolicMMA"])
#     opLat = Param.Cycles(1, "latency in cycles")

#     # 脉动阵列硬件参数
#     arrayRows = Param.Unsigned(8, "Number of rows in systolic array")
#     arrayCols = Param.Unsigned(8, "Number of columns in systolic array")
#     peRowsPerTile = Param.Unsigned(1, "Number of rows in systolic tile")
#     peColsPerTile = Param.Unsigned(4, "Number of columns in systolic tile")

class InstQueue(TickedObject):
    type="InstQueue"
    cxx_header="cpu/minor/AME/inst_queue.hh"
    cxx_class="gem5::InstQueue"
    
    
class AMEInterface(SimObject):
    type="AMEInterface"
    cxx_header="cpu/minor/AME/ame_interface.hh"
    cxx_class="gem5::AMEInterface"

    #systolicArrayCore是Param一个成员的名字，第一个Param.SystolicArrayCore是产生该object成员的指针，第二个SystolicArrayCore是该成员构造的实体。。。
    #上面的规则好像只针对object类型，因为gem5里不允许随意复制object，所以上层一般是复制指针
    systolicArrayCore = Param.SystolicArrayCore(
        SystolicArrayCore(), "Systolic array hardware core"
    )
    instQueueSize_=Param.Unsigned(1, "Instruction queue depth")
    inst_queue=Param.InstQueue(InstQueue(),"InstQueue")
    
    #AME访存扩展
    system = Param.System(Parent.any, "system object")
    ame_mmu=Param.AMEMemUnit(AMEMemUnit(),"AME MMU")
    mem_port = RequestPort("AME memory port")
