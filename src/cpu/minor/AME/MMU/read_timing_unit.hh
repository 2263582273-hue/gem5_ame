#ifndef __CPU_MEM_UNIT_READ_TIMING__
#define __CPU_MEM_UNIT_READ_TIMING__

#include <cstdint>
#include <functional>
#include <queue>

// #include "cpu/minor/AME/ame_interface.hh"
#include "base/statistics.hh"
#include "base/types.hh"
#include "cpu/exec_context.hh"
#include "cpu/minor/AME/defines.hh"
#include "params/MemUnitReadTiming.hh"
#include "sim/ticked_object.hh"


namespace gem5 {
class AMEInterface;

class MemUnitReadTiming : public TickedObject
{
public:
    MemUnitReadTiming(const MemUnitReadTimingParams *p);
    ~MemUnitReadTiming();

    // overrides
    void evaluate() override;
    void regStats() override;
    // Queuedata is used for indexed operations
    void queueData(uint8_t *data);
    void initialize(AMEInterface* &_ame_wrapper,Addr _ea,RegVal _size, uint8_t _DST_SIZE,
        RegId _regid,Location _data_from, ExecContextPtr& _xc);
    bool readFunction();
    void on_item_load(uint8_t* data,bool done);
    void clearDataQ();

private:

    const uint8_t channel; //通道
    const uint64_t cacheLineSize; //内存的一行有多少字节
    const uint64_t regLineSize; //矩阵寄存器的一行有多少字节，等于mrlenb
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    volatile bool done;
    // std::function<bool(void)> readFunction;
    //Used by indexed Operations to hold the element index
    std::deque<uint8_t *> dataQ;
    //modified by readFunction closure over time
    uint64_t readIndex; //现在读的是第几个元素
    uint64_t numcount;//总共有多少个元素
    AMEInterface* amewrapper;
public:
    Addr ea;
    RegVal size;
    uint8_t DST_SIZE;
    RegId regid;
    Location data_from;
    ExecContextPtr xc;
    bool occupied;
    // Stat for number of cache lines read requested
    statistics::Scalar Cache_line_r_req;

};
}






#endif //__CPU_MEM_UNIT_READ_TIMING__
