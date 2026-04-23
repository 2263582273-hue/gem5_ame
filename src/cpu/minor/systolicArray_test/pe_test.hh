#ifndef __CPU_SYSTOLIC_ARRAY_TEST_PE_HH__
#define __CPU_SYSTOLIC_ARRAY_TEST_PE_HH__

#include <string>

#include "params/ProcessElementTester.hh"
#include "sim/sim_object.hh"

namespace gem5
{

// 前向声明，避免循环依赖
class ProcessElement;

class ProcessElementTester : public SimObject
{
public:
    PARAMS(ProcessElementTester);

    ProcessElementTester(const Params &params);
    ~ProcessElementTester();

    void startup() override;

private:
    // 使用指针避免构造顺序问题
    ProcessElement* pe_;
    uint64_t testDataA_;
    uint64_t testDataB_;
    __uint128_t expectedOutput_;
    __uint128_t actualOutput_;
    std::vector<uint64_t> vectorA_;
    std::vector<uint64_t> vectorB_;
    bool dataFinished_;

    // 测试方法
    bool runBasicTests();
    //bool runSaturationTests();
    //bool runFloatTests();
    //bool runWideningTests();

    void logTestResult(const std::string& testName, bool passed,
                      uint64_t expected = 0, uint64_t actual = 0);

    // 辅助方法：等待PE完成计算
    //bool waitForCompletion(int maxCycles = 100);
};

} // namespace gem5

#endif // __CPU_SYSTOLIC_ARRAY_TEST_PE_HH__
