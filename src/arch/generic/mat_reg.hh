// Matrix Extension
#ifndef __ARCH_GENERIC_MAT_REG_HH__
#define __ARCH_GENERIC_MAT_REG_HH__

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#include "base/cprintf.hh"
#include "base/logging.hh"
#include "base/types.hh"
#include "sim/serialize_handlers.hh"

namespace gem5
{
    constexpr unsigned MaxMatRegLenInBytes = 1ULL << 29;
    constexpr unsigned MaxMatRowRegLenInBytes = 1ULL << 13; //620
    template <size_t MSIZE, size_t RSIZE>
    class MatRegContainer
    {
    private:
        static_assert((MSIZE > 0) && (RSIZE > 0), "Cannot Create Matrix Regsiter Container of zero size");
        static_assert(MSIZE <= MaxMatRegLenInBytes, "Matrix Register size limit exceeded");
        static_assert(RSIZE <= MaxMatRowRegLenInBytes, "Matrix Register Rowsize limit exceeded");
        static_assert(MSIZE % RSIZE == 0, "The size of the matrix must be an interger multiple of size of the row");

    public:
        static constexpr uint32_t row = MSIZE / RSIZE;
        static constexpr inline size_t size() { return MSIZE; };
        using RowContainer = std::array<uint8_t, RSIZE>;
        using MatrixContainer = std::array<RowContainer, MSIZE / RSIZE>;

    private:
        alignas(4) MatrixContainer container;


    public:
        MatRegContainer() {}
        MatRegContainer(const MatRegContainer &) = default;

        void set_one_row(uint8_t val, uint32_t index)
        {
            memset(container[index].data(), val, RSIZE);
        }

        void set(uint8_t val)
        {
            for (uint32_t i = 0; i < row; i++)
                set_one_row(val, i);
        }

        void zero() { set(0); }

        MatRegContainer<MSIZE, RSIZE> &
        operator=(const MatRegContainer<MSIZE, RSIZE> &that)
        {
            if (&that != this)
            {
                for (uint32_t i = 0; i < row; i++)
                {
                    std::memcpy(container[i].data(), that.container[i].data(), RSIZE);
                }
            }
            return *this;
        }

        template <size_t S2, size_t S3>
        bool
        operator!=(const MatRegContainer<S2, S3> &that) const
        {
            if ((MSIZE != S2) || (RSIZE != S3))
                return true;
            for (uint32_t i = 0; i < row; i++)
            {
                if (memcmp(container[i].data(), that.container[i].data(), RSIZE))
                    return true;
            }
            return false;
        }

        template <size_t S2, size_t S3>
        inline bool
        operator==(const MatRegContainer<S2, S3> &that) const
        {
            return !operator!=(that);
        }

        template <typename MatElem>
        std::array<MatElem *, row>
        as()
        {
            static_assert(RSIZE % sizeof(MatElem) == 0,
                          "MatElem does not evenly divide the register size");

            std::array<MatElem *, row> ptrs;
            for (size_t i = 0; i < row; ++i)
            {
                ptrs[i] = reinterpret_cast<MatElem *>(container[i].data());
            }
            return ptrs;
        }

        friend std::ostream &
        operator<<(std::ostream &os, const MatRegContainer<MSIZE, RSIZE> &v)
        {
            ccprintf(os, "{");
            uint32_t r_cnt = 0;
            for (r_cnt = 0; r_cnt < v.row; r_cnt++)
            {
                ccprintf(os, "[");
                size_t count = 0;
                for (auto &b : v.container[r_cnt])
                {
                    if (count && (count % 4) == 0)
                        os << "_";
                    ccprintf(os, "%02x", b);
                    count++;
                }
                ccprintf(os, "]");
                ccprintf(os, ",");
            }
            ccprintf(os, "}");
            return os;
        }

        std::string
        getString(const uint64_t &size) const
        {
            std::stringstream s;
            uint32_t r_cnt = 0;
            s << "{";

            for (r_cnt = 0; r_cnt < row; r_cnt++)
            {
                size_t count = 0;
                s << "[";
                for (auto &b : container[r_cnt])
                {
                    if (count && (count % 4) == 0)
                        s << "_";
                    s << std::hex << std::setfill('0') << std::setw(2) << (uint16_t)b;
                    count++;
                    if (count == size)
                        break;
                }
                s << "], ";
            }
            s << "}";
            return s.str();
        }
        friend ShowParam<MatRegContainer<MSIZE, RSIZE>>;
    };

    template <size_t Sz0, size_t Sz1>
    struct ParseParam<MatRegContainer<Sz0, Sz1>>
    {
        static bool
        parse(const std::string &str, MatRegContainer<Sz0, Sz1> &value)
        {
            fatal_if(str.size()> 2 * Sz0,
                     "Vector register value overflow at unserialize");
            for (int i = 0; i < Sz0; i++)
            {
                uint8_t b = 0;
                if (2 * i < str.size())
                    b = stoul(str.substr(i * 2, 2), nullptr, 16);
                value.template as<uint8_t>()[i] = b;
            }
            return true;
        }
    };

    template <size_t Sz0, size_t Sz1>
    struct ShowParam<MatRegContainer<Sz0, Sz1>>
    {
        static void
        show(std::ostream &os, const MatRegContainer<Sz0, Sz1> &value)
        {
            for (uint32_t i = 0; i < value.row; i++)
            {
                for (auto &b : value.container[i])
                    ccprintf(os, "%02x", b);
            }
        }
    };

    struct DummyMatRegContainer
    {
        RegVal filler = 0;
        bool operator==(const DummyMatRegContainer &d) const { return true; }
        bool operator!=(const DummyMatRegContainer &d) const { return true; }
        template <typename MatElem>
        MatElem *as() { return nullptr; }
    };

    template <>
    struct ParseParam < DummyMatRegContainer>
    {
        static bool
        parse(const std::string &s, DummyMatRegContainer &value)
        {
            return false;
        }
    };
    static_assert(sizeof(DummyMatRegContainer) == sizeof(RegVal));
    static inline std::ostream &
    operator<<(std::ostream &os, const DummyMatRegContainer &d)
    {
        return os;
    }

}
#endif // __ARCH_GENERIC_MAT_REG_HH__
