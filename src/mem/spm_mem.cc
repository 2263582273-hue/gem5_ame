#include "mem/spm_mem.hh"

#include "base/random.hh"
#include "base/statistics.hh"
#include "debug/Drain.hh"
#include "sim/system.hh"
#include "mem/abstract_mem.hh"
#include <algorithm>
namespace gem5 {
namespace memory {
using namespace gem5;
using namespace gem5::memory;
ScratchpadMemory::ScratchpadMemory(const ::gem5::ScratchpadMemoryParams* p)
    : SimpleMemory(*p),                 // 传引用给基类
      latency_write(p->latency_write),
      latency_write_var(p->latency_write_var),
      energy_read(p->energy_read),
      energy_write(p->energy_write),
      energy_overhead(p->energy_overhead)
{}

Tick
ScratchpadMemory::getWriteLatency() const
{
    return latency_write +
           ((latency_write_var!=0) ?latency_write_var : 0);
}


void ScratchpadMemory::init()
{
    SimpleMemory::init();

    // 取本内存的物理地址范围
    const AddrRange r = static_cast<AbstractMemory&>(*this).getAddrRange();

    MemBackdoorReq req(r,MemBackdoor::Readable);
    MemBackdoorPtr bd;
    this->recvMemBackdoorReq(req, bd);

    if (bd && bd->ptr()) { // 某些分支没有 isValid()/valid()，用 ptr() 判也可
        const uint8_t* p = static_cast<const uint8_t*>(bd->ptr());

        const AddrRange& br = bd->range();
        // 兼容地求长度：优先用 size()；没有就用 end()-start()
        Addr len = 0;
        // 如果你的 AddrRange 有 size()
        len = br.size();
        const Addr n = std::min<Addr>(len, 16);
        DPRINTF(MinorMem, "SPM backing[0:%lu]:", (unsigned long)n);
        for (Addr i = 0; i < n; ++i) DPRINTF(MinorMem, " %02x", p[i]);
        DPRINTF(MinorMem, "\n");

        // 需要后续直接访问时：
        // 保存基址与范围，计算偏移：off = (guest_addr - br.start())
        // 并在 memcpy 前后断言 off+len <= (br.end()-br.start())
    } else {
        DPRINTF(MinorMem, "No backdoor available yet.\n");
    }
}
}
// create() 要在 gem5 命名空间里实现，且带 const
gem5::memory::ScratchpadMemory* gem5::ScratchpadMemoryParams::create() const
{
    return new gem5::memory::ScratchpadMemory(this);
}
} // namespace gem5