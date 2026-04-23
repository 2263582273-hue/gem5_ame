#ifndef __CPU_MINOR_FUNC_UNIT_SYSTOLIC_HARDWARE_HH__
#define __CPU_MINOR_FUNC_UNIT_SYSTOLIC_HARDWARE_HH__

#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "sim/clocked_object.hh"

/*
  * todo: MatRegContainer的定义的include
  * Q:到底是RiscvISA::MatRegContainer还是RiscvISAInst::MatRegContainer？
  * 前者需要在arch/riscv/regs/matrix.hh中定义，
  * 后者需要定义RiscvISAInst命名空间，在arch/riscv/isa/includes.isa中定义
  * 或者是include generate的
    race/riscv-ame-gem5/build/RISCV/arch/riscv/generated/中的文件？
  * 后续未用到MatRegContainer，暂时搁置此问题
*/

//#include "arch/riscv/isa/includes.isa"
//#include "arch/riscv/regs/matrix.hh"
#include "cpu/minor/cpu.hh"
#include "cpu/minor/func_unit.hh"

//#include "cpu/minor/execute.hh"
#include "cpu/minor/systolicArray/systolic_array_core.hh"
#include "debug/MinorSystolicArrayFU.hh"
#include "params/SystolicArrayFU.hh"

namespace gem5
{

// =======================================
// SystolicArrayFU 定义
// 这里主要是作为接口，实际核的实现见 SystolicArrayCore
// =======================================
class SystolicArrayFU : public MinorFU
{
    public:
        SystolicArrayFU(const SystolicArrayFUParams &params)
        : MinorFU(params),
          systolicArrayCore_(*params.systolicArrayCore),
          instQueueSize_(params.instQueueSize)
        {
            DPRINTF(MinorSystolicArrayFU,
                "[SystolicArrayFU::SystolicArrayFU] : %s\n", name());
        };
        ~SystolicArrayFU(){};

    public:
        // 创建 SystolicArrayCore 对象
        SystolicArrayCore& systolicArrayCore_;
        // 指令输入队列尺寸
        // 暂时设置为1，因为如果这里可以放置多个指令，那么就需要考虑指令之间的依赖关系，
        const unsigned int instQueueSize_;
};

namespace minor
{

// 前向声明
class Execute;
class ExecContext;
// ============================
// SystolicArrayFUPipeline 定义
// ============================
class SystolicArrayFUPipeline : public FUPipeline
{
protected:
    // cpu
    MinorCPU &cpu_;
    // execute, 用于访问执行阶段的信息
    Execute &execute_;

    SystolicArrayFU &systolicArrayFU_;
    //
    //ExecContext* execContext_;
    ExecContextPtr execContext_;
    /* 指令输入队列 - 只负责保序，不模拟延迟 */
    std::queue<QueuedInst> inputInstQueue_;
    // 当前正在执行的指令
    QueuedInst *currInst_;
    /* 指令输入队列尺寸 */
    const unsigned int instQueueSize_;

public:
    SystolicArrayFUPipeline(const std::string &name,
                            MinorCPU &cpu,
                            MinorFU &description,
                            ClockedObject &timeSource,
                            Execute &execute);
    ~SystolicArrayFUPipeline() = default;
public:
    bool computing_;
    /* 输出结果 */
    // todo: 整个矩阵？寄存器编号？
    // std::vector<RegVal> results;
public:
    // 重写基类关键方法
    bool alreadyPushed() override;
    QueuedInst &front() override;
    const QueuedInst &front() const override;
    void push(QueuedInst &inst) override;
    void advance() override;
    bool canInsert() const override;
    //是纯虚函数，需要在派生类中实现
    void computeOpLat(const StaticInstPtr &inst) override;

    void pop();
public:
    /* 脉动阵列相关成员函数*/
    // 结果保存接口
    void writeToRegister();
    void reset();
    void getExecContext(ExecContext &xc);
};

} // namespace minor
} // namespace gem5
#endif /* __CPU_MINOR_FUNC_UNIT_SYSTOLIC_HARDWARE_HH__ */
