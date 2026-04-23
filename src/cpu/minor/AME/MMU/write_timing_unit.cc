#include "cpu/minor/AME/MMU/write_timing_unit.hh"

#include "params/MemUnitWriteTiming.hh"

namespace gem5
{

MemUnitWriteTiming::MemUnitWriteTiming(const MemUnitWriteTimingParams *p)
    : TickedObject(*p),
      channel(p->channel),
      cacheLineSize(p->cacheLineSize),
      VRF_LineSize(p->VRF_LineSize),
      done(true),
      writeFunction(nullptr),
      vecIndex(0),
      occupied(false),
      amewrapper(nullptr)
{
}

MemUnitWriteTiming::~MemUnitWriteTiming()
{
}

void
MemUnitWriteTiming::evaluate()
{
    if (writeFunction && writeFunction()) {
        done = true;
        stop();
    }
}

void
MemUnitWriteTiming::regStats()
{
    TickedObject::regStats();
    Cache_line_w_req
        .name(name() + ".cacheLineWriteRequests")
        .desc("Number of AME cache line write requests");
}

void
MemUnitWriteTiming::queueData(uint8_t *data)
{
    dataQ.push_back(data);
}

void
MemUnitWriteTiming::queueAddrs(uint8_t *data)
{
    AddrsQ.push_back(data);
}

void
MemUnitWriteTiming::initialize(AMEInterface& vector_wrapper, uint64_t count,
    uint64_t DST_SIZE, uint64_t mem_addr, uint8_t mop, uint64_t stride,
    Location data_to, ExecContextPtr& xc,
    std::function<void(bool)> on_item_store)
{
    (void)count;
    (void)DST_SIZE;
    (void)mem_addr;
    (void)mop;
    (void)stride;
    (void)data_to;
    (void)xc;
    (void)on_item_store;

    amewrapper = &vector_wrapper;
    done = false;
    vecIndex = 0;
    writeFunction = [this]() { return true; };
    start();
}

MemUnitWriteTiming *
MemUnitWriteTimingParams::create() const
{
    return new MemUnitWriteTiming(this);
}

} // namespace gem5
