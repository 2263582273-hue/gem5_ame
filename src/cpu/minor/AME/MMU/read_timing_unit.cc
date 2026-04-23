#include "cpu/minor/AME/MMU/read_timing_unit.hh"
#include "base/logging.hh"
#include "base/cprintf.hh"
#include "base/trace.hh"
#include "cpu/minor/AME/ame_interface.hh"
#include "cpu/minor/AME/defines.hh"
#include "debug/AMEMMU.hh"
#include "mem/cache/queue_entry.hh"
#include "params/MemUnitReadTiming.hh"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>

namespace gem5
{

namespace
{

std::string
formatBytes(const uint8_t *data, uint64_t size)
{
    std::ostringstream os;
    os << "0x";
    for (uint64_t i = 0; i < size; ++i) {
        ccprintf(os, "%02x", data[i]);
    }
    return os.str();
}

} // anonymous namespace

MemUnitReadTiming::MemUnitReadTiming(const MemUnitReadTimingParams *p)
    : TickedObject(*p),
      channel(p->channel),
      cacheLineSize(p->cacheLineSize),
      regLineSize(p->VRF_LineSize),
      done(false),
    //   readFunction(nullptr),
      numcount(0),
      occupied(false),
      amewrapper(nullptr)
{
}

MemUnitReadTiming::~MemUnitReadTiming()
{
    clearDataQ();
}

void
MemUnitReadTiming::evaluate()
{
    if (!done) {
        readFunction();
    } else {
        stop();
        done=false;

    }
}

void
MemUnitReadTiming::regStats()
{
    TickedObject::regStats();
    Cache_line_r_req
        .name(name() + ".cacheLineReadRequests")
        .desc("Number of AME cache line read requests");
}

void
MemUnitReadTiming::queueData(uint8_t *data)
{
    dataQ.push_back(data);
    DPRINTF(AMEMMU, "queueData[%lu/%lu]: %s\n",
        dataQ.size(), numcount, formatBytes(data, DST_SIZE));
}

void
MemUnitReadTiming::clearDataQ()
{
    for (auto *data : dataQ) {
        delete[] data;
    }
    dataQ.clear();
}

void
MemUnitReadTiming::initialize(AMEInterface* &_ame_wrapper,Addr _ea,RegVal _size, uint8_t _DST_SIZE,
        RegId _regid,Location _data_from, ExecContextPtr& _xc)
{
    amewrapper=_ame_wrapper; 
    ea =_ea;
    size=_size;
    DST_SIZE=_DST_SIZE;
    regid=_regid;
    data_from=_data_from;
    xc=_xc;
    numcount=size/DST_SIZE; //当前指令总共要读几个元素
    readIndex=0;
    
    assert(!running && !done);
    assert(!dataQ.size());
    assert(!occupied);
    occupied=true;
    if (readFunction()&&done) {
        done=false;
    } else {
        start();
    }



}
bool
MemUnitReadTiming::readFunction(){
    std::vector<uint64_t> line_offsets;
    uint64_t line_size;
        switch (data_from) {
            case Location::mem      : line_size = cacheLineSize; break;
            case Location::matrix_rf: line_size = regLineSize; break;
            default: panic("invalid data from"); break;
        }
    uint64_t i=this->readIndex; 
    //因为不知道：1.虚拟地址ea是不是一定是cache line entry的开头；
    //2.内存的cacheLineSize是否等于矩阵寄存器的mrlenb；
    //所以有可能会在内存中跨行
    uint64_t addr=ea+DST_SIZE*i; //第i个元素的地址
    uint64_t line_addr=addr-(addr%line_size); //第i个元素的行地址
    line_offsets.push_back(addr%line_size);
    uint8_t items_in_line=1;
    //try to read more items in the same cache-line
    for (uint8_t j=1; j<(line_size/DST_SIZE) && (i+j)<numcount; ++j) {
        uint64_t next_addr=ea+DST_SIZE*(i+j);
        uint64_t next_line_addr = next_addr - (next_addr % line_size);
        if (next_line_addr == line_addr) {
            items_in_line++;
            line_offsets.push_back(next_addr % line_size);
        }
        else {
            break;
        }

    }
    
    //下面要开始读取内存(或者矩阵寄存器)中的一行，已经有了该行的地址line_addr，在该行中的元素数量items_in_line以及各个元素的行偏置line_offsets
    bool success;
    if (data_from==Location::mem){
        success=amewrapper->readAMEMem(line_addr, line_size, xc->tcBase(),  channel, [line_offsets,i,this]
            (uint8_t*data, uint8_t size)
        {

            for (uint64_t j=0; j< line_offsets.size(); ++j) {
                bool _done = ( (i+j+1) == this->numcount );
                uint8_t *ndata = new uint8_t[this->DST_SIZE];
                memcpy(ndata, data + line_offsets[j], this->DST_SIZE);
                DPRINTF(AMEMMU,
                    "calling on_item_load with  'done'=%d\n", _done);
                on_item_load(ndata, _done);
            }
        });
     } else {
        panic("尚未完工");
     }
     if (success) {
        readIndex+=items_in_line;
        this->done=(readIndex==numcount);
        return true;
     }
     return false;
}

//每读完一个元素执行一次on_item_load
void MemUnitReadTiming::on_item_load(uint8_t* data,bool _done){

    // uint8_t* ndata=new uint8_t[DST_SIZE];
    // memcpy(ndata,data,DST_SIZE);

    
    // dataQ.push_back(ndata);
    queueData(data);
    //如果_done，说明这是最后一个读取的元素了，因此把剩下没读取的元素指令，林
    if (_done) {
        DPRINTF(AMEMMU, "read finish, dataQ size=%lu\n", dataQ.size());
        uint64_t index = 0;
        for (const auto *queued_data : dataQ) {
            DPRINTF(AMEMMU, "dataQ[%lu]: %s\n",
                index, formatBytes(queued_data, DST_SIZE));
            ++index;
        }
        //说明MemUnitReadTiming的任务彻底结束了，把占用清除，把队列清空即可
        amewrapper->amemmu->finishCurrentMemInst();
        occupied=false;
        clearDataQ();



    }

}



MemUnitReadTiming *
MemUnitReadTimingParams::create() const
{
    return new MemUnitReadTiming(this);
}

} // namespace gem5
