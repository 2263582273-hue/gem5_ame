
#ifndef __SCRATCHPAD_MEMORY_HH__
#define __SCRATCHPAD_MEMORY_HH__

#include <deque>

#include "base/statistics.hh"
#include "mem/simple_mem.hh"
#include "mem/port.hh"
#include "params/SimpleMemory.hh"
#include "params/ScratchpadMemory.hh"
#include "base/types.hh"
#include "mem/packet.hh"
#include "debug/MinorMem.hh"

class System;

/**
 * This definition of scratchpad just adds stats to output
 *
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 */
namespace gem5{
namespace memory{
class ScratchpadMemory : public SimpleMemory
{
  private:
    /**
     * Latency if is a write request. If it is a read request,
     * latency from SimpleMemory is used (see implementation)
     */
    const Tick latency_write;

    /**
     * Fudge factor added to the write latency.
     */
    const Tick latency_write_var;

    /**
     * Fudge factor added to the write latency.
     */
    const double energy_read;

    /**
     * Fudge factor added to the write latency.
     */
    const double energy_write;

    /**
     * Fudge factor added to the write latency.
     */
    const double energy_overhead;

    /*
     * Command energies
    */
  public:

    ScratchpadMemory(const ScratchpadMemoryParams *p);
    void init() override;

  protected:

    Tick getWriteLatency() const;
};
}
}
#endif //__SCRATCHPAD_MEMORY_HH__

