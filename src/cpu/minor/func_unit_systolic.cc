#include "cpu/minor/func_unit_systolic.hh"

#include "arch/riscv/insts/matrix.hh"
#include "cpu/static_inst.hh"
#include "debug/MinorExecute.hh"

namespace gem5 {
namespace minor {

// // ========================
// // SystolicPE 实现
// // ========================

// void
// SystolicPE::advance(uint64_t new_weight, bool valid_w,
//                     uint64_t new_input, bool valid_i)
// {
//     // 1. 更新输入
//     if (valid_w) {
//         weight_reg = new_weight;
//         valid_weight = true;
//     }
//     if (valid_i) {
//         input_reg = new_input;
//         valid_input = true;
//     }

//     // 2. 执行 MAC（如果输入有效）
//     if (valid_weight && valid_input) {
//         accumulator += weight_reg * input_reg;
//     }

//     // 注意：数据将在下一 cycle 向右/下传递（由 SystolicArray 控制）
// }

// // ========================
// // SystolicArray 实现
// // ========================

// SystolicArray::SystolicArray(unsigned int n)
//     : size(n), pes(n, std::vector<SystolicPE>(n))
// {
//     fatal_if(n == 0, "SystolicArray size must be > 0");
// }

// void
// SystolicArray::loadWeights(const std::vector<uint64_t> &weights)
// {
//     assert(weights.size() == size);
//     for (auto w : weights)
//         weight_column.push(w);
// }

// void
// SystolicArray::loadActivations(const std::vector<uint64_t> &acts)
// {
//     assert(acts.size() == size);
//     for (auto a : acts)
//         input_row.push(a);
// }

// bool
// SystolicArray::advanceCycle()
// {
//     if (execState.active && execState.elapsed >= execState.totalLatency)
//         return false;  // 已完成

//     // 简化模型：假设权重和激活按需供应
//     // 实际中可从 LSQ 或缓存加载

//     // 临时数据：记录本 cycle 各 PE 的输出
// std::vector<std::vector<uint64_t>>
// next_weight(size, std::vector<uint64_t>(size));
// std::vector<std::vector<uint64_t>>
// next_input(size, std::vector<uint64_t>(size));
// std::vector<std::vector<bool>>
// valid_w(size, std::vector<bool>(size, false));
// std::vector<std::vector<bool>>
// valid_i(size, std::vector<bool>(size, false));

//     // 推进每个 PE
//     for (unsigned i = 0; i < size; i++) {
//         for (unsigned j = 0; j < size; j++) {
//             uint64_t w_in = (i == 0) ?
//                 (weight_column.empty() ? 0 : weight_column.front()) :
//                 pes[i-1][j].weight_reg;
//             uint64_t a_in = (j == 0) ?
//                 (input_row.empty() ? 0 : input_row.front()) :
//                 pes[i][j-1].input_reg;

//             bool valid_w_in = (i == 0) ? !weight_column.empty() : true;
//             bool valid_i_in = (j == 0) ? !input_row.empty() : true;

//             // 保存下一 cycle 输入
//             next_weight[i][j] = w_in;
//             next_input[i][j] = a_in;
//             valid_w[i][j] = valid_w_in;
//             valid_i[i][j] = valid_i_in;
//         }
//     }

//     // 批量更新所有 PE（避免中间状态不一致）
//     for (unsigned i = 0; i < size; i++) {
//         for (unsigned j = 0; j < size; j++) {
//             pes[i][j].advance(next_weight[i][j], valid_w[i][j],
//                               next_input[i][j], valid_i[i][j]);
//         }
//     }

//     // 弹出已使用的输入
//     if (!weight_column.empty()) weight_column.pop();
//     if (!input_row.empty()) input_row.pop();

//     return true;
// }

// std::vector<uint64_t>
// SystolicArray::getOutputs() const
// {
//     std::vector<uint64_t> outputs(size);
//     for (unsigned i = 0; i < size; i++) {
//         outputs[i] = pes[i][size-1].accumulator;
//     }
//     return outputs;
// }

// ========================
// SystolicFUPipeline 实现
// ========================

SystolicFUPipeline::SystolicFUPipeline(const std::string &name,
     MinorFU &description_,
     ClockedObject &timeSource_, uint64_t systolicSize) :
      FUPipeline(name, description_, timeSource_),
      systolicSize(systolicSize),
      currentInst(nullptr)
{
    fatal_if(systolicSize == 0, "SystolicFUPipeline: array_size must be > 0");
}

void
SystolicFUPipeline::computeOpLat(const StaticInstPtr &inst)
{
    const gem5::RiscvISA::MatrixArithMicroInst* matrixMicroInst =
    dynamic_cast<const gem5::RiscvISA::MatrixArithMicroInst*>(inst.get());

    if (matrixMicroInst) {
        Cycles newLat = Cycles(matrixMicroInst->getK());
        description.opLat = newLat;
    } else {
        DPRINTF(MinorExecute, "Systolic Error: unknown Instr\n",
             description.opLat, systolicSize);
    }
    DPRINTF(MinorExecute, "findTiming %d; systolicSize %d\n",
        description.opLat, systolicSize);
}

// void
// SystolicFUPipeline::advance()
// {
//     // 调用基类逻辑（管理插入周期等）
//     FUPipeline::advance();

//     // 检查是否有新指令推入
//     if (alreadyPushed()) {
//         currentInst = getInst();  // 假设接口可用
//         if (currentInst && currentInst->opClass() == enums::SystolicMMA) {
//             int N = currentInst->isMatrixOp() ?
// currentInst->getMatrixSize() : 16;

//             // 初始化执行状态
//             execState.active = true;
//             execState.startCycle = timeSource.curCycle();
//             execState.matrixSize = N;
//             execState.totalLatency = 3 * N - 2;
//             execState.elapsed = 0;

//             // 简化：随机初始化权重和激活（实际应从内存加载）
//             std::vector<uint64_t> weights(N, 1);
//             std::vector<uint64_t> acts(N, 2);
//             array.loadWeights(weights);
//             array.loadActivations(acts);

//             DPRINTF(MinorCPU, "GEMM(%d) started at cycle %d\n", N,
// execState.startCycle);
//         }
//     }

//     // 推进脉动阵列
//     if (execState.active) {
//         array.advanceCycle();
//         execState.elapsed++;

//         if (execState.elapsed >= execState.totalLatency) {
//             auto outputs = array.getOutputs();
//             DPRINTF(MinorCPU, "GEMM(%d) completed. Output[0] = %lu\n",
//                     execState.matrixSize, outputs[0]);

//             // 标记完成（实际中应通知写回阶段）
//             execState.active = false;

//             // 更新统计
//             numGEMM++;
//             totalCycles += execState.totalLatency;
//             totalMACs += execState.matrixSize *
// execState.matrixSize * execState.matrixSize;
//         }
//     }
// }

// void
// SystolicFUPipeline::regStats()
// {
//     FUPipeline::regStats();

//     numGEMM
//         .init(0)
//         .name(name() + ".num_gemm")
//         .desc("Number of GEMM operations executed");

//     totalCycles
//         .init(0)
//         .name(name() + ".total_cycles")
//         .desc("Total execution cycles spent on GEMM");

//     totalMACs
//         .init(0)
//         .name(name() + ".total_macs")
//         .desc("Total number of MAC operations performed");
// }
} // namespace minor
} // namespace gem5
