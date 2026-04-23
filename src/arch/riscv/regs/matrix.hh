// Matrix Extension
#ifndef __ARCH_RISCV_REGS_MATRIX_HH__
#define __ARCH_RISCV_REGS_MATRIX_HH__

#include <cstdint>
#include <string>
#include <vector>

#include "arch/generic/mat_reg.hh"
#include "arch/riscv/types.hh"
#include "base/bitunion.hh"
#include "cpu/reg_class.hh"
#include "debug/AME.hh"
#include "debug/MatRegs.hh"

namespace gem5
{

namespace RiscvISA
{

using MatRegContainer = gem5::MatRegContainer<MaxMatLenInBytes, MaxMatRowLenInBytes>;
using mreg_t = MatRegContainer;

const int NumMatTileRegs = 8;
const int NumMatAccRegs = 8;
const int NumMatRegs = NumMatTileRegs + NumMatAccRegs;

const std::vector<std::string> MatRegNames = {
    // Tile registers
    "tr0", "tr1", "tr2", "tr3", "tr4", "tr5", "tr6", "tr7",
    // Accumulation registers
    "acc0", "acc1", "acc2", "acc3", "acc4", "acc5", "acc6", "acc7"
};

static inline TypedRegClassOps<RiscvISA::MatRegContainer> matRegClassOps;

inline constexpr RegClass matRegClass =
    RegClass(MatRegClass, MatRegClassName, NumMatRegs, debug::MatRegs).
        ops(matRegClassOps).
        regType<MatRegContainer>();

BitUnion64(MTYPE)
    Bitfield<63>    mill;      // Illegal value if set
    Bitfield<62,17> reserved;  // Must be zero
    Bitfield<16>    mma;
    Bitfield<15>    mba;       // Matrix out‐of‐bound agnostic
    Bitfield<14>    mfp64;     // Enable FP64
    Bitfield<13,12> mfp32;     // Enable FP32 / TF32
    Bitfield<11,10> mfp16;     // Enable FP16 / BF16
    Bitfield<9,8>   mfp8;      // Enable FP8
    Bitfield<7>     mint64;    // Enable INT64
    Bitfield<6>     mint32;    // Enable INT32
    Bitfield<5>     mint16;    // Enable INT16
    Bitfield<4>     mint8;     // Enable INT8
    Bitfield<3>     mint4;     // Enable INT4
    Bitfield<2,0>   msew;      // Selected element width (SEW)
EndBitUnion(MTYPE)

}   // namespace RiscvISA
}   // namespace gem5

#endif // __ARCH_RISCV_REGS_MATRIX_HH__
