/**
 * @file systolic_array_core.hh
 * @brief 脉动阵列核心类
 * @author ztt
 */
#ifndef __CPU_SYSTOLIC_ARRAY_CORE_HH__
#define __CPU_SYSTOLIC_ARRAY_CORE_HH__

#include <memory>
#include <vector>

#include "arch/riscv/insts/matrix.hh"
#include "arch/riscv/regs/matrix.hh"
#include "base/named.hh"
#include "base/types.hh"
#include "cpu/minor/dyn_inst.hh"
#include "cpu/minor/systolicArray/process_element.hh"
#include "cpu/minor/systolicArray/systolic_matrix.hh"
#include "cpu/minor/systolicArray/systolic_tile.hh"
#include "cpu/minor/systolicArray/types.hh"
#include "params/SystolicArrayCore.hh"
#include "sim/sim_object.hh"


//#include "cpu/minor/exec_context.hh"

namespace gem5
{
namespace minor {
    class ExecContext;  // 前向声明
    class QueuedInst; // 前向声明
}
class SystolicArrayCore : public SimObject
{
public:
    SystolicArrayCore(const SystolicArrayCoreParams &params);
    ~SystolicArrayCore() = default;
public:
    const uint32_t arrayRows_;     // 脉动阵列行数
    const uint32_t arrayCols_;     // 脉动阵列列数
    Cycles peOpDelay_;             // PE操作延迟(实际上的设置在pe中，pe会根据操作类型)
    const uint32_t peRowsPerTile_;     // 脉动阵列Tile的pe行数
    const uint32_t peColsPerTile_;     // 脉动阵列Tile的pe列数
    uint32_t tileRows_;     // 脉动阵列Tile行数
    uint32_t tileCols_;     // 脉动阵列Tile列数

    // 数据矩阵
    std::unique_ptr<HorizontalInputMatrix> horizontalMatrix_;
    std::unique_ptr<VerticalInputMatrix> verticalMatrix_;
    std::unique_ptr<OutputMatrix> outputMatrix_;

    // 输入输入结束标志矩阵
    std::vector<std::vector<bool>> dataFinishedMatrix_;

    // tile阵列（按Tile组织）
    std::vector<std::vector<std::unique_ptr<SystolicTile>>> tileArray_;

    // 执行状态
    bool isConfigured_;
    uint32_t currentCycle_;
    uint32_t totalCycles_;
    uint32_t accRowIndex_;     // 当前累加的行索引
    bool accFinished_;     // 是否完成累加过程

    // 待计算的指令
    //minor::QueuedInst *currInst_;
    minor::MinorDynInstPtr currMinorDynInst_;
    RiscvISA::MatrixArithMmaInst* currInst_;
    // 机器码用来获取位宽之类的指令信息(实际上并没有用到)
    uint64_t machInst_;

    MatDataType srcType_;
    MatDataType destType_;
    bool saturationEnabled_;
    uint8_t wideningFactor_;
    bool isFloat_;
    //bool msat_; // 饱和标志位

    // 输入输出寄存器指针（指向指令中的寄存器）
    RiscvISA::MatRegContainer tmp_s1;
    RiscvISA::MatRegContainer tmp_s2;
    RiscvISA::MatRegContainer* tmp_d0;

    // 实际计算的矩阵大小（可能小于arrayRows_/arrayCols_）
    uint32_t computedM_;
    uint32_t computedN_;
    uint32_t computedK_;
    uint32_t computedTileRows_;     // 实际需要的脉动阵列Tile行数
    uint32_t computedTileCols_;     // 实际需要的脉动阵列Tile列数

    // 输入数据缓存（用于PE加载）
    std::vector<uint64_t> horizontalInputBuffer_;
    std::vector<uint64_t> verticalInputBuffer_;
    uint32_t horizontalInputIndex_;
    uint32_t verticalInputIndex_;
    uint32_t elementsPer64bit_;// for col data

public:
    // === 配置PE接口 ===
    void configurePE(MatDataType srcType, MatDataType destType,
                   bool saturationEnabled, uint8_t wideningFactor);
    // === 与FUPipeline接口 ===
    void acceptInstruction(minor::MinorDynInstPtr &inst, ExecContextPtr &xc);

    // === 执行控制 ===
    // 脉动推进一个周期
    void advance();
    // 重置整个阵列
    void reset();

    // === 结果获取 ===
    std::vector<__uint128_t> getOutputRow(uint32_t row) const;
    std::vector<__uint128_t> getOutputColumn(uint32_t col) const;
    __uint128_t getOutput(uint32_t row, uint32_t col) const;
    void writeBackOutput();


    // === 状态查询 ===
    bool isIdle() const;
    bool isComputing() const;
    bool isMacFinished() const;
    bool isOutputValid() const;

    // === 调试接口 ===
    void printArrayState() const;
    void printDataFlows() const;

private:
    // 初始化PE阵列和Tile
    void initializePEArray();
    // 从指令中确认计算类型的函数
    void computeSrcType(uint8_t fp, uint8_t sn, uint8_t eew);
    void computeDestType(uint8_t fp, uint8_t sn, uint8_t eew, uint8_t widen);
    // === 数据加载接口 ===
    // 从左矩阵寄存器加载横向数据（DataA)到vector，实际上加载某一列，col_index表示第一行加载的列数
        // 后续每一行加载的列数=前一行-1
    void loadHorizontalData(uint32_t col_index);
    // 从右矩阵寄存器加载纵向数据（DataB）到vector，实际上加载某一行，row_index表示第一列加载的行数
        // 后续每一列加载的行数=前一列-1
    void loadVerticalData(uint32_t row_index);
    // 更新数据加载完成标志矩阵
    void updateDataFinishedMatrix();
    // 累加输出矩阵到结果寄存器：取输出矩阵的一行，累加（这里需要实现各种计算）
    void accumulateOutput(uint32_t row_index);
    // 整数计算模板
    template<typename SrcT, typename DestT>
    void computeIntegerACC(uint32_t row_index, bool saturationEnabled);
    // 浮点计算模板
    template<typename SrcT, typename DestT>
    void computeFloatACC(uint32_t row_index);
    // 输出模板
    template<typename T>
    void writeBackInt(uint32_t row, uint32_t col, __uint128_t data);
    template<typename T>
    void writeBackFloat(uint32_t row, uint32_t col, __uint128_t data);
};

} // namespace gem5

#endif // __CPU_SYSTOLIC_ARRAY_CORE_HH__
