#include "cpu/minor/AME/ame_interface.hh"
#include "base/trace.hh"
#include "cpu/minor/AME/inst_queue.hh"
#include "cpu/minor/AME/req_state.hh"
#include "debug/AMEInterface.hh"
#include "debug/AMEMMU.hh"
#include "cpu/minor/exec_context.hh"
#include "debug/MMU.hh"
#include "enums/OpClass.hh"
#include "mem/port.hh"
#include <cstdint>
#include <iterator>
#include "cpu/minor/AME/packet.hh"

namespace gem5 {
// namespace minor {

namespace
{

bool
isAMEIssueSupported(const minor::MinorDynInstPtr &inst)
{
    return inst && inst->isInst() &&
        (inst->staticInst->opClass() == enums::SystolicMMA ||
         inst->staticInst->isMemRef());
}

uint8_t
buildRdValid(const minor::MinorDynInstPtr &inst)
{
    if (!inst || !inst->isInst()) {
        return 0;
    }

    const uint8_t num_src_regs = inst->staticInst->numSrcRegs();
    uint8_t rd_valid = 0;
    if (num_src_regs >= 1) {
        rd_valid |= 0x1;
    }
    if (num_src_regs >= 2) {
        rd_valid |= 0x2;
    }
    return rd_valid;
}

bool
buildWbValid(const minor::MinorDynInstPtr &inst)
{
    return inst && inst->isInst() && inst->staticInst->numDestRegs();
}

} // aicb namespace

AMEInterface::AMEInterface(const AMEInterfaceParams &params)
    : SimObject(params),
      systolicArrayCore(params.systolicArrayCore),
      inst_queue(params.inst_queue),
      instQueueSize_(params.instQueueSize_),
      //访存初始化
      amemem_port(name() + ".mem_port", this, 1),
      uniqueReqId(0),
      amemmu(params.ame_mmu),
      AMECacheMasterId(
          params.system->getRequestorId(this, name() + ".AME_cache"))
{
    for (uint8_t i = 0; i < 1; ++i) {
        AMERegMasterIds.push_back(params.system->getRequestorId(
            this, name() + ".AME_reg" + std::to_string(i)));
        AMERegPorts.push_back(AMERegPort(name() + ".AME_reg_port", this, i));
    }
}

void
AMEInterface::sendInst(minor::MinorDynInstPtr &inst, ExecContextPtr &xc,
    std::function<void()> dependencie_callback)
{
    if ((inst_queue->Instruction_Queue.size() == 0) &&
        (inst_queue->Memory_Queue.size() == 0)) {
        inst_queue->startTicking(*this);
        // DPRINTF(AMEMMU,"just a test0\n");
    }

    if (inst->staticInst->opClass() == enums::SystolicMMA) {
        DPRINTF(AMEInterface, "Sending a new command to the AME~ %s\n", inst);

        // dependencie_callback(); //虽然mma指令还没有完成，但是提前执行回调函数，因为读写的都是矩阵寄存器，普通的指令无需阻塞

        //然后将该指令插入队列，下个周期就能开始执行了
        inst_queue->Instruction_Queue.push_back(
            new InstQueue::QueueEntry(inst, xc, dependencie_callback));
    } else if (inst->staticInst->isMemRef()) {
        DPRINTF(AMEInterface, "Sending a new command to the AME~ %s\n", inst);
        inst_queue->Memory_Queue.push_back(
            new InstQueue::QueueEntry(inst, xc, dependencie_callback));
        auto currInst_ = dynamic_cast<gem5::RiscvISA::MleMicroInst*>(
            inst->staticInst.get());
        if (currInst_) {
            DPRINTF(AMEInterface,
                "MleMicroInst params: eew=%u, mtilek=%u, tr=%u, widen=%u\n",
                currInst_->eew, currInst_->mtilek, currInst_->tr,
                currInst_->widen);
        }
    } else {
        panic("Invalid Matrix Instruction, insn=%s\n", inst);
    }
}

bool
AMEInterface::requestGrant(minor::MinorDynInstPtr &inst)
{
    return (inst_queue->Instruction_Queue.size() +
        inst_queue->Memory_Queue.size()) < instQueueSize_;
}

AMEInterface::AICBIssueResp
AMEInterface::issue_aicb(bool valid, uint64_t hartId,
    minor::MinorDynInstPtr &inst, uint64_t instId, ExecContextPtr &xc,
    std::function<void()> dependencie_callback)
{
    AICBIssueResp resp;
    const bool supported = isAMEIssueSupported(inst);

    resp.rdValid = buildRdValid(inst);
    resp.wbValid = buildWbValid(inst);
    resp.ready = supported && requestGrant(inst);
    resp.accept = valid && resp.ready;

    DPRINTF(AMEInterface,
        "issue_aicb: valid=%d hartId=%llu instId=%llu ready=%d "
        "accept=%d rdValid=%u wbValid=%d inst=%s\n",
        valid, static_cast<unsigned long long>(hartId),
        static_cast<unsigned long long>(instId), resp.ready,
        resp.accept, static_cast<unsigned>(resp.rdValid), resp.wbValid,
        (inst && inst->isInst()) ? inst->staticInst->getName() : "<null>");

    if (resp.accept) {
        sendInst(inst, xc, dependencie_callback);
    }

    return resp;
}

void
AMEInterface::issue(InstQueue::QueueEntry &inst)
{
    //issue的核心目的其实就是把Instruction_Queue里的指令发送给systolicArrayCore
    inst.issued = true;
    // DPRINTF(AMEMMU,"just a test3\n");
    if (inst.inst->isMemRef()) {
        DPRINTF(AMEMMU, "Sending instruction %s to VMU\n",
            inst.inst->staticInst->getName());
        amemmu->issue(*this, inst);
    } else if (
        (inst.inst->staticInst->opClass() == enums::SystolicMMA) ||
        (inst.inst->staticInst->opClass() == enums::SystolicMMA)) {
        systolicArrayCore->acceptInstruction(inst.inst, inst.xc);
    } else {
        panic("Invalid Vector Instruction\n");
    }
}

bool
AMEInterface::isbussy()
{
    return !systolicArrayCore->isIdle() ||
        inst_queue->occupied ||
        !inst_queue->Instruction_Queue.empty() ||
        !inst_queue->Memory_Queue.empty() ||
        amemmu->isOccupied();
}

Port &
AMEInterface::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "mem_port") {
        return amemem_port;
    }

    return SimObject::getPort(if_name, idx);
}

