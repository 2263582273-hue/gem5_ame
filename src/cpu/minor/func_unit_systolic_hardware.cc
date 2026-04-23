#include "cpu/minor/func_unit_systolic_hardware.hh"

#include "debug/MinorSystolicArrayFUPipeline.hh"

//#include "cpu/minor/cpu.hh"
#include "cpu/minor/exec_context.hh"
#include "cpu/minor/execute.hh"
#include <memory>

namespace gem5
{
namespace minor
{

// ================================
// SystolicArrayFUPipeline 具体实现
// ================================
SystolicArrayFUPipeline::SystolicArrayFUPipeline(
                            const std::string &name,
                            MinorCPU &cpu,
                            MinorFU &description,
                            ClockedObject &timeSource,
                            Execute &execute) :
    FUPipeline(name, description, timeSource),
    // cpu
    cpu_(cpu),
    // execute, 用于访问执行阶段的信息
    execute_(execute),
    systolicArrayFU_(dynamic_cast<SystolicArrayFU&>(description)),
    instQueueSize_(systolicArrayFU_.instQueueSize_)
{
    // 初始化指令输入队列
    inputInstQueue_ = std::queue<QueuedInst>();
    computing_ = false;
    currInst_ = nullptr;
    /*
    //DPRINTF(MinorSystolicArrayFUPipeline,
        "[MinorSystolicArrayFUPipeline::SystolicArrayFUPipeline]
        Created systolic array FU: %dx%d\n",
        systolicArrayFU_.systolicArrayCore_.arrayRows_,
        systolicArrayFU_.systolicArrayCore_.arrayCols_);

    */
    DPRINTFS(MinorSystolicArrayFUPipeline,
        static_cast<Named *>(this),
        "[MinorSystolicArrayFUPipeline::SystolicArrayFUPipeline] "
        "Created systolic array FU\n");
}

bool SystolicArrayFUPipeline::alreadyPushed()
{
    // 只要队列非满，就可以 push
    if (inputInstQueue_.size() >= instQueueSize_) {
        return true;
    }
    return false;
}

QueuedInst &SystolicArrayFUPipeline::front()
{
    // 确保队列非空
    // assert(!inputInstQueue_.empty());
    // 返回队列前端指令
    if (!inputInstQueue_.empty())
        return inputInstQueue_.front();
    else {
        static QueuedInst bubble = QueuedInst::bubble();
        return bubble;
    }
        // return QueuedInst::bubble();
}

const QueuedInst &SystolicArrayFUPipeline::front() const
{
    //assert(!inputInstQueue_.empty());
    if (!inputInstQueue_.empty())
        return inputInstQueue_.front();
    else {
        static QueuedInst bubble = QueuedInst::bubble();
        return bubble;
    }
}

void SystolicArrayFUPipeline::push(QueuedInst &inst)
{
    // 在指令队列非满的时候，将指令入队
    assert(!alreadyPushed());
    inputInstQueue_.push(inst);
    if (!inst.isBubble()) {
        occupancy++;
        /* If an instruction was pushed into the pipeline, set the delay before
        *  the next instruction can follow */
        if (nextInsertCycle <= timeSource.curCycle()) {
            nextInsertCycle = timeSource.curCycle()
                              + systolicArrayFU_.issueLat;
        }
    }
    /*
     // 调用父类方法处理基础簿记
    FUPipeline::push(inst);

    // 创建计算任务记录实际开始时间
    ActiveComputation comp;
    comp.inst = inst;
    comp.startCycle = timeSource.curCycle();
    comp.completed = false;

    // 脉动阵列开始实际计算
    if (systolicCore->startComputation(inst)) {
        comp.estimatedCycles = systolicCore->getExpectedCycles();
        computationQueue.push(comp);
        DPRINTF(SystolicArray,
            "Started computation at cycle %d, estimated %d cycles\n",
            comp.startCycle, comp.estimatedCycles);
    }
    */
}

void SystolicArrayFUPipeline::pop()
{
    // 从队列中移除指令
    assert(!inputInstQueue_.empty());
    inputInstQueue_.pop();
    occupancy--;
}

bool SystolicArrayFUPipeline::canInsert() const
{
    // 这个主要是在现有指令完成但未提交或者issue_Lat未达到时，阻止新指令入队
    // 暂时可以保持和原来一致的逻辑
    return FUPipeline::canInsert();
}

void SystolicArrayFUPipeline::getExecContext(ExecContext &xc)
{
    execContext_.reset(&xc);
}
void SystolicArrayFUPipeline::advance()
{
    DPRINTFS(MinorSystolicArrayFUPipeline,
        static_cast<Named *>(this),
        "[MinorSystolicArrayFUPipeline::advance]\n");
    // 记录前一个周期是否stalled
    bool was_stalled = stalled;
    /*如果前一个指令计算完了stall为true，但是还未commit（只有commit才会将stall再次置为false）
     *  禁止下一个指令入队（？这里沿用了之前的逻辑，不过有点不是非常理解为什么要这样）
     */
    if (was_stalled && nextInsertCycle != 0) {
        /* Don't count stalled cycles as part of the issue latency */
        ++nextInsertCycle;
    }

    /*todo：这部分需要实现的功能是：
     * 如果stall，则停止脉动阵列计算
     * 否则如果脉动阵列目前没在计算，但是队列非空，则需要将队列前端指令出队，传给脉动阵列，计算开始
     * 否则如果脉动阵列正在计算，但是计算还未完成，则需要继续推进计算
     * 如果刚计算完成，stall为true，而且occupancy--（也就是待完成的指令数减一）
     */

    // 1. 如果当前处于stall状态，暂停脉动阵列计算
    if (stalled) {
        // stall状态下不进行计算推进
        DPRINTFS(MinorSystolicArrayFUPipeline,
            static_cast<Named *>(this),
            "Pipeline stalled, skipping computation advance\n");
        return;
    }

    // 2. 检查脉动阵列是否可以需要推进计算

    if (systolicArrayFU_.systolicArrayCore_.isIdle()
        && !inputInstQueue_.empty()) {
        // 释放之前的 execContext_
        execContext_.reset();
        // 脉动阵列IDLE状态且队列非空，开始新计算
        currInst_ = &inputInstQueue_.front();
        execContext_ = std::make_shared<ExecContext>(
            cpu_,
            *cpu_.threads[currInst_->inst->id.threadId],
            execute_,
            currInst_->inst);
        systolicArrayFU_.systolicArrayCore_.acceptInstruction(
            currInst_->inst, execContext_);
        bool started = true; //systolicArrayFU->startComputation(inst);
        /*
        // 从队列中移除指令
        inputInstQueue_.pop();
        occupancy--;
        */


        // 标记脉动阵列正在工作
        computing_ = true;
        DPRINTFS(MinorSystolicArrayFUPipeline,
            static_cast<Named *>(this),
            "Starting computation for inst [sn:%llu]\n",
            currInst_->inst->id.execSeqNum);
        /*
        if (systolicArrayFU->canAcceptNewInstruction()) {
            // 将指令传递给脉动阵列
            bool started = systolicArrayFU_.startComputation(inst);
            if (started) {
                DPRINTFS(MinorSystolicArrayFUPipeline,
                    static_cast<Named *>(this),
                    "Starting computation for inst [sn:%llu]\n",
                    currInst_->inst->id.execSeqNum);

                // 从队列中移除指令
                inputInstQueue_.pop();
                occupancy--;

                // 标记脉动阵列正在工作
                computing = true;
            } else {
                computing = false;
                DPRINTFS(MinorSystolicArrayFUPipeline,
                    static_cast<Named *>(this),
                    "Failed to start computation for inst [sn:%llu]\n",
                    currInst_->inst->id.execSeqNum);
            }
        }
        */
    } else if (systolicArrayFU_.systolicArrayCore_.isComputing()) {
        // 脉动阵列正在计算，继续推进计算
        computing_ = true;
    } else {
        // 其他情况，保持当前状态
        // computing_ = false;
        DPRINTFS(MinorSystolicArrayFUPipeline,
            static_cast<Named *>(this),
            "No new computation started, current state: %s\n",
            systolicArrayFU_.systolicArrayCore_.isComputing() ?
            "Computing" : "Idle");
    }

    // 3. 推进脉动阵列计算（无论是否开始新计算）
    if (computing_) {
        DPRINTFS(MinorSystolicArrayFUPipeline,
            static_cast<Named *>(this),
            "debug::enter computate and sacore advance\n");
        systolicArrayFU_.systolicArrayCore_.advance();
        DPRINTFS(MinorSystolicArrayFUPipeline,
            static_cast<Named *>(this), "debug::finish sacore advance\n");
        // 4. 检查计算是否完成
        if (systolicArrayFU_.systolicArrayCore_.isOutputValid()) {
            DPRINTFS(MinorSystolicArrayFUPipeline,
                static_cast<Named *>(this), "Computation completed\n");

            // 计算完成，进入stall状态等待commit
            stalled = true;
            computing_ = false;

            // todo：结果保存是否可以由脉动阵列完成？
            // 应该不行，计算结果写回到寄存器应该要在commit阶段完成吧？
            // 现在gem5中涉及寄存器读写的部分是如何完成的呢？commit_inst中调用excute同时就写入了
            // 由于现在已经获得了execContext_，可以直接调用寄存器写入接口
            // 而且scoreboard应该会标记valid前寄存器都不会被其它指令使用
            // 那么其实在这里写入是可以的
            writeToRegister();

            /*
            // 为防止commit卡死，以下步骤在之后进行：
            // 从队列中移除这条指令
            inputInstQueue_.pop();
            occupancy--;
            */


        }
    }
}
void SystolicArrayFUPipeline::writeToRegister()
{
    // 保存结果到寄存器文件
    systolicArrayFU_.systolicArrayCore_.writeBackOutput();
}
void SystolicArrayFUPipeline::reset()
{
    // 重置脉动阵列
    systolicArrayFU_.systolicArrayCore_.reset();
    computing_ = false;
}
void SystolicArrayFUPipeline::computeOpLat(const StaticInstPtr &inst)
{
    systolicArrayFU_.opLat = Cycles(0);
    /*
    const gem5::RiscvISA::MatrixArithMicroInst* matrixMicroInst =
    dynamic_cast<const gem5::RiscvISA::MatrixArithMicroInst*>(inst.get());

    if (matrixMicroInst) {
        Cycles newLat = Cycles(matrixMicroInst->getK());
        description.opLat = newLat;
    } else {
        DPRINTF(SystolicArrayFU, "Systolic Error: unknown Instr\n",
             description.opLat, systolicSize);
    }
    DPRINTF(SystolicArrayFU, "findTiming %d; systolicSize %d\n",
        description.opLat, systolicSize);
    */
}

}// namespace minor
} // namespace gem5
