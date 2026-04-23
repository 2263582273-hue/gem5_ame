// Matrix Extension
#include "arch/riscv/insts/matrix.hh"

#include <sstream>
#include <string>

#include "arch/riscv/insts/static_inst.hh"
#include "arch/riscv/isa.hh"
#include "arch/riscv/regs/misc.hh"
#include "arch/riscv/regs/matrix.hh"
#include "arch/riscv/utility.hh"
#include "cpu/static_inst.hh"

namespace gem5
{
namespace RiscvISA
{
// get Matrix element length(bit)
uint32_t
getMelen(uint64_t width, uint64_t mtype_msew) {
    switch (width) {
    case 0b000: return 8;
    // Matrix Extension 0.5a Page11
    case 0b001: return 16;
    case 0b010: return 32;
    case 0b011: return 64;
    case 0b111: return 4;
    // 0b101 is illegal
    case 0b100: return getMelen(mtype_msew, 0b101);
    default: GEM5_UNREACHABLE;
    }
}

uint32_t
getMtilemMax(uint32_t mlen, uint32_t mrlen)
{
    uint32_t mtilemmax = mlen/mrlen;
    return mtilemmax;
}

uint32_t
getMtilekMax(uint32_t mlen, uint32_t mrlen, uint32_t melen)
{
    uint32_t mtilekmax =
    (mlen/mrlen < mrlen/melen) ? mlen/mrlen : mrlen/melen;
    return mtilekmax;
}

uint32_t
getMtilenMax(uint32_t mrlen, uint32_t melen)
{
    uint32_t mtilenmax = mrlen/melen;
    return mtilenmax;
}

uint32_t
getMtileRow(uint64_t mfunct6, uint32_t mlen, uint32_t mrlen, uint32_t mtilem, uint32_t mtilek)
{
    switch (mfunct6)
    {
    // Matrix C
    case 0:
        return mtilem;
    // Matrix A
    case 1:
        return mtilem;
    // Matrix B
    case 2:
        return mtilek;
    // Matrix Tr/Acc
    case 3:
        return getMtilemMax(mlen, mrlen);
    default: GEM5_UNREACHABLE;
    }
}

uint32_t
getMtileCol(uint64_t mfunct6, uint32_t mrlen, uint32_t melen, uint32_t mtilek, uint32_t mtilen)
{
    switch (mfunct6)
    {
    // Matrix C
    case 0:
        return mtilen;
    // Matrix A
    case 1:
        return mtilek;
    // Matrix B
    case 2:
        return mtilen;
    // Matrix Tr/Acc
    case 3:
        return getMtilenMax(mrlen, melen);
    default: GEM5_UNREACHABLE;
    }
}

// new: ztt/2025/10/28
// get Matrix element type string
// eew: 0b000: 8bit, 0b001: 16bit, 0b010: 32bit, 0b011: 64bit, 0b111: 4bit
uint32_t
getMvEEW(uint64_t width)
{
    switch (width) {
    case 0b000:
    case 0b100: return 8;
    case 0b001:
    case 0b101: return 16;
    case 0b010:
    case 0b110: return 32;
    case 0b011:
    case 0b111: return 64;
    default: GEM5_UNREACHABLE;
    }
}

const char*
getTypeStr(uint8_t eew, uint8_t fp) {
    if (fp) {
        switch (eew)
        {
        case 0b100: return "";
        case 0b000: return ".cf";
        case 0b001: return ".hf";
        case 0b010: return ".f";
        case 0b011: return ".d";
        default: return ".ERROR";
        }
    }
    else {
        switch (eew)
        {
        case 0b100: return "";
        case 0b000: return ".b";
        case 0b001: return ".h";
        case 0b010: return ".w";
        case 0b011: return ".dw";
        case 0b111: return ".hb";
        default: return ".ERROR";
        }
    }
}

std::string
MConfOp::generateDisassembly(Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << ' ' << registerName(destRegIdx(0)) << ", "
       << registerName(srcRegIdx(0));
    return ss.str();
}

std::string
MSetTilemOp::generateDisassembly(Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << ' ' << registerName(destRegIdx(0)) << ", "
       << registerName(srcRegIdx(0));
    return ss.str();
}

std::string
MSetTilemiOp::generateDisassembly(Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << ' ' << registerName(destRegIdx(0)) << ", "
       << imm10;
    return ss.str();
}

std::string
MSetTilekOp::generateDisassembly(Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << ' ' << registerName(destRegIdx(0)) << ", "
       << registerName(srcRegIdx(0));
    return ss.str();
}

std::string
MSetTilekiOp::generateDisassembly(Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << ' ' << registerName(destRegIdx(0)) << ", "
       << imm10;
    return ss.str();
}

std::string
MSetTilenOp::generateDisassembly(Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << ' ' << registerName(destRegIdx(0)) << ", "
       << registerName(srcRegIdx(0));
    return ss.str();
}

std::string
MSetTileniOp::generateDisassembly(Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << ' ' << registerName(destRegIdx(0)) << ", "
       << imm10;
    return ss.str();
}

std::string
MleMacroInst::generateDisassembly(Addr pc,
    const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    uint32_t melen = getMelen(eew, mtype.msew);
    const char* underscore = strchr(mnemonic, '_');
    if (underscore != nullptr &&
        underscore > mnemonic && *(underscore + 1) != '\0') {
        ss.write(mnemonic, underscore - mnemonic);
        ss << melen << ".";
        ss << (underscore + 1);
    } else {
        ss << mnemonic;
    }
    ss << ' ' << registerName(destRegIdx(0)) << ", "
       << '(' << registerName(srcRegIdx(0)) << ')' << ", "
       <<registerName(srcRegIdx(1));
    return ss.str();
}

std::string
MleMicroInst::generateDisassembly(Addr pc,
    const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    uint32_t melen = getMelen(eew, mtype.msew);
    const char* underscore = strchr(mnemonic, '_');
    if (underscore != nullptr &&
        underscore > mnemonic && *(underscore + 1) != '\0') {
        ss.write(mnemonic, underscore - mnemonic);
        ss << melen << ".";
        ss << (underscore + 1);
    } else {
        ss << mnemonic;
    }
    ss <<'[' << microIdx << ']' << ' '
       << registerName(destRegIdx(0)) << ", "
       << '(' << registerName(srcRegIdx(0)) << ')' << ", "
       << registerName(srcRegIdx(1));
    return ss.str();
}

std::string
MseMacroInst::generateDisassembly(Addr pc,
    const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    uint32_t melen = getMelen(eew, mtype.msew);
    const char* underscore = strchr(mnemonic, '_');
    if (underscore != nullptr &&
        underscore > mnemonic && *(underscore + 1) != '\0') {
        ss.write(mnemonic, underscore - mnemonic);
        ss << melen << ".";
        ss << (underscore + 1);
    } else {
        ss << mnemonic;
    }
    ss << ' ' << registerName(srcRegIdx(2)) << ", "
       << '(' << registerName(srcRegIdx(0)) << ')' << ", "
       <<registerName(srcRegIdx(1));
    return ss.str();
}

std::string
MseMicroInst::generateDisassembly(Addr pc,
    const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    uint32_t melen = getMelen(eew, mtype.msew);
    const char* underscore = strchr(mnemonic, '_');
    if (underscore != nullptr &&
        underscore > mnemonic && *(underscore + 1) != '\0') {
        ss.write(mnemonic, underscore - mnemonic);
        ss << melen << ".";
        ss << (underscore + 1);
    } else {
        ss << mnemonic;
    }
    ss <<'[' << microIdx << ']' << ' '
       << registerName(srcRegIdx(2)) << ", "
       << '(' << registerName(srcRegIdx(0)) << ')' << ", "
       << registerName(srcRegIdx(1));
    return ss.str();
}

std::string MatrixArithMacroInst::generateDisassembly(Addr pc,
    const loader::SymbolTable *symtab) const
{
    DPRINTF(AME, "%s gen assamebly begin\n", mnemonic);
    std::stringstream ss;
    const char* underscore = strchr(mnemonic, '_');
    // 检查指令助记符是否以 "mmv", "mfmve", "mt", "mbc" 开头
    bool is_dataMove_instr = (strncmp(mnemonic, "mmv", 3) == 0) ||
                            (strncmp(mnemonic, "mfmve", 5) == 0) ||
                            (strncmp(mnemonic, "mt", 2) == 0) ||
                            (strncmp(mnemonic, "mbc", 3) == 0);
    if (underscore != nullptr &&
        underscore > mnemonic && *(underscore + 1) != '\0') {
        if (is_dataMove_instr) {
            // 数据移动指令转换成*.*.*形式
            std::string modified_mnemonic(mnemonic);
            for (char& c : modified_mnemonic) {
                if (c == '_') c = '.';
            }
            ss << modified_mnemonic;
        } else {
            // 非数据移动指令保持原有逻辑
            ss.write(mnemonic, underscore - mnemonic);
            ss << getTypeStr(eew, fp) << ".";
            ss << (underscore + 1);
        }
    } else {
        ss << mnemonic;
    }
    ss << ' ' << registerName(destRegIdx(0))
    << ", "<< registerName(srcRegIdx(0 + ma)) << ", "
    << registerName(srcRegIdx(1 + ma));
    DPRINTF(AME, "gen assamebly end\n");
    return ss.str();
}

std::string MatrixArithMicroInst::generateDisassembly(Addr pc,
    const loader::SymbolTable *symtab) const
{
    DPRINTF(AME, "%s gen assamebly begin\n", mnemonic);
    std::stringstream ss;
    const char* underscore = strchr(mnemonic, '_');
    // 检查指令助记符是否以 "mmv", "mfmve", "mt", "mbc" 开头
    bool is_dataMove_instr = (strncmp(mnemonic, "mmv", 3) == 0) ||
                            (strncmp(mnemonic, "mfmve", 5) == 0) ||
                            (strncmp(mnemonic, "mt", 2) == 0) ||
                            (strncmp(mnemonic, "mbc", 3) == 0);
    if (underscore != nullptr &&
        underscore > mnemonic && *(underscore + 1) != '\0') {
        if (is_dataMove_instr) {
            std::string modified_mnemonic(mnemonic);
            for (char& c : modified_mnemonic) {
                if (c == '_') c = '.';
            }
            ss << modified_mnemonic;
        } else {
            // 非数据移动指令保持原有逻辑
            ss.write(mnemonic, underscore - mnemonic);
            ss << getTypeStr(eew, fp) << ".";
            ss << (underscore + 1);
        }
    } else {
        ss << mnemonic;
    }
    ss <<'[' << microIdx << ']' << ' '
    << registerName(destRegIdx(0)) << ", "
    << registerName(srcRegIdx(0 + ma)) << ", "
    << registerName(srcRegIdx(1 + ma));
    DPRINTF(AME, "gen assamebly end\n");
    return ss.str();
}

std::string MatrixArithCvtMicroInst::generateDisassembly(
    Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << '[' << microIdx << "] "
       << registerName(destRegIdx(0)) << ", "
       << registerName(srcRegIdx(0));
    return ss.str();
}

std::string MatrixArithCvtMacroInst::generateDisassembly(
    Addr pc, const loader::SymbolTable *symtab) const
{
    std::stringstream ss;
    ss << mnemonic << ' '
       << registerName(destRegIdx(0)) << ", "
       << registerName(srcRegIdx(0));
    return ss.str();
}

std::string MatrixArithMmaInst::generateDisassembly(Addr pc,
    const loader::SymbolTable *symtab) const
{
    DPRINTF(AME, "%s gen assamebly begin\n", mnemonic);
    std::stringstream ss;
    const char* underscore = strchr(mnemonic, '_');
    if (underscore != nullptr &&
        underscore > mnemonic && *(underscore + 1) != '\0') {
        ss.write(mnemonic, underscore - mnemonic);
        ss << getTypeStr(eew, fp) << ".";
        ss << (underscore + 1);
    } else {
        ss << mnemonic;
    }
    ss << ' ' << registerName(destRegIdx(0))
    << ", "<< registerName(srcRegIdx(0 + ma)) << ", "
    << registerName(srcRegIdx(1 + ma));
    DPRINTF(AME, "gen assamebly end\n");
    return ss.str();
}

} // namespace RiscvISA
} // namespace gem5