//访存函数
AMEInterface::AMEMemPort::AMEMemPort(const std::string& name, AMEInterface* owner,
    uint8_t channels): RequestPort(name,owner),owner(owner)
{
        for (uint8_t i=0; i<channels; ++i) {
                    laCachePktQs.push_back(std::deque<PacketPtr>());
        }
}
        
AMEInterface::AMEMemPort::~AMEMemPort() {}

AMEInterface::AMEMemPort::Tlb_Translation::Tlb_Translation(AMEInterface *owner)
    :event(this, true), owner(owner)
{
            
}

AMEInterface::AMEMemPort::Tlb_Translation::~Tlb_Translation(){ }

void
AMEInterface::AMEMemPort::Tlb_Translation::finish(const Fault &_fault,
    const RequestPtr &_req, ThreadContext *_tc, BaseMMU::Mode _mode)
{
    fault = _fault;
}

void AMEInterface::AMEMemPort::Tlb_Translation::finish(const Fault _fault,
    uint64_t latency)
{
        fault = _fault;
}

void AMEInterface::AMEMemPort::Tlb_Translation::markDelayed()
{
    panic("Tlb_Translation::markDelayed not implemented");
}

std::string AMEInterface::AMEMemPort::Tlb_Translation::name()
{
    return "Tlb_Translation";
}

void AMEInterface::AMEMemPort::Tlb_Translation::translated()
{

}

