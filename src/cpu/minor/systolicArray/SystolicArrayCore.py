from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject

# from m5.objects.MinorFU import MinorFU
# from m5.objects.BaseMinorCPU import MinorOpClass, MinorOpClassSet, MinorFU

'''
def minorMakeOpClassSet(op_classes):
    """Make a MinorOpClassSet from a list of OpClass enum value strings"""

    def boxOpClass(op_class):
        return MinorOpClass(opClass=op_class)

    return MinorOpClassSet(opClasses=[boxOpClass(o) for o in op_classes])
'''


# 脉动阵列功能单元，增加了 systolic array 相关的参数
class SystolicArrayCore(SimObject):
    type = "SystolicArrayCore"
    cxx_header = "cpu/minor/systolicArray/systolic_array_core.hh"
    cxx_class = "gem5::SystolicArrayCore"

    # opClasses = minorMakeOpClassSet(["SystolicMMA"])
    opLat = Param.Cycles(1, "latency in cycles")

    # 脉动阵列硬件参数
    arrayRows = Param.Unsigned(8, "Number of rows in systolic array")
    arrayCols = Param.Unsigned(8, "Number of columns in systolic array")
    peRowsPerTile = Param.Unsigned(1, "Number of rows in systolic tile")
    peColsPerTile = Param.Unsigned(4, "Number of columns in systolic tile")
    # 暂时只支持 output_stationary 数据flow type
    # dataflow_type = Param.String("output_stationary",
    #                            "Dataflow type: weight_stationary, output_stationary, or row_stationary")

    # 功能特性(暂时不考虑)
    """
    support_int8 = Param.Bool(True, "Support int8 computations")
    support_fp16 = Param.Bool(False, "Support fp16 computations")
    support_sparsity = Param.Bool(True, "Support weight sparsity")
    support_quantization = Param.Bool(True, "Support quantization")
    """
    # 性能参数(暂时不知道这些怎么使用)
    """
    clock_frequency = Param.Clock("1GHz", "Operating frequency")
    power_model = Param.PowerModel(NULL, "Power model for the array")
    area_model = Param.AreaModel(NULL, "Area model for the array")
    """
