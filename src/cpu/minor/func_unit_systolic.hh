#ifndef __CPU_MINOR_FUNC_UNIT_SYSTOLIC_HH__
#define __CPU_MINOR_FUNC_UNIT_SYSTOLIC_HH__

#include "base/types.hh"
#include "cpu/minor/func_unit.hh"
#include "sim/clocked_object.hh"

namespace gem5 {
namespace minor {

// /**
//  * SystolicPE: 单个脉动阵列处理单元（Processing Element）
//  */
// class SystolicPE
// {
// public:
//     // 寄存器
//     uint64_t weight_reg;      // 权重寄存器（从上方流入）
//     uint64_t input_reg;  // 激活值寄存器（从左方流入）
//     uint64_t accumulator;     // 累加器

//     bool valid_weight;
//     bool valid_input;

//     SystolicPE() : weight_reg(0), input_reg(0), accumulator(0),
//                    valid_weight(false), valid_input(false) {}

//     /**
//      * 在每个 cycle 推进数据：
//      * - 接收新权重（来自上一行）
//      * - 接收新激活（来自左一列）
//      * - 执行 MAC
//      * - 向右、向下转发数据
//      */
//     void advance(uint64_t new_weight, bool valid_w,
//                  uint64_t new_input, bool valid_i);
// };

// /**
//  * SystolicArray: 管理 N x N 的 PE 阵列
//  */
// class SystolicArray
// {
// private:
//     unsigned int size;
//     std::vector<std::vector<SystolicPE>> pes;

//     // 当前执行状态
//     struct ExecState {
//         bool active;
//         Cycles startCycle;
//         unsigned int matrixSize;
//         Cycles totalLatency;
//         Cycles elapsed;
//     } execState;

//     // 输入缓冲区（简化模型）
//     std::queue<uint64_t> weight_column;   // 当前列权重
//     std::queue<uint64_t> input_row;  // 当前行激活值

// public:
//     SystolicArray(unsigned int n);

//     /**
//      * 加载一列权重（用于一列 PE）
//      */
//     void loadWeights(const std::vector<uint64_t> &weights);

//     /**
//      * 加载一行激活值（用于一行 PE）
//      */
//     void loadActivations(const std::vector<uint64_t> &acts);

//     /**
//      * 每 cycle 推进整个阵列
//      * 返回是否仍在执行中
//      */
//     bool advanceCycle();

//     /**
//      * 获取输出（最后一列的累加器值）
//      */
//     std::vector<uint64_t> getOutputs() const;

//     unsigned int getSize() const { return size; }
// };

/**
 * SystolicFUPipeline: 功能单元主类
 */
class SystolicFUPipeline : public FUPipeline
{
private:
    unsigned int systolicSize;
    // SystolicArray array;

    StaticInstPtr currentInst;

public:
    SystolicFUPipeline(const std::string &name, MinorFU &description_,
        ClockedObject &timeSource_, uint64_t systolicSize);

    void computeOpLat(const StaticInstPtr &inst) override;

    virtual std::string name() const override {
        return "systolic";
    }
    // void advance() override;

    // void regStats() override;

};

} // namespace minor
} // namespace gem5

#endif // __CPU_MINOR_FUNC_UNIT_SYSTOLIC_HH__
