/*
 * Copyright (c) 2012 Google
 * Copyright (c) The University of Virginia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "arch/riscv/decoder.hh"
#include "arch/riscv/isa.hh"
#include "arch/riscv/types.hh"
#include "base/bitfield.hh"
#include "debug/Decode.hh"

namespace gem5
{

namespace RiscvISA
{

Decoder::Decoder(const RiscvDecoderParams &p) : InstDecoder(p, &machInst)
{
    ISA *isa = dynamic_cast<ISA*>(p.isa);
    vlen = isa->getVecLenInBits();
    elen = isa->getVecElemLenInBits();

    // Matrix Extension
    melen = isa->getMatElemLenInBits();
    mlen = isa->getMatLenInBits();
    mrlen = isa->getMatRowLenInBits();
    mamul = isa->getMatAccMul();

    _enableZcd = isa->enableZcd();
    reset();
}

void Decoder::reset()
{
    aligned = true;
    mid = false;
    machInst = 0;
    emi = 0;
    squashed = true;
}

void
Decoder::moreBytes(const PCStateBase &pc, Addr fetchPC)
{
    // The MSB of the upper and lower halves of a machine instruction.
    constexpr size_t max_bit = sizeof(machInst) * 8 - 1;
    constexpr size_t mid_bit = sizeof(machInst) * 4 - 1;

    auto inst = letoh(machInst);
    DPRINTF(Decode, "Requesting bytes 0x%08x from address %#x\n", inst,
            fetchPC);

    bool aligned = pc.instAddr() % sizeof(machInst) == 0;
    if (aligned) {
        emi.instBits = inst;
        if (compressed(inst))
            emi.instBits = bits(inst, mid_bit, 0);
        outOfBytes = !compressed(emi);
        instDone = true;
    } else {
        if (mid) {
            assert(bits(emi.instBits, max_bit, mid_bit + 1) == 0);
            replaceBits(emi.instBits, max_bit, mid_bit + 1, inst);
            mid = false;
            outOfBytes = false;
            instDone = true;
        } else {
            emi.instBits = bits(inst, max_bit, mid_bit + 1);
            mid = !compressed(emi);
            outOfBytes = true;
            instDone = compressed(emi);
        }
    }
}

StaticInstPtr
Decoder::decode(ExtMachInst mach_inst, Addr addr)
{
    DPRINTF(Decode, "Decoding instruction 0x%08x at address %#x\n",
            mach_inst.instBits, addr);

    StaticInstPtr &si = instMap[mach_inst];
    if (!si) {
        si = decodeInst(mach_inst);
    }

    si->size(compressed(mach_inst) ? 2 : 4);

    DPRINTF(Decode, "Decode: Decoded %s instruction: %#x\n",
            si->getName(), mach_inst);
    return si;
}

StaticInstPtr
Decoder::decode(PCStateBase &_next_pc)
{
    if (!instDone)
        return nullptr;
    instDone = false;

    auto &next_pc = _next_pc.as<PCState>();

    if (compressed(emi)) {
        next_pc.npc(next_pc.instAddr() + sizeof(machInst) / 2);
        next_pc.compressed(true);
    } else {
        next_pc.npc(next_pc.instAddr() + sizeof(machInst));
        next_pc.compressed(false);
    }

    if (GEM5_UNLIKELY(squashed ||
        next_pc.new_vconf() ||
        next_pc.new_mconf())) {
        squashed = false;
        next_pc.new_vconf(false);
        next_pc.new_mconf(false);
        vl = next_pc.vl();
        vtype = next_pc.vtype();
        mtype = next_pc.mtype();
        mtilem = next_pc.mtilem();
        mtilen = next_pc.mtilen();
        mtilek = next_pc.mtilek();
    } else {
        next_pc.vl(vl);
        next_pc.vtype(vtype);
        next_pc.mtype(mtype);
        next_pc.mtilem(mtilem);
        next_pc.mtilen(mtilen);
        next_pc.mtilek(mtilek);
    }

    // initial high32 bit
    emi.high32 = 0;

    emi.rv_type = static_cast<int>(next_pc.rvType());
    emi.enable_zcd = _enableZcd;

    emi.vl      = vl;
    emi.vtype8  = vtype & 0xff;
    emi.vill    = vtype.vill;

    // Matrix Extension
    if ((emi & 0x7f) == 0x77) {
        emi.mtype9.msew = mtype & 0x3;
        emi.mtype9.mfp32 = (mtype >> 13) & 0x1;
        emi.mtype9.mfp16 = (mtype >> 11) & 0x1;
        // TODO, fp8,mba
        emi.mtilem = mtilem & 0x3f;
        emi.mtilen = mtilen & 0x3f;
        emi.mtilek = mtilek & 0x3f;
        emi.mill    = mtype.mill;
    }

    return decode(emi, next_pc.instAddr());
}

} // namespace RiscvISA
} // namespace gem5
