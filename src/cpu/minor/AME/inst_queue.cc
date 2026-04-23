#include "cpu/minor/AME/inst_queue.hh"
#include "base/trace.hh"
#include "debug/AMEMMU.hh"
#include "debug/AMExzc.hh"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <queue>
#include "cpu/minor/AME/ame_interface.hh" //只是在头文件里不能循环引用，.cc是独立编译的所以可以
#include "cpu/minor/exec_context.hh"
namespace gem5 {
    InstQueue::InstQueue(const InstQueueParams &param):
    TickedObject(param),
    occupied(false) {

    }

    InstQueue::~InstQueue()
    {
    }

    void InstQueue::startTicking(AMEInterface& ame_wrapper) {
        ameInterface=&ame_wrapper;
        start();
    }

    void InstQueue::stopTicking() {
        stop();
    }

    void InstQueue::evaluate() {

    if ((Instruction_Queue.size()==0)&&(!occupied)&&(Memory_Queue.size()==0)) {
            stopTicking();
            DPRINTF(AMExzc,"AME is stopped since no instruction\n");
            return;
        }

    if ((Instruction_Queue.size()!=0)||(occupied)) {
        if (!occupied)
        {
            QueueEntry& Instruction=*Instruction_Queue.front();
            ameInterface->issue(Instruction);
            occupied=true;
        }

        ameInterface->systolicArrayCore->advance();
        // DPRINTF(AMExzc,"systolicArrayCore is advanced\n");
        if (ameInterface->systolicArrayCore->isOutputValid()) {

            ameInterface->systolicArrayCore->writeBackOutput();
            occupied=false;
            QueueEntry& Instruction=*Instruction_Queue.front(); //这里注意要用引用
            this->print_result(Instruction);
            Instruction.dependencie_callback();

            // ameInterface->systolicArrayCore->reset(); //这个函数会引起segment fault?
            QueueEntry *entry = Instruction_Queue.front(); //entry用来防止内存泄漏？
            delete entry;
            Instruction_Queue.pop_front();

        }
    }
    if (Memory_Queue.size()!=0) {
        // DPRINTF(AMEMMU,"just a test2\n");
        QueueEntry *entry = Memory_Queue.front();

        if (!entry->issued && !ameInterface->amemmu->isOccupied()) {
            entry->issued = true;
            ameInterface->issue(*entry);
        }

        if (entry->completed) {
            this->print_result(*entry);
            entry->dependencie_callback();
            Memory_Queue.pop_front();
            delete entry;
        }
        //amemmu是自身驱动的，因此不用像systolicArrayCore那样执行advance函数

        
    }



    }

    void InstQueue::print_result(QueueEntry &Instruction){
        minor::MinorDynInstPtr inst=Instruction.inst;
            if (inst->traceData) {
                inst->traceData->setWhen(curTick());
                inst->traceData->dump();
            }
    }

}