bool AMEInterface::AMEMemPort::startTranslation(Addr addr, uint8_t *data,
    uint64_t size, BaseMMU::Mode mode, ThreadContext *tc, uint64_t req_id,
    uint8_t channel)
{
    Process * p = tc->getProcessPtr();
    Addr page1 = p->pTable->pageAlign(addr);
    Addr page2 = p->pTable->pageAlign(addr+size-1);
    assert(page1 == page2);

    //NOTE: need to make a buffer for reads so cache can write to it!
    uint8_t *ndata = new uint8_t[size];
    if (data != nullptr) {
        assert(mode == BaseMMU::Write);
        memcpy(ndata, data, size);
    } else {
      //put a pattern here for debugging
      memset(ndata, 'Z', size);
    }
    MemCmd cmd = (mode==BaseMMU::Write) ? MemCmd::WriteReq :
        MemCmd::ReadReq;


    const Addr pc = tc->pcState().instAddr();
    // const Addr pc = 0;
    RequestPtr req = std::make_shared<Request>(addr, size, 0,
        owner->AMECacheMasterId, pc, tc->contextId());

    BaseCPU *cpu = tc->getCpuPtr();

    req->taskId(cpu->taskId());

    //start translation
    Tlb_Translation *translation = new Tlb_Translation(owner);

    tc->getMMUPtr()->translateTiming(req, tc, translation,  mode);

    if (translation->fault == NoFault){
        PacketPtr pkt = new AMEPacket(req, cmd, req_id, channel);
        pkt->dataDynamic(ndata);

        if (!sendTimingReq(pkt)) {
            laCachePktQs[channel].push_back(pkt);
            }

        delete translation;
        return true;
    }else{
        translation->fault->invoke(tc, NULL);
        delete translation;
        return false;
        }
}

bool AMEInterface::AMEMemPort::sendTimingReadReq(Addr addr, uint64_t size,
    ThreadContext *tc, uint64_t req_id, uint8_t channel)
    {
        return startTranslation(addr, nullptr, size, BaseMMU::Read, tc, req_id,channel);
    }

bool AMEInterface::AMEMemPort::sendTimingWriteReq(Addr addr,
    uint8_t *data, uint64_t size, ThreadContext *tc, uint64_t req_id,
    uint8_t channel)
{
    return startTranslation(addr, data, size, BaseMMU::Write, tc, req_id,
        channel);
}

bool AMEInterface::AMEMemPort::recvTimingResp(PacketPtr pkt)
{
    AMEPacketPtr la_pkt = dynamic_cast<AMEPacketPtr>(pkt);
    assert(la_pkt != nullptr);
    owner->recvTimingResp(la_pkt);
    return true;
}

void AMEInterface::AMEMemPort::recvReqRetry()
{
    //TODO: must be a better way to figure out which channel specified the
    //      retry
    for (auto& laCachePktQ : laCachePktQs) {
        //assert(laCachePktQ.size());
        while (laCachePktQ.size() && sendTimingReq(laCachePktQ.front())) {
            // memory system takes ownership of packet
            laCachePktQ.pop_front();
        }
    }
}

AMEInterface::AMERegPort::AMERegPort(const std::string& name,
    AMEInterface* owner, uint64_t channel) :
    RequestPort(name, owner), owner(owner), channel(channel)
{
}

AMEInterface::AMERegPort::~AMERegPort(){ }

bool AMEInterface::AMERegPort::sendTimingReadReq(Addr addr, uint64_t size,
    uint64_t req_id)
{
    //physical addressing
    RequestPtr req = std::make_shared<Request>
        (addr,size,0,owner->AMERegMasterIds[channel]);
    PacketPtr pkt = new AMEPacket(req, MemCmd::ReadReq, req_id);

    //make data for cache to put data into
    uint8_t *ndata = new uint8_t[size];
    memset(ndata, 'Z', size);
    pkt->dataDynamic(ndata);

    if (!sendTimingReq(pkt)) {
        //delete pkt->req;
        delete[] ndata;
        delete pkt;
        return false;
    }
    return true;
}

