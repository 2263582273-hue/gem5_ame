// Matrix Extension
#ifndef __ARCH_RISCV_INSTS_MATRIX_HH__
#define __ARCH_RISCV_INSTS_MATRIX_HH__

#include <string>

#include "arch/riscv/faults.hh"
#include "arch/riscv/insts/static_inst.hh"
#include "arch/riscv/isa.hh"
#include "arch/riscv/regs/misc.hh"
#include "arch/riscv/utility.hh"
#include "cpu/exec_context.hh"
#include "cpu/static_inst.hh"

namespace gem5
{
namespace RiscvISA
{

    // uint32_t
    // getMSew(uint32_t msew);
    uint32_t
    getMelen(uint64_t width, uint64_t mtype_msew);

    uint32_t
    getMtilemMax(uint32_t mlen, uint32_t mrlen);

    uint32_t
    getMtilekMax(uint32_t mlen, uint32_t mrlen, uint32_t msew);

    uint32_t
    getMtilenMax(uint32_t mrlen, uint32_t msew);

    // use funct6, Load and Store Both OK
    uint32_t
    getMtileRow(uint64_t mfunct6, uint32_t mlen, uint32_t mrlen,
        uint32_t mtilem, uint32_t mtilek);

    uint32_t
    getMtileCol(uint64_t mfunct6, uint32_t mrlen, uint32_t melen,
        uint32_t mtilek, uint32_t mtilen);

    // get EEW from width，data move need
    uint32_t
    getMvEEW(uint64_t width);

    const char*
    getTypeStr(uint8_t eew, uint8_t fp);

