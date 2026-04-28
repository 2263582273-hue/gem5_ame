#include "cpu/minor/AME/MMU/write_timing_unit.hh"

#include "base/cprintf.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "arch/riscv/insts/matrix.hh"
#include "arch/riscv/regs/matrix.hh"
#include "cpu/minor/AME/ame_interface.hh"
#include "cpu/minor/AME/defines.hh"
#include "debug/AMEMMU.hh"
#include "mem/cache/queue_entry.hh"
#include "params/MemUnitWriteTiming.hh"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

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

template <typename ElemT>
void
queueStoreRowFromMatReg(MemUnitWriteTiming *writer,
    RiscvISA::MatRegContainer *src_reg, uint32_t row_idx,
    uint64_t num_elems, uint64_t dst_size)
{
    auto ms = (*src_reg).as<ElemT>();
    for (uint64_t col_idx = 0; col_idx < num_elems; ++col_idx) {
        ElemT value = ms[row_idx][col_idx];
        uint8_t *ndata = new uint8_t[dst_size];
        memcpy(ndata, &value, dst_size);
        writer->queueData(ndata);
    }
}

template <typename ElemT>
std::string
formatMatRegStoreRow(RiscvISA::MatRegContainer *src_reg, uint32_t row_idx,
    uint64_t start_col_idx, uint64_t num_elems)
{
    auto ms = (*src_reg).as<ElemT>();
    std::ostringstream os;
    os << "[";
    for (uint64_t col_idx = 0; col_idx < num_elems; ++col_idx) {
        if (col_idx != 0) {
            os << ' ';
        }
        os << "0x";
        ccprintf(os, "%0*llx", static_cast<int>(sizeof(ElemT) * 2),
            static_cast<unsigned long long>(
                ms[row_idx][start_col_idx + col_idx]));
    }
    os << "]";
    return os.str();
}

} // anonymous namespace

MemUnitWriteTiming::MemUnitWriteTiming(const MemUnitWriteTimingParams *p)
    : TickedObject(*p),
      channel(p->channel),
      cacheLineSize(p->cacheLineSize),
      regLineSize(p->VRF_LineSize),
      done(false),
      numcount(0),
      occupied(false),
      amewrapper(nullptr)
{
}

MemUnitWriteTiming::~MemUnitWriteTiming()
{
    clearDataQ();
}

