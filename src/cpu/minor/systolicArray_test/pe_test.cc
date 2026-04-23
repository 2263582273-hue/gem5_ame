#include "cpu/minor/systolicArray_test/pe_test.hh"

#include <cstdint>
#include <iostream>

#include "base/trace.hh"
#include "cpu/minor/systolicArray/process_element.hh"
#include "debug/ProcessElementTester.hh"
#include "sim/sim_exit.hh"

namespace gem5
{

ProcessElementTester::ProcessElementTester(const Params &params)
    : SimObject(params), pe_(nullptr)
{
    // 在构造函数中创建PE实例
    testDataA_ = 0x0;
    testDataB_ = 0x0;
    expectedOutput_ = 0x0;
    actualOutput_ = 0x0;

    // 假设操作延迟为5个周期
    pe_ = new ProcessElement(5, &testDataA_, &testDataB_, &actualOutput_);

    DPRINTF(ProcessElementTester,
        "ProcessElementTester constructor completed\n");
    DPRINTF(ProcessElementTester,
        "PE instance created successfully\n");
}

ProcessElementTester::~ProcessElementTester()
{
    DPRINTF(ProcessElementTester,
        "ProcessElementTester destructor called\n");
    if (pe_) {
        delete pe_;
        DPRINTF(ProcessElementTester, "PE instance destroyed\n");
    }
}

void ProcessElementTester::startup()
{
    DPRINTF(ProcessElementTester,
        "=== ProcessElementTester startup initiated ===\n");

    std::cout <<
        "==========================================" << std::endl;
    std::cout <<
        "Starting Processing Element Comprehensive Test" << std::endl;
    std::cout <<
        "==========================================" << std::endl;

    bool allPassed = true;

    // 分别执行测试并收集结果
    DPRINTF(ProcessElementTester, "Starting basic integer tests\n");
    std::cout << "\n--- Basic Integer Tests ---" << std::endl;
    allPassed &= runBasicTests();
    std::cout << "\n allPassed " << allPassed << std::endl;
    /*
    DPRINTF(ProcessElementTester, "Starting saturation tests\n");
    std::cout << "\n--- Saturation Tests ---" << std::endl;
    allPassed &= runSaturationTests();
    std::cout << "\n allPassed " << allPassed << std::endl;

    DPRINTF(ProcessElementTester, "Starting floating point tests\n");
    std::cout << "\n--- Floating Point Tests ---" << std::endl;
    allPassed &= runFloatTests();
    std::cout << "\n allPassed " << allPassed << std::endl;

    DPRINTF(ProcessElementTester, "Starting widening tests\n");
    std::cout << "\n--- Widening Tests ---" << std::endl;
    allPassed &= runWideningTests();
    std::cout << "\n allPassed " << allPassed << std::endl;
    */
    DPRINTF(ProcessElementTester,
            "All tests completed, overall result: %s\n",
            allPassed ? "PASS" : "FAIL");

    std::cout <<
        "==========================================" << std::endl;
    if (allPassed) {
        std::cout << "✅ ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "❌ SOME TESTS FAILED" << std::endl;
    }
    std::cout <<
        "==========================================" << std::endl;

    // 测试完成后退出仿真
    DPRINTF(ProcessElementTester,
            "Exiting simulation with code: %d\n",
            allPassed ? 0 : 1);
    exitSimLoop("PE test completed", allPassed ? 0 : 1);
}


bool ProcessElementTester::runBasicTests()
{
    // 初始化数据
    vectorA_.clear();
    vectorB_.clear();
    vectorA_.push_back(0x0102030405060708);
    vectorB_.push_back(0x0807060504030201);
    vectorA_.push_back(0x0102030405060708);
    vectorB_.push_back(0x0807060504030201);
    bool stall = false;
    dataFinished_ = false;

    // configure PE
    DPRINTF(ProcessElementTester,
        "Configuring PE for UInt8->UInt8 operation\n");
    pe_->configure(MatDataType::UInt8, MatDataType::UInt8, false, 1);
    bool passed = true;
    // 计算过程
    while (!stall) {
        if (!dataFinished_) {
            testDataA_ = vectorA_.front();
            vectorA_.erase(vectorA_.begin());
            testDataB_ = vectorB_.front();
            vectorB_.erase(vectorB_.begin());
        }
        pe_->advance(dataFinished_);
        dataFinished_ = (vectorA_.empty() && vectorB_.empty());
        if (pe_->isOutputValid()) {
            actualOutput_ = pe_->getOutput();
            expectedOutput_ =
                2*(1*8 + 2*7 + 3*6 + 4*5 + 5*4 + 6*3 + 7*2 + 8*1); // = 240
            passed = (actualOutput_ == expectedOutput_);
            DPRINTF(ProcessElementTester,
                "UInt8->UInt8 Test - Expected: %lu, Actual: %lu\n",
                expectedOutput_, actualOutput_);
            logTestResult("UInt8->UInt8 Dot Product",
                passed, expectedOutput_, actualOutput_);
            stall = true;
        }

    }

    // 清理状态
    pe_->reset();
    DPRINTF(ProcessElementTester, "PE output cleared after basic test\n");
    return passed;
}

/*
bool ProcessElementTester::runSaturationTests()
{
    DPRINTF(ProcessElementTester,
        "Configuring PE for saturation test (Int8->Int16, sat enabled)\n");
    pe_->configure(MatDataType::Int8, MatDataType::Int16, true, 2);

    // 使用会导致饱和的大数
    uint64_t dataA = 0x4040404040404040;  // 64 in each lane
    uint64_t dataB = 0x4040404040404040;

    DPRINTF(ProcessElementTester,
        "Feeding saturation test data A: 0x%016lx\n", dataA);
    DPRINTF(ProcessElementTester,
        "Feeding saturation test data B: 0x%016lx\n", dataB);

    pe_->feedDataA(dataA);
    pe_->feedDataB(dataB);

    bool completed = waitForCompletion(100);
    if (!completed) {
        logTestResult("Int8->Int16 Saturation", false, 0x7FFF, 0);
        return false;
    }

    uint64_t result = pe_->getOutput();
    // 检查是否饱和到int16的最大值
    uint64_t expected = 0x7FFF;  // INT16_MAX

    DPRINTF(ProcessElementTester,
        "Saturation Test - Expected: 0x%016lx, Actual: 0x%016lx\n",
        expected, result);

    bool passed = (result == expected);
    logTestResult("Int8->Int16 Saturation", passed, expected, result);

    pe_->clearOutput();
    DPRINTF(ProcessElementTester,
        "PE output cleared after saturation test\n");
    return passed;
}

bool ProcessElementTester::runFloatTests()
{
    DPRINTF(ProcessElementTester,
        "Configuring PE for Float32->Float32 operation\n");
    pe_->configure(MatDataType::Float32, MatDataType::Float32, false, 1);

    int round = 4;
    std::vector<uint64_t> dataAs(round);
    std::vector<uint64_t> dataBs(round);
    dataAs[0] = 0x3FC000003FC00000; // 1.5
    dataBs[0] = 0x4020000040200000; // 2.5
    dataAs[1] = 0x0000000000000000;
    dataBs[1] = 0x0000000000000000;
    dataAs[2] = 0x0000000000000000;
    dataBs[2] = 0x0000000000000000;
    dataAs[3] = 0x0000000000000000;
    dataBs[3] = 0x0000000000000000;

    int cycles = 0;
    int data_index = 0;
    bool stall = false;
    bool isLastRound = true;

    DPRINTF(ProcessElementTester,
        "Starting floating point data feeding loop\n");

    while (!stall && cycles < 1000){
        if (pe_->canAcceptData()){
            DPRINTF(ProcessElementTester,
                "Cycle %d: Feeding data pair %d\n", cycles, data_index);
            DPRINTF(ProcessElementTester,
                "  Data A: 0x%016lx\n", dataAs[data_index]);
            DPRINTF(ProcessElementTester,
                "  Data B: 0x%016lx\n", dataBs[data_index]);

            pe_->feedDataA(dataAs[data_index]);
            pe_->feedDataB(dataBs[data_index]);
            data_index++;
            cycles++;
            continue;
        }
        if (pe_->isComputing()){
            DPRINTF(ProcessElementTester,
                "Cycle %d: PE is computing, ticking\n", cycles);
            pe_->clockTick(isLastRound);
            cycles++;
        }
        if (pe_->isOutputValid()){
            DPRINTF(ProcessElementTester,
                "Cycle %d: PE output is valid\n", cycles);
            stall = true;
        }
    }

    if (cycles >= 1000) {
        DPRINTF(ProcessElementTester,
            "WARNING: Float test timeout after 1000 cycles\n");
        logTestResult("Float32->Float32 Basic", false, 0x40F00000, 0);
        return false;
    }

    uint64_t result = pe_->getOutput();
    // 标准结果的=1.5*2.5+1.5*2.5=7.5=0x40F00000
    uint64_t expected = 0x0000000040F00000;
    DPRINTF(ProcessElementTester,
        "Float Test - Expected: 0x%016lx, Actual: 0x%016lx\n",
        expected, result);

    bool passed = (result == expected);
    logTestResult("Float32->Float32 Basic", passed, expected, result);

    pe_->clearOutput();
    DPRINTF(ProcessElementTester,
        "PE output cleared after float test\n");
    return passed;
}

bool ProcessElementTester::runWideningTests()
{
    DPRINTF(ProcessElementTester,
        "Configuring PE for widening test (UInt8->UInt32, widening x4)\n");
    pe_->configure(MatDataType::UInt8, MatDataType::UInt32, false, 4);

    uint64_t dataA = 0x0101010101010101;  // 全1
    uint64_t dataB = 0x0202020202020202;  // 全2
    // 预期结果：每个UInt8扩展为UInt32，结果为0x01010101020202020202020202020202
    DPRINTF(ProcessElementTester,
        "Feeding widening test data A: 0x%016lx\n", dataA);
    DPRINTF(ProcessElementTester,
        "Feeding widening test data B: 0x%016lx\n", dataB);

    pe_->feedDataA(dataA);
    pe_->feedDataB(dataB);

    bool completed = waitForCompletion(100);
    if (!completed) {
        logTestResult("Widening x4 Operation", false, 0x10, 0);
        return false;
    }

    uint64_t result = pe_->getOutput();
    uint64_t expected = 0x0000000000000010;  // 基础检查

    DPRINTF(ProcessElementTester,
        "Widening Test - Expected: 0x%016lx, Actual: 0x%016lx\n",
            expected, result);

    bool passed = (result == expected);
    logTestResult("Widening x4 Operation", passed, expected, result);

    pe_->clearOutput();
    DPRINTF(ProcessElementTester,
        "PE output cleared after widening test\n");
    return passed;
}
*/
void ProcessElementTester::logTestResult(const std::string& testName,
    bool passed, uint64_t expected, uint64_t actual)
{
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << testName;
    if (!passed && expected != 0) {
        std::cout <<
            " (Expected: " << expected <<
            ", Got: " << actual << ")";
    }
    std::cout << std::endl;

    DPRINTF(ProcessElementTester, "Test Result: %s - %s\n",
            testName.c_str(), passed ? "PASS" : "FAIL");
    if (!passed) {
        DPRINTF(ProcessElementTester,
            "Expected: 0x%016lx, Actual: 0x%016lx\n", expected, actual);
    }
}

} // namespace gem5
