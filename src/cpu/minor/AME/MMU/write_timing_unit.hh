#ifndef __CPU_MEM_UNIT_WRITE_TIMING__
#define __CPU_MEM_UNIT_WRITE_TIMING__

#include <cstdint>
#include <functional>
#include <queue>

#include "base/statistics.hh"
#include "base/types.hh"
// #include "cpu/minor/AME/ame_interface.hh"

#include "cpu/exec_context.hh"
#include "cpu/minor/AME/defines.hh"
#include "params/MemUnitWriteTiming.hh" 
#include "sim/ticked_object.hh"

namespace gem5{
class AMEInterface;

class MemUnitWriteTiming : public TickedObject
{
public:
    MemUnitWriteTiming(const MemUnitWriteTimingParams *p);
    ~MemUnitWriteTiming();

    // overrides
    void evaluate() override;
    void regStats() override;

    void queueData(uint8_t *data);
    void initialize(AMEInterface* &_ame_wrapper, Addr _ea, RegVal _size,
        uint8_t _DST_SIZE, RegId _regid, Location _data_to,
        ExecContextPtr& _xc);
    bool writeFunction();
    void on_item_store(bool done);
    void clearDataQ();

private:
    //set by params
    const uint8_t channel;
    const uint64_t cacheLineSize;
    const uint64_t regLineSize;

    volatile bool done;
    std::deque<uint8_t *> dataQ;

    //modified by writeFunction closure over time
    uint64_t writeIndex;
    uint64_t numcount;
    AMEInterface* amewrapper;

public:
    Addr ea;
    RegVal size;
    uint8_t DST_SIZE;
    RegId regid;
    Location data_to;
    ExecContextPtr xc;
    // Stat for number of cache lines write requested
    bool occupied;
    statistics::Scalar Cache_line_w_req;
};
}

#endif //__CPU_MEM_UNIT_WRITE_TIMING__