// memcpy of data, so the caller can delete it when it pleases
bool AMEInterface::AMERegPort::sendTimingWriteReq(Addr addr, uint8_t *data,
    uint64_t size, uint64_t req_id)
{
    //physical addressing
    RequestPtr req = std::make_shared<Request>
        (addr,size,0,owner->AMERegMasterIds[channel]);
    AMEPacketPtr pkt = new AMEPacket(req, MemCmd::WriteReq, req_id);

    //make copy of data here
    uint8_t *ndata = new uint8_t[size];
    memcpy(ndata, data, size);
    pkt->dataDynamic(ndata);

    if (!sendTimingReq(pkt)){
        //delete pkt->req;
        delete[] ndata;
        delete pkt;
        return false;
    }
    return true;
}

bool AMEInterface::AMERegPort::recvTimingResp(PacketPtr pkt)
{
    AMEPacketPtr ame_pkt = dynamic_cast<AMEPacketPtr>(pkt);
    assert(ame_pkt != nullptr);
    owner->recvTimingResp(ame_pkt);
    return true;
}

void
AMEInterface::AMERegPort::recvReqRetry()
{
    //do nothing, caller will manually resend request
}

void
AMEInterface::recvTimingResp(AMEPacketPtr pkt)
{
    //find the associated request in the queue and associate with pkt
    assert(AME_PendingReqQ.size());
    bool found = false;
    for (AME_ReqState * pending : AME_PendingReqQ) {
        if (pending->getReqId() == pkt->reqId){
            pending->setPacket(pkt);
            found = true;
            break;
        }
    }
    assert(found);

    //commit each request in order they were issued
    while (AME_PendingReqQ.size() &&
        AME_PendingReqQ.front()->isMatched())
    {
        AME_ReqState * pending = AME_PendingReqQ.front();
        AME_PendingReqQ.pop_front();
        pending->executeCallback();
        //NOTE: pending deletes packet. packet calls delete[] on the data
        delete pending;
    }
}

bool
AMEInterface::writeAMEMem(Addr addr, uint8_t *data, uint32_t size,
    ThreadContext *tc, uint8_t channel, std::function<void(void)> callback)
{
    uint64_t id = (uniqueReqId++);
    AME_ReqState *pending = new AME_W_ReqState(id, callback);
    AME_PendingReqQ.push_back(pending);
    if (!amemem_port.sendTimingWriteReq(addr, data, size, tc, id, channel)){
        delete AME_PendingReqQ.back();
        AME_PendingReqQ.pop_back();
        return false;
    }
    return true;
}

bool
AMEInterface::writeAMEReg(Addr addr, uint8_t *data,
    uint32_t size, uint8_t channel,
    std::function<void(void)> callback)
{
    //DPRINTF(VectorEngine, "writeVectorReg got %d bytes to write at %#x\n"
    //    ,size, addr);
    uint64_t id = (uniqueReqId++);
    AME_ReqState *pending = new AME_W_ReqState(id, callback);
    AME_PendingReqQ.push_back(pending);
    if (!AMERegPorts[channel].sendTimingWriteReq(addr,data,size,id)) {
        delete AME_PendingReqQ.back();
        AME_PendingReqQ.pop_back();
        return false;
    }
    return true;
}

bool
AMEInterface::readAMEMem(Addr addr, uint32_t size,
    ThreadContext *tc, uint8_t channel,
    std::function<void(uint8_t*,uint8_t)> callback)
{
    uint64_t id = (uniqueReqId++);
    AME_ReqState *pending = new AME_R_ReqState(id, callback);
    AME_PendingReqQ.push_back(pending);
    if (!amemem_port.sendTimingReadReq(addr, size, tc, id, channel)) {
        delete AME_PendingReqQ.back();
        AME_PendingReqQ.pop_back();
        return false;
    }
    return true;
}

bool
AMEInterface::readAMEReg(Addr addr, uint32_t size,
    uint8_t channel,
    std::function<void(uint8_t*,uint8_t)> callback)
{
    uint64_t id = (uniqueReqId++);
    AME_ReqState *pending = new AME_R_ReqState(id,callback);
    AME_PendingReqQ.push_back(pending);
    if (!AMERegPorts[channel].sendTimingReadReq(addr,size,id)) {
        delete AME_PendingReqQ.back();
        AME_PendingReqQ.pop_back();
        return false;
    }
    return true;
}
}
