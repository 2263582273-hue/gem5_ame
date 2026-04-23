#ifndef __CPU_INST_QUEUE_HH__
#define __CPU_INST_QUEUE_HH__

#include <cmath>
#include <cstdint>
#include <functional>
#include "mem/cache/queue_entry.hh"
#include "cpu/minor/dyn_inst.hh"
#include "sim/ticked_object.hh"
#include "cpu/exec_context.hh"
#include "params/InstQueue.hh"

namespace gem5 {
class AMEInterface;

// class InstQueueParams ; //只是权宜之计

namespace minor {
    class ExecContext;
}
class InstQueue : public TickedObject {

public:
    InstQueue(const InstQueueParams &p);
    ~InstQueue();

protected:
    AMEInterface* ameInterface;

public:
    class QueueEntry {
        public:
        QueueEntry(minor::MinorDynInstPtr &inst, ExecContextPtr & _xc, 
            std::function<void()> dependencie_callback):
            dependencie_callback(dependencie_callback),
            issued(false),
            completed(false),
            inst(inst)
            {
                xc=_xc;
            }
        ~QueueEntry() {}
        minor::MinorDynInstPtr inst;
        std::function<void()> dependencie_callback;
        ExecContextPtr xc;
        bool issued;
        bool completed;
        
        //


    };
    std::deque<QueueEntry *> Instruction_Queue; //mma指令队列
    std::deque<QueueEntry *> Memory_Queue; //访存指令队列
    uint64_t mem_queue_size; //这两个暂时没用到
    uint64_t arith_queue_size;
    //这个occupied是专门用来看systolic是否被占用的
    bool occupied;

    void startTicking(AMEInterface& ame_wrapper);
    void stopTicking();
    void evaluate() override;
    void print_result(QueueEntry &Instruction);
    


    

};

}

#endif //__CPU_INST_QUEUE_HH__
