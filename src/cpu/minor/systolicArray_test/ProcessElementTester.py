from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject


class ProcessElementTester(SimObject):
    type = "ProcessElementTester"
    cxx_class = "gem5::ProcessElementTester"
    cxx_header = "cpu/minor/systolicArray_test/pe_test.hh"

    # 可以添加可配置参数
    test_mode = Param.String(
        "comprehensive", "Test mode: basic, saturation, float, comprehensive"
    )
    verbose = Param.Bool(True, "Enable verbose test output")