    class MConfOp : public RiscvStaticInst
    {
        protected:
            uint64_t mimm10;
            uint64_t mimm;
            MConfOp(const char *mnem, ExtMachInst _extMachInst, OpClass __opClass)
                : RiscvStaticInst(mnem, _extMachInst, __opClass),
                  mimm10(_extMachInst.imm10),
                  mimm(_extMachInst.imm)
        {
            this->flags[IsMatrix] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MSetTilemOp : public RiscvStaticInst
    {
        protected:
            uint32_t mlen;
            uint32_t mrlen;

        MSetTilemOp(const char *mnem, ExtMachInst _extMachInst,
                uint32_t _mlen, uint32_t _mrlen,
                OpClass __opClass)
            : RiscvStaticInst(mnem, _extMachInst, __opClass),
              mlen(_mlen),
              mrlen(_mrlen)

        {
            this->flags[IsMatrix] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MSetTilemiOp : public RiscvStaticInst
    {
        protected:
            uint64_t imm10;
            uint32_t mlen;
            uint32_t mrlen;

        MSetTilemiOp(const char *mnem, ExtMachInst _extMachInst,
                uint32_t _mlen, uint32_t _mrlen,
                OpClass __opClass)
            : RiscvStaticInst(mnem, _extMachInst, __opClass),
              imm10(_extMachInst.imm10),
              mlen(_mlen),
              mrlen(_mrlen)

        {
            this->flags[IsMatrix] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MSetTilenOp : public RiscvStaticInst
    {
        protected:
            uint32_t mrlen;
        MSetTilenOp(const char *mnem, ExtMachInst _extMachInst,
            uint32_t _mrlen,
            OpClass __opClass)
            : RiscvStaticInst(mnem, _extMachInst, __opClass),
              mrlen(_mrlen)

        {
            this->flags[IsMatrix] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MSetTileniOp : public RiscvStaticInst
    {
        protected:
            uint64_t imm10;
            uint32_t mrlen;
        MSetTileniOp(const char *mnem, ExtMachInst _extMachInst,
                uint32_t _mrlen,
                OpClass __opClass)
            : RiscvStaticInst(mnem, _extMachInst, __opClass),
              imm10(_extMachInst.imm10),
              mrlen(_mrlen)

        {
            this->flags[IsMatrix] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MSetTilekOp : public RiscvStaticInst
    {
        protected:
            uint32_t mlen;
            uint32_t mrlen;

        MSetTilekOp(const char *mnem, ExtMachInst _extMachInst,
                uint32_t _mlen, uint32_t _mrlen,
                OpClass __opClass)
            : RiscvStaticInst(mnem, _extMachInst, __opClass),
              mlen(_mlen),
              mrlen(_mrlen)

        {
            this->flags[IsMatrix] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MSetTilekiOp : public RiscvStaticInst
    {
        protected:
            uint64_t imm10;
            uint32_t mlen;
            uint32_t mrlen;
        MSetTilekiOp(const char *mnem, ExtMachInst _extMachInst,
                uint32_t _mlen, uint32_t _mrlen,
                OpClass __opClass)
            : RiscvStaticInst(mnem, _extMachInst, __opClass),
              imm10(_extMachInst.imm10),
              mlen(_mlen),
              mrlen(_mrlen)

        {
            this->flags[IsMatrix] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MatrixNopMicroInst : public RiscvMicroInst
    {
    public:
        MatrixNopMicroInst(ExtMachInst _machInst)
            : RiscvMicroInst("mnop", _machInst, No_OpClass)
        {}

        Fault execute(ExecContext* xc, trace::InstRecord* traceData)
            const override
        {
            return NoFault;
        }

        std::string generateDisassembly(Addr pc, const loader::SymbolTable *symtab)
            const override
        {
            std::stringstream ss;
            ss << mnemonic;
            return ss.str();
        }
    };

    class MatrixMacroInst : public RiscvMacroInst
    {
    protected:
        uint32_t mlen;
        uint32_t mrlen;
        uint32_t mtilem;
        uint32_t mtilek;
        uint32_t mtilen;
        MTYPE mtype;
        uint8_t eew;

        MatrixMacroInst(const char* mnem, ExtMachInst _machInst,
                    OpClass __opClass, uint32_t _mlen,
                    uint32_t _mrlen, uint32_t _mtilem,
                    uint32_t _mtilek, uint32_t _mtilen,
                    MTYPE _mtype)
            : RiscvMacroInst(mnem, _machInst, __opClass),
            mlen(_mlen),
            mrlen(_mrlen),
            mtilem(_mtilem),
            mtilek(_mtilek),
            mtilen(_mtilen),
            mtype(_mtype),
            eew(_machInst.eew)
        {
            this->flags[IsMatrix] = true;
        }
    };

    class MatrixMemMacroInst : public MatrixMacroInst{
    protected:
        MatrixMemMacroInst(const char* mnem, ExtMachInst _machInst,
                        OpClass __opClass, uint32_t _mlen,
                        uint32_t _mrlen, uint32_t _mtilem,
                        uint32_t _mtilek, uint32_t _mtilen,
                        MTYPE _mtype)
        : MatrixMacroInst(mnem, _machInst,
                        __opClass, _mlen,
                        _mrlen, _mtilem,
                        _mtilek, _mtilen,
                        _mtype)
        {}
    };

    class MatrixMicroInst : public RiscvMicroInst
    {
    public:
        uint32_t microIdx;
        uint32_t mlen;
        uint32_t mrlen;
        uint32_t mtilem;
        uint32_t mtilek;
        uint32_t mtilen;
        MTYPE mtype;
        uint8_t ls;
        uint8_t eew;
        uint8_t tr;
        uint8_t widen;

        MatrixMicroInst(const char *mnem, ExtMachInst _machInst,
                    OpClass __opClass, uint32_t _microIdx,
                    uint32_t _mlen, uint32_t _mrlen,
                    uint32_t _mtilem, uint32_t _mtilek,
                    uint32_t _mtilen, MTYPE _mtype)
            : RiscvMicroInst(mnem, _machInst, __opClass),
            microIdx(_microIdx),
            mlen(_mlen),
            mrlen(_mrlen),
            mtilem(_mtilem),
            mtilek(_mtilek),
            mtilen(_mtilen),
            mtype(_mtype),
            ls(_machInst.ls),
            eew(_machInst.eew),
            tr(_machInst.tr),
            widen(_machInst.widen)
        {
            this->flags[IsMatrix] = true;
            _machInst.ms3;
        }

    };

    class MleMacroInst : public MatrixMemMacroInst
    {
    protected:
        MleMacroInst(const char* mnem, ExtMachInst _machInst,
                    OpClass __opClass, uint32_t _mlen,
                    uint32_t _mrlen, uint32_t _mtilem,
                    uint32_t _mtilek, uint32_t _mtilen,
                    MTYPE _mtype)
            : MatrixMemMacroInst(mnem, _machInst,
                                __opClass, _mlen,
                                _mrlen, _mtilem,
                                _mtilek, _mtilen,
                                _mtype)
        {}

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MleMicroInst : public MatrixMicroInst
    {
    public:
        uint32_t mlen;
        uint32_t mrlen;
        uint32_t mtilem;
        uint32_t mtilek;
        uint32_t mtilen;
        MTYPE mtype;
        uint8_t ls;
        uint8_t eew;
        uint8_t tr;
        uint8_t widen;
    protected:
        Request::Flags memAccessFlags;

        MleMicroInst(const char *mnem, ExtMachInst _machInst,
                    OpClass __opClass, uint32_t _microIdx,
                    uint32_t _mlen, uint32_t _mrlen,
                    uint32_t _mtilem, uint32_t _mtilek,
                    uint32_t _mtilen, MTYPE _mtype)
            : MatrixMicroInst(mnem, _machInst,
                            __opClass, _microIdx,
                            _mlen, _mrlen,
                            _mtilem, _mtilek,
                            _mtilen, _mtype),
            mlen(_mlen),
            mrlen(_mrlen),
            mtilem(_mtilem),
            mtilek(_mtilek),
            mtilen(_mtilen),
            mtype(_mtype),
            ls(_machInst.ls),
            eew(_machInst.eew),
            tr(_machInst.tr),
            widen(_machInst.widen)
        {
            this->flags[IsLoad] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MseMacroInst : public MatrixMemMacroInst
    {
    protected:
        MseMacroInst(const char* mnem, ExtMachInst _machInst,
                    OpClass __opClass, uint32_t _mlen,
                    uint32_t _mrlen, uint32_t _mtilem,
                    uint32_t _mtilek, uint32_t _mtilen,
                    MTYPE _mtype)
            : MatrixMemMacroInst(mnem, _machInst,
                                __opClass, _mlen,
                                _mrlen, _mtilem,
                                _mtilek, _mtilen,
                                _mtype)
        {}

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MseMicroInst : public MatrixMicroInst
    {
    public:
        uint32_t mlen;
        uint32_t mrlen;
        uint32_t mtilem;
        uint32_t mtilek;
        uint32_t mtilen;
        MTYPE mtype;
        uint8_t ls;
        uint8_t eew;
        uint8_t tr;
        uint8_t widen;
    protected:
        Request::Flags memAccessFlags;

        MseMicroInst(const char *mnem, ExtMachInst _machInst,
                    OpClass __opClass, uint32_t _microIdx,
                    uint32_t _mlen, uint32_t _mrlen,
                    uint32_t _mtilem, uint32_t _mtilek,
                    uint32_t _mtilen, MTYPE _mtype)
            : MatrixMicroInst(mnem, _machInst,
                            __opClass, _microIdx,
                            _mlen, _mrlen,
                            _mtilem, _mtilek,
                            _mtilen, _mtype),
            mlen(_mlen),
            mrlen(_mrlen),
            mtilem(_mtilem),
            mtilek(_mtilek),
            mtilen(_mtilen),
            mtype(_mtype),
            ls(_machInst.ls),
            eew(_machInst.eew),
            tr(_machInst.tr),
            widen(_machInst.widen)
        {
            this->flags[IsStore] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MatrixArithMicroInst : public MatrixMicroInst
    {
    protected:
        uint8_t fp;
        uint8_t sa;
        uint8_t sn;
        uint8_t ma;
        MatrixArithMicroInst(const char *mnem, ExtMachInst _machInst,
                            OpClass __opClass, uint32_t _microIdx,
                            uint32_t _mlen, uint32_t _mrlen,
                            uint32_t _mtilem, uint32_t _mtilek,
                            uint32_t _mtilen, MTYPE _mtype)
            : MatrixMicroInst(mnem, _machInst,
                            __opClass, _microIdx,
                            _mlen, _mrlen,
                            _mtilem, _mtilek,
                            _mtilen, _mtype),
            fp(_machInst.fp),
            sa(_machInst.sa),
            sn(_machInst.sn),
            ma(_machInst.ma)
        {
            this->flags[IsMatrix] = true;
            if (fp) {
                flags[IsFloating] = true;
            } else {
                flags[IsInteger] = true;
            }
        }

        std::string generateDisassembly(
                Addr pc, const loader::SymbolTable *symtab) const override;
    public:
        uint64_t getK() const {return mtilek;};
    };

    class MatrixArithMacroInst : public MatrixMacroInst
    {
    protected:
        uint8_t fp;
        uint8_t sa;
        uint8_t sn;
        uint8_t ma;
        uint8_t widen;
        MatrixArithMacroInst(const char* mnem, ExtMachInst _machInst,
                            OpClass __opClass,
                            uint32_t _mlen, uint32_t _mrlen,
                            uint32_t _mtilem, uint32_t _mtilek,
                            uint32_t _mtilen, MTYPE _mtype)
            : MatrixMacroInst(mnem, _machInst,
                            __opClass,
                            _mlen, _mrlen,
                            _mtilem, _mtilek,
                            _mtilen, _mtype),
            fp(_machInst.fp),
            sa(_machInst.sa),
            sn(_machInst.sn),
            ma(_machInst.ma),
            widen(_machInst.widen)
        {
            this->flags[IsMatrix] = true;
            if (fp) {
                flags[IsFloating] = true;
            } else {
                flags[IsInteger] = true;
            }
        }
        std::string generateDisassembly(
                Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MatrixArithCvtMicroInst : public MatrixMicroInst
    {
    protected:
        // 符合 templates 中的调用：
        // "%(mnemonic)s", _machInst, %(op_class)s, _microIdx,
        // _mlen, _mrlen, _mtilem, _mtilek, _mtilen, _mtype
        MatrixArithCvtMicroInst(const char *mnem, ExtMachInst _machInst,
                            OpClass __opClass,
                            uint32_t _microIdx,
                            uint64_t _mlen, uint64_t _mrlen,
                            uint64_t _mtilem, uint64_t _mtilek, uint64_t _mtilen,
                            MTYPE _mtype): MatrixMicroInst(mnem, _machInst,
                                        __opClass, _microIdx,
                                        _mlen, _mrlen,
                                        _mtilem, _mtilek,
                                        _mtilen, _mtype)
        {
            this->flags[IsMatrix] = true;
        }


    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };

    class MatrixArithCvtMacroInst : public MatrixMacroInst
    {
    protected:
        // 符合 templates 中的调用：
        // "%(mnemonic)s", _machInst, %(op_class)s,
        // _mlen, _mrlen, _mtilem, _mtilek, _mtilen, _mtype
        MatrixArithCvtMacroInst(const char *mnem, ExtMachInst _machInst,
                            OpClass __opClass,
                            uint64_t _mlen, uint64_t _mrlen,
                            uint64_t _mtilem, uint64_t _mtilek, uint64_t _mtilen,
                            MTYPE _mtype)
                        : MatrixMacroInst(mnem, _machInst,
                                        __opClass, _mlen,
                                        _mrlen, _mtilem,
                                        _mtilek, _mtilen,
                                        _mtype)
        {
            this->flags[IsMatrix] = true;
        }

        std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
    };


    class MatrixArithMmaInst : public RiscvStaticInst
    {
    public:
        uint32_t mlen;
        uint32_t mrlen;
        uint32_t mtilem;
        uint32_t mtilek;
        uint32_t mtilen;
        MTYPE mtype;
        uint8_t eew;
        uint8_t fp;
        uint8_t sa;
        uint8_t sn;
        uint8_t ma;
        uint8_t widen;
    protected:
        MatrixArithMmaInst(const char* mnem, ExtMachInst _machInst,
                            OpClass __opClass,
                            uint32_t _mlen, uint32_t _mrlen,
                            uint32_t _mtilem, uint32_t _mtilek,
                            uint32_t _mtilen, MTYPE _mtype)
            :
            RiscvStaticInst(mnem, _machInst, __opClass),
            mlen(_mlen),
            mrlen(_mrlen),
            mtilem(_mtilem),
            mtilek(_mtilek),
            mtilen(_mtilen),
            mtype(_mtype),
            eew(_machInst.eew),
            fp(_machInst.fp),
            sa(_machInst.sa),
            sn(_machInst.sn),
            ma(_machInst.ma),
            widen(_machInst.widen)
        {
            this->flags[IsMatrix] = true;
            if (fp) {
                flags[IsFloating] = true;
            } else {
                flags[IsInteger] = true;
            }
        }
        std::string generateDisassembly(
                Addr pc, const loader::SymbolTable *symtab) const override;
    };

} // namespace RiscvISA
} // namespace gem5

#endif // __ARCH_RISCV_INSTS_MATRIX_HH__