void
MemUnitWriteTiming::evaluate()
{
    if (!done) {
        writeFunction();
    } else {
        stop();
        done = false;
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
    DPRINTF(AMEMMU, "queueStoreData[%lu/%lu]: %s\n",
        dataQ.size(), numcount, formatBytes(data, DST_SIZE));
}

void
MemUnitWriteTiming::clearDataQ()
{
    for (auto *data : dataQ) {
        delete[] data;
    }
    dataQ.clear();
}

void
MemUnitWriteTiming::initialize(AMEInterface* &_ame_wrapper, Addr _ea,
    RegVal _size, uint8_t _DST_SIZE, RegId _regid, Location _data_to,
    ExecContextPtr& _xc)
{
    amewrapper = _ame_wrapper;
    ea = _ea;
    size = _size;
    DST_SIZE = _DST_SIZE;
    regid = _regid;
    data_to = _data_to;
    xc = _xc;
    numcount = size / DST_SIZE;
    writeIndex = 0;

    assert(!running && !done);
    assert(!dataQ.size());
    assert(!occupied);

    auto *current_mem_inst = amewrapper->amemmu->getCurrentMemInst();
    panic_if(!current_mem_inst,
        "MemUnitWriteTiming initialized without a current memory instruction");
    auto *matrix_inst =
        dynamic_cast<gem5::RiscvISA::MatrixMicroInst *>(
            current_mem_inst->inst->staticInst.get());
    panic_if(!matrix_inst,
        "MemUnitWriteTiming got non-matrix instruction on initialization");

    RiscvISA::MatRegContainer src_reg;
    current_mem_inst->xc->getRegOperand(
        current_mem_inst->inst->staticInst.get(), 2, &src_reg);

    const uint32_t row_idx = matrix_inst->microIdx;
    switch (DST_SIZE) {
        case 1:
            queueStoreRowFromMatReg<uint8_t>(
                this, &src_reg, row_idx, numcount, DST_SIZE);
            DPRINTF(AMEMMU, "matReg row[%u] store data: %s\n", row_idx,
                formatMatRegStoreRow<uint8_t>(
                    &src_reg, row_idx, 0, numcount));
            break;
        case 2:
            queueStoreRowFromMatReg<uint16_t>(
                this, &src_reg, row_idx, numcount, DST_SIZE);
            DPRINTF(AMEMMU, "matReg row[%u] store data: %s\n", row_idx,
                formatMatRegStoreRow<uint16_t>(
                    &src_reg, row_idx, 0, numcount));
            break;
        case 4:
            queueStoreRowFromMatReg<uint32_t>(
                this, &src_reg, row_idx, numcount, DST_SIZE);
            DPRINTF(AMEMMU, "matReg row[%u] store data: %s\n", row_idx,
                formatMatRegStoreRow<uint32_t>(
                    &src_reg, row_idx, 0, numcount));
            break;
        case 8:
            queueStoreRowFromMatReg<uint64_t>(
                this, &src_reg, row_idx, numcount, DST_SIZE);
            DPRINTF(AMEMMU, "matReg row[%u] store data: %s\n", row_idx,
                formatMatRegStoreRow<uint64_t>(
                    &src_reg, row_idx, 0, numcount));
            break;
        default:
            panic("Unsupported DST_SIZE in MemUnitWriteTiming: %u",
                DST_SIZE);
    }

    occupied = true;
    if (writeFunction() && done) {
        done = false;
    } else {
        start();
    }
}

bool
MemUnitWriteTiming::writeFunction()
{
    std::vector<uint64_t> line_offsets;
    uint64_t line_size;
    switch (data_to) {
        case Location::mem      : line_size = cacheLineSize; break;
        case Location::matrix_rf: line_size = regLineSize; break;
        default: panic("invalid data to"); break;
    }

    uint64_t i = this->writeIndex;
    uint64_t addr = ea + DST_SIZE * i;
    uint64_t line_addr = addr - (addr % line_size);
    line_offsets.push_back(addr % line_size);
    uint8_t items_in_line = 1;
    for (uint8_t j = 1; j < (line_size / DST_SIZE) && (i + j) < numcount;
         ++j) {
        uint64_t next_addr = ea + DST_SIZE * (i + j);
        uint64_t next_line_addr = next_addr - (next_addr % line_size);
        if (next_line_addr == line_addr) {
            items_in_line++;
            line_offsets.push_back(next_addr % line_size);
        } else {
            break;
        }
    }

    bool success;
    if (data_to == Location::mem) {
        panic_if(dataQ.size() != numcount,
            "MemUnitWriteTiming dataQ size %lu does not match numcount %lu",
            dataQ.size(), numcount);
        const uint64_t write_size =
            line_offsets.back() + DST_SIZE - line_offsets.front();
        uint8_t *data = new uint8_t[write_size];
        for (uint64_t j = 0; j < line_offsets.size(); ++j) {
            memcpy(data + line_offsets[j] - line_offsets.front(),
                dataQ[i + j], DST_SIZE);
        }
        DPRINTF(AMEMMU, "writeAMEMem addr=%#llx size=%lu data=%s\n",
            static_cast<unsigned long long>(line_addr + line_offsets.front()),
            write_size, formatBytes(data, write_size));

        const bool _done = ((i + items_in_line) == this->numcount);
        success = amewrapper->writeAMEMem(
            line_addr + line_offsets.front(), data, write_size,
            xc->tcBase(), channel, [this, _done]()
            {
                DPRINTF(AMEMMU,
                    "calling on_item_store with  'done'=%d\n", _done);
                on_item_store(_done);
            });
        delete[] data;
    } else {
        panic("尚未完工");
    }

    if (success) {
        writeIndex += items_in_line;
        this->done = (writeIndex == numcount);
        return true;
    }
    return false;
}

void
MemUnitWriteTiming::on_item_store(bool _done)
{
    if (_done) {
        auto *current_mem_inst = amewrapper->amemmu->getCurrentMemInst();
        panic_if(!current_mem_inst,
            "MemUnitWriteTiming finished without a current memory instruction");
        auto *matrix_inst =
            dynamic_cast<gem5::RiscvISA::MatrixMicroInst *>(
                current_mem_inst->inst->staticInst.get());
        panic_if(!matrix_inst,
            "MemUnitWriteTiming got non-matrix instruction on completion");

        DPRINTF(AMEMMU, "matReg row[%u] store finished, dataQ size=%lu\n",
            matrix_inst->microIdx, dataQ.size());

        amewrapper->amemmu->finishCurrentMemInst();
        occupied = false;
        clearDataQ();
    }
}

MemUnitWriteTiming *
MemUnitWriteTimingParams::create() const
{
    return new MemUnitWriteTiming(this);
}

} // namespace gem5
