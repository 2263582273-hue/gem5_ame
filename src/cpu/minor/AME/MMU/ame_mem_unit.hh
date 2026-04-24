#ifndef __CPU_MEM_UNIT_HH__
#define __CPU_MEM_UNIT_HH__

#include <functional>
#include "cpu/minor/dyn_inst.hh"
#include "cpu/minor/AME/inst_queue.hh"
#include  "cpu/minor/AME/MMU/read_timing_unit.hh"
#include "cpu/minor/AME/MMU/write_timing_unit.hh"
#include "params/AMEMemUnit.hh"

namespace gem5{

class AMEInterface;

class AMEMemUnit : public SimObject
{
  public:
    AMEMemUnit(const AMEMemUnitParams *p);
    ~AMEMemUnit();

    bool isOccupied();
    void issue(AMEInterface& vector_wrapper,InstQueue::QueueEntry &inst);
    void finishCurrentMemInst();
    InstQueue::QueueEntry *getCurrentMemInst() const;

  private:
    
    MemUnitReadTiming * memReader;
    MemUnitWriteTiming * memWriter;

    AMEInterface* amewrapper;
    InstQueue::QueueEntry *currentMemInst;


};
}




#endif // __CPU_MEM_UNIT_HH__
