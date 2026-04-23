#ifndef __CPU_AME_INTERFACE_HH__
#define __CPU_AME_INTERFACE_HH__
#include <numeric>
#include <string>
#include <vector>

#include "arch/generic/tlb.hh"
#include "cpu/translation.hh"
#include "base/types.hh"
#include "mem/request.hh"
#include "cpu/minor/AME/packet.hh"
#include "cpu/exec_context.hh"
#include "cpu/minor/dyn_inst.hh"
#include "mem/port.hh"
#include "sim/sim_object.hh"
#include "cpu/minor/systolicArray/systolic_array_core.hh"
#include "params/AMEInterface.hh"
#include "cpu/minor/func_unit.hh"
#include "cpu/minor/AME/inst_queue.hh"
#include "cpu/minor/AME/req_state.hh"
#include "cpu/minor/AME/MMU/ame_mem_unit.hh"

namespace gem5 {
//思路：ame既然是和cpu解耦的，那么应该有一套自己的事件队列产生和维护机制，在minor cpu担任这一角色的是pipeline
//目前让InstQueue* inst_queue担任这一角色吧

// namespace minor { 报错1: cxx_class="gem5::AMEInterface",所以就不能是minor域内定义
    class AMEInterface : public SimObject {

public:
    class AMEMemPort : public RequestPort
    {
        public:
        AMEMemPort(const std::string& name, AMEInterface* owner,
            uint8_t channels);
        ~AMEMemPort();
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;
        bool startTranslation(Addr addr, uint8_t *data, uint64_t size,
            BaseMMU::Mode mode, ThreadContext *tc, uint64_t req_id,
            uint8_t channel);
        bool sendTimingReadReq(Addr addr, uint64_t size, ThreadContext *tc,
            uint64_t req_id, uint8_t channel);
        bool sendTimingWriteReq(Addr addr, uint8_t *data, uint64_t size,
            ThreadContext *tc, uint64_t req_id, uint8_t channel);

        std::vector< std::deque<PacketPtr> > laCachePktQs;
        AMEInterface *owner;
        class Tlb_Translation : public BaseMMU::Translation
        {
          public:
            Tlb_Translation(AMEInterface *owner);
            ~Tlb_Translation();

            void markDelayed() override;
            /** TLB interace */
            void finish(const Fault &_fault,const RequestPtr &_req,
                ThreadContext *_tc, BaseMMU::Mode _mode) ;

            void finish(const Fault _fault, uint64_t latency);
            std::string name();
          private:
            void translated();
            EventWrapper<Tlb_Translation,&Tlb_Translation::translated> event;
            AMEInterface *owner;
          public:
            Fault fault;
        };

    };
    
    class AMERegPort : public RequestPort
    {
    public:
        AMERegPort(const std::string& name, AMEInterface* owner,
            uint64_t channel);
        ~AMERegPort();

        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;

        bool sendTimingReadReq(Addr addr, uint64_t size,
            uint64_t req_id);
        bool sendTimingWriteReq(Addr addr, uint8_t *data, uint64_t size,
            uint64_t req_id);

        AMEInterface *owner;
        const uint64_t channel;
    };

    AMEInterface(const AMEInterfaceParams &param); //这里的param是在AMEInterface.py里赋值的
    ~AMEInterface()= default; //报错2，= default

    
    /**
    * requestGrant function is used by the scalar to ask for permision to send
    * a new vector instruction to the vector engine.
    */
    bool requestGrant(minor::MinorDynInstPtr &inst);
    

    void sendInst(minor::MinorDynInstPtr &inst, ExecContextPtr &xc, std::function<void()> dependencie_callback);
    bool isbussy();
    void issue(InstQueue::QueueEntry &inst);
    const unsigned int instQueueSize_;

    //访存函数
    void recvTimingResp(AMEPacketPtr pkt);

    bool writeAMEReg(Addr addr, uint8_t *data, uint32_t size,
        uint8_t channel,
        std::function<void(void)> callback);
    bool readAMEReg(Addr addr, uint32_t size,
        uint8_t channel,
        std::function<void(uint8_t*,uint8_t)> callback);

    bool writeAMEMem(Addr addr, uint8_t *data, uint32_t size,
        ThreadContext *tc, uint8_t channel,
        std::function<void(void)> callback);
    bool readAMEMem(Addr addr, uint32_t size, ThreadContext *tc,
        uint8_t channel,
        std::function<void(uint8_t*,uint8_t)> callback);

    Port &getPort(const std::string &if_name,
        PortID idx = InvalidPortID) override;


public:
    SystolicArrayCore* systolicArrayCore;
    InstQueue* inst_queue;
    //used to identify ports uniquely to whole memory system
    RequestorID AMECacheMasterId;
    AMEMemPort amemem_port;
    //used to identify ports uniquely to whole memory system
    std::vector<RequestorID> AMERegMasterIds;
    std::vector<AMERegPort> AMERegPorts;
    //fields for keeping track of outstanding requests for reordering
    uint64_t uniqueReqId;
    std::deque<AME_ReqState *> AME_PendingReqQ;
    AMEMemUnit* amemmu;

};

}


// }

#endif
