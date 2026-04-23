#ifndef __CPU_MEM_UNIT_WRITE_TIMING__
#define __CPU_MEM_UNIT_WRITE_TIMING__

#include <cstdint>
#include <functional>
#include <queue>

#include "base/statistics.hh"
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
    void queueAddrs(uint8_t *data);
    void initialize(AMEInterface& vector_wrapper, uint64_t count,
        uint64_t DST_SIZE,uint64_t mem_addr,uint8_t mop,uint64_t stride,
        Location data_to,ExecContextPtr& xc,
        std::function<void(bool)> on_item_store);

private:
    //set by params
    const uint8_t channel;
    const uint64_t cacheLineSize;
    const uint64_t VRF_LineSize;

    volatile bool done;
    std::deque<uint8_t *> dataQ;
    //Used by indexed Operations to hold the element index
    std::deque<uint8_t *> AddrsQ;
    std::function<bool(void)> writeFunction;

    //modified by writeFunction closure over time
    uint64_t vecIndex;
    AMEInterface* amewrapper;

public:
    // Stat for number of cache lines write requested
    bool occupied;
    statistics::Scalar Cache_line_w_req;
};
}

#endif //__CPU_MEM_UNIT_WRITE_TIMING__
