#include "cpu/minor/AME/MMU/ame_mem_unit.hh"

#include <cassert>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "arch/riscv/insts/matrix.hh"
#include "debug/AMEMMU.hh"
#include "mem/translating_port_proxy.hh"
#include "params/AMEMemUnit.hh"

namespace gem5
{

AMEMemUnit::AMEMemUnit(const AMEMemUnitParams *p)
    : SimObject(*p),
      
      memReader(p->memReader),
      memWriter(p->memWriter),
      amewrapper(nullptr),
      currentMemInst(nullptr)
{
}

AMEMemUnit::~AMEMemUnit()
{
}

bool
AMEMemUnit::isOccupied()
{
   return (memReader->occupied||memWriter->occupied);
}

void
AMEMemUnit::finishCurrentMemInst()
{
    assert(currentMemInst != nullptr);
    currentMemInst->completed = true;
    currentMemInst = nullptr;
}

InstQueue::QueueEntry *
AMEMemUnit::getCurrentMemInst() const
{
    return currentMemInst;
}

void
AMEMemUnit::issue(AMEInterface& vector_wrapper, InstQueue::QueueEntry &inst)
{
    
    amewrapper = &vector_wrapper;
    assert(currentMemInst == nullptr);
    currentMemInst = &inst;
    
    auto *static_inst = inst.inst->staticInst.get();
    auto *curinst = dynamic_cast<gem5::RiscvISA::MatrixMicroInst*>(
        static_inst);
    const std::string inst_name = static_inst->getName();
    panic_if(!curinst, "AMEMemUnit got non-matrix micro instruction: %s",
             inst_name.c_str());

    //rs1和rs2都是整数寄存器的编号，这两个寄存器分别存储起始的内存地址和每行的字节数
    //md对应矩阵寄存器的编号
    RegVal base = inst.xc->getRegOperand(static_inst, 0);
    //这里的stride，是矩阵寄存器一行的字节数，同时因为一条微指令load/store的内容就是一行矩阵寄存器，因此stride也是总共搬运的字节数
    RegVal stride = inst.xc->getRegOperand(static_inst, 1);
    Addr ea = base + stride * curinst->microIdx;
    const RegId &mem_reg = static_inst->isLoad() ?
        static_inst->destRegIdx(0) : static_inst->srcRegIdx(2);

    uint8_t eew=curinst->eew;
    uint8_t DST_SIZE;
    switch (eew) {
        case 0x0:DST_SIZE=1; break;
        case 0x1:DST_SIZE=2; break;
        case 0x2:DST_SIZE=4; break;
        case 0x3:DST_SIZE=8; break;
        default: panic("not supported width specified in insn");
    
    }

    DPRINTF(AMEMMU,
        "AMEMemUnit issue %s: microIdx=%u, mlen=%u, mrlen=%u, "
        "mtilem=%u, mtilek=%u, mtilen=%u, base=%#llx, stride=%#llx, "
        "ea=%#llx, reg=%s[%u], ls=%u, eew=%u, tr=%u\n",
        inst_name.c_str(), curinst->microIdx, curinst->mlen,
        curinst->mrlen, curinst->mtilem, curinst->mtilek, curinst->mtilen,
        static_cast<unsigned long long>(base),
        static_cast<unsigned long long>(stride),
        static_cast<unsigned long long>(ea),  mem_reg.className(),
        static_cast<unsigned>(mem_reg.index()),
        static_cast<unsigned>(curinst->ls), static_cast<unsigned>(curinst->eew),
        static_cast<unsigned>(curinst->tr));

    if (static_inst->isLoad()) {
        memReader->initialize(amewrapper, ea, stride, DST_SIZE,
             mem_reg, Location::mem, inst.xc);
    } else {
        memWriter->initialize(amewrapper, ea, stride, DST_SIZE,
             mem_reg, Location::mem, inst.xc);
    }
}
AMEMemUnit *
AMEMemUnitParams::create() const
{
    return new AMEMemUnit(this);
}

} // namespace gem5
