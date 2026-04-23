/**
 * @file types.hh
 * @brief 定义 systolic array 中使用的类型, 浮点计算模板函数
 * @author ztt
 */
#ifndef __CPU_SYSTOLIC_ARRAY_TYPES_HH__
#define __CPU_SYSTOLIC_ARRAY_TYPES_HH__

#include <cstdint>
#include <stdexcept>
#include <type_traits>

// 引入softfloat.hh文件，用于fp的计算库
#include <internals.h>
#include <softfloat.h>
#include <specialize.h>

namespace gem5
{
/**
 * @brief 定义 systolic array 中使用浮点计算模板函数
 * 采用类似riscv-ame-gem5/src/arch/riscv/utility.hh中的浮点运算模板函数
 */
template<typename Type> struct double_width;
template<> struct double_width<float32_t>   { using type = float64_t;};
template<> struct double_width<float16_t>   { using type = float32_t;};
template<> struct double_width<float8_t>    { using type = float16_t;};
template<typename Type> struct quad_width;
template<> struct quad_width<float8_t>      {using type = float32_t; };
template<> struct quad_width<float16_t>     {using type = float64_t; };

static constexpr float16_t f16(uint16_t v) { return {v}; }
static constexpr float32_t f32(uint32_t v) { return {v}; }
static constexpr float64_t f64(uint64_t v) { return {v}; }
template<typename FloatType, typename IntType = decltype(FloatType::v)> auto
ftype(IntType a) -> FloatType
{
    if constexpr(std::is_same_v<uint32_t, IntType>)
        return f32(a);
    else if constexpr(std::is_same_v<uint64_t, IntType>)
        return f64(a);
    else if constexpr(std::is_same_v<uint16_t, IntType>)
        return f16(a);
    GEM5_UNREACHABLE;
}

template<typename FloatType> FloatType
fadd(FloatType a, FloatType b)
{
    if constexpr(std::is_same_v<float32_t, FloatType>)
        return f32_add(a, b);
    else if constexpr(std::is_same_v<float64_t, FloatType>)
        return f64_add(a, b);
    else if constexpr(std::is_same_v<float16_t, FloatType>)
        return f16_add(a, b);
    GEM5_UNREACHABLE;
}

template<typename FloatType> FloatType
fmul(FloatType a, FloatType b)
{
    if constexpr(std::is_same_v<float32_t, FloatType>)
        return f32_mul(a, b);
    else if constexpr(std::is_same_v<float64_t, FloatType>)
        return f64_mul(a, b);
    else if constexpr(std::is_same_v<float16_t, FloatType>)
        return f16_mul(a, b);
    GEM5_UNREACHABLE;
}

template<typename FT, typename WFT = typename double_width<FT>::type> WFT
fwiden(FT a)
{
    if constexpr(std::is_same_v<float32_t, FT>)
        return f32_to_f64(a);
    else if constexpr(std::is_same_v<float16_t, FT>)
        return f16_to_f32(a);
    GEM5_UNREACHABLE;
}

template<typename FT, typename QFT = typename quad_width<FT>::type> QFT
fquad(FT a)
{
    if constexpr(std::is_same_v<float16_t, FT>)
        return f16_to_f64(a);
    // else if constexpr(std::is_same_v<float8_t, FT>)
    //     return f16_to_f64(a);
    GEM5_UNREACHABLE;
}

/**
 * @brief 定义 systolic array 中使用的矩阵数据类型枚举
 * 包含整数和浮点数类型，用于表示 systolic array 中处理的数据类型
 */
enum MatDataType
{
    No_MatDataType = 0,
    UInt8 = 1,
    Int8 = 2,
    UInt16 = 3,
    Int16 = 4,
    UInt32 = 5,
    Int32 = 6,
    UInt64 = 7,
    Int64 = 8,
    Float16 = 9,           // 16位浮点
    BFloat16 = 10,
    Float32 = 11,         // 32位浮点
    Float64 = 12,         // 64位浮点
    Num_MatDataType = 13
};

/**
 * @brief 用于将MatDataType枚举值映射到对应的标准C++类型
 *
 * 这个结构体模板提供了一个类型转换，将MatDataType枚举值转换为对应的标准C++类型。
 * 它主要用于在编译时根据数据类型进行类型选择和转换。
 *
 * @tparam DATA_TYPE MatDataType枚举值，指定要映射的类型
 */
// 主模板声明
template<MatDataType DATA_TYPE>
struct mat_data_type_to_std_type;

// 模板特化：为每个枚举值映射到对应的标准类型
template<> struct mat_data_type_to_std_type<UInt8>
    { using type = uint8_t; };
template<> struct mat_data_type_to_std_type<Int8>
    { using type = int8_t; };
template<> struct mat_data_type_to_std_type<UInt16>
    { using type = uint16_t; };
template<> struct mat_data_type_to_std_type<Int16>
    { using type = int16_t; };
template<> struct mat_data_type_to_std_type<UInt32>
    { using type = uint32_t; };
template<> struct mat_data_type_to_std_type<Int32>
    { using type = int32_t; };
template<> struct mat_data_type_to_std_type<UInt64>
    { using type = uint64_t; };
template<> struct mat_data_type_to_std_type<Int64>
    { using type = int64_t; };
template<> struct mat_data_type_to_std_type<Float16>
    { using type = float16_t; };
template<> struct mat_data_type_to_std_type<BFloat16>
    { using type = float16_t; };  // 注意：BFloat16暂时用float16_t代替
template<> struct mat_data_type_to_std_type<Float32>
    { using type = float32_t; };
template<> struct mat_data_type_to_std_type<Float64>
    { using type = float64_t; };

// No_MatDataType 映射到void或者提供一个默认处理
template<> struct mat_data_type_to_std_type<No_MatDataType>
    { using type = void; };

// 别名模板方便使用
template<MatDataType DATA_TYPE>
using mat_type_t = typename mat_data_type_to_std_type<DATA_TYPE>::type;

/*
// 使用示例
void example() {
    mat_type_t<UInt8> a;     // uint8_t
    mat_type_t<Int16> b;     // int16_t
    mat_type_t<UInt32> c;    // uint32_t
    mat_type_t<Float32> d;   // float32_t
    mat_type_t<Float64> e;   // float64_t

    // 编译时验证类型正确性
    static_assert(std::is_same_v<mat_type_t<UInt8>, uint8_t>);
    static_assert(std::is_same_v<mat_type_t<Int32>, int32_t>);
    static_assert(std::is_same_v<mat_type_t<Float16>, float16_t>);
}
*/

/**
 * @brief 用于将标准C++类型映射到MatDataType枚举值
 *
 * 这个结构体模板提供了一个反向映射，将标准C++类型转换为对应的MatDataType枚举值。
 * 它主要用于在编译时根据类型进行选择和转换。
 *
 * @tparam T 标准C++类型，指定要映射的类型
 */
// 反向映射：从类型到枚举值（如果需要）
template<typename T>
struct std_type_to_mat_data_type;

template<> struct std_type_to_mat_data_type<uint8_t>
    { static constexpr MatDataType value = UInt8; };
template<> struct std_type_to_mat_data_type<int8_t>
    { static constexpr MatDataType value = Int8; };
template<> struct std_type_to_mat_data_type<uint16_t>
    { static constexpr MatDataType value = UInt16; };
template<> struct std_type_to_mat_data_type<int16_t>
    { static constexpr MatDataType value = Int16; };
template<> struct std_type_to_mat_data_type<uint32_t>
    { static constexpr MatDataType value = UInt32; };
template<> struct std_type_to_mat_data_type<int32_t>
    { static constexpr MatDataType value = Int32; };
template<> struct std_type_to_mat_data_type<uint64_t>
    { static constexpr MatDataType value = UInt64; };
template<> struct std_type_to_mat_data_type<int64_t>
    { static constexpr MatDataType value = Int64; };
//template<> struct std_type_to_mat_data_type<float16_t>
//  { static constexpr MatDataType value = BFloat16; };
template<> struct std_type_to_mat_data_type<float16_t>
    { static constexpr MatDataType value = Float16; };
template<> struct std_type_to_mat_data_type<float32_t>
    { static constexpr MatDataType value = Float32; };
template<> struct std_type_to_mat_data_type<float64_t>
    { static constexpr MatDataType value = Float64; };

/*
// 使用示例
void reverse_example() {
    constexpr MatDataType
        type1 = std_type_to_mat_data_type<uint16_t>::value;  // UInt16
    constexpr MatDataType
        type2 = std_type_to_mat_data_type<float32_t>::value; // Float32
}
*/

/**
 * @brief 类型特征结构体：提供关于MatDataType的编译时信息
 *
 * 这个结构体模板为每个MatDataType枚举值提供了编译时的类型特征信息。
 * 它包含了数据类型、包装类型、实际类型以及是否为整数类型等信息。
 *
 * @tparam DT MatDataType枚举值，指定要查询的类型
 */
template<MatDataType DT>
struct mat_type_traits
{
    // 核心信息
    static constexpr MatDataType data_type = DT;

    // 类型映射
    using wrapper_type = mat_type_t<DT>;  // 包装类型（用于API接口）
    using actual_type = wrapper_type;     // 默认：包装类型就是实际类型

    // 特征信息
    static constexpr bool is_integer =
        (DT == MatDataType::UInt8) || (DT == MatDataType::Int8) ||
        (DT == MatDataType::UInt16) || (DT == MatDataType::Int16) ||
        (DT == MatDataType::UInt32) || (DT == MatDataType::Int32) ||
        (DT == MatDataType::UInt64) || (DT == MatDataType::Int64);

    static constexpr const char* name = []() {
        switch(DT) {
            case MatDataType::UInt8: return "uint8";
            case MatDataType::Int8: return "int8";
            case MatDataType::UInt16: return "uint16";
            case MatDataType::Int16: return "int16";
            case MatDataType::UInt32: return "uint32";
            case MatDataType::Int32: return "int32";
            case MatDataType::UInt64: return "uint64";
            case MatDataType::Int64: return "int64";
            case MatDataType::Float16: return "float16";
            case MatDataType::BFloat16: return "bfloat16";
            case MatDataType::Float32: return "float32";
            case MatDataType::Float64: return "float64";
            default: return "unknown";
        }
    }();
};
// 整数类型使用默认的（actual_type = wrapper_type）
// 只特化需要不同actual_type的浮点类型
template<>
struct mat_type_traits<MatDataType::Float16>
{
    static constexpr MatDataType data_type = MatDataType::Float16;
    using wrapper_type = mat_type_t<MatDataType::Float16>;  // struct float16_t
    using actual_type = decltype(wrapper_type::v);          // 内部的v的类型
    static constexpr bool is_integer = false;
    static constexpr const char* name = "float16";
};
template<>
struct mat_type_traits<MatDataType::BFloat16>
{
    static constexpr MatDataType data_type = MatDataType::BFloat16;
    using wrapper_type = mat_type_t<MatDataType::BFloat16>;
    using actual_type = decltype(wrapper_type::v);
    static constexpr bool is_integer = false;
    static constexpr const char* name = "bfloat16";
};
template<>
struct mat_type_traits<MatDataType::Float32>
{
    static constexpr MatDataType data_type = MatDataType::Float32;
    using wrapper_type = mat_type_t<MatDataType::Float32>;
    using actual_type = decltype(wrapper_type::v);
    static constexpr bool is_integer = false;
    static constexpr const char* name = "float32";
};
template<>
struct mat_type_traits<MatDataType::Float64>
{
    static constexpr MatDataType data_type = MatDataType::Float64;
    using wrapper_type = mat_type_t<MatDataType::Float64>;
    using actual_type = decltype(wrapper_type::v);
    static constexpr bool is_integer = false;
    static constexpr const char* name = "float64";
};

/**
 * @brief 检查两个类型组合是否有效（是否支持）
 *
 * 这个函数模板用于检查在systolic array中是否允许将SrcDT类型的数据转换为DestDT类型。
 * 它考虑了整数类型和浮点类型的不同组合规则。
 *
 * @tparam SrcDT 源数据类型，必须是MatDataType枚举值
 * @tparam DestDT 目标数据类型，必须是MatDataType枚举值
 * @return true 如果组合有效（支持）
 * @return false 如果组合无效（不支持）
 */
template<MatDataType SrcDT, MatDataType DestDT>
constexpr bool is_valid_combination() {
    if constexpr (!mat_type_traits<SrcDT>::is_integer) {
        // 浮点类型组合规则
        if constexpr (SrcDT == MatDataType::Float16
            || SrcDT == MatDataType::BFloat16) {
            return DestDT == MatDataType::Float16
                   || DestDT == MatDataType::BFloat16
                   || DestDT == MatDataType::Float32
                   || DestDT == MatDataType::Float64;
        } else if constexpr (SrcDT == MatDataType::Float32) {
            return DestDT == MatDataType::Float32
                   || DestDT == MatDataType::Float64;
        } else if constexpr (SrcDT == MatDataType::Float64) {
            return DestDT == MatDataType::Float64;
        }
        return false;
    } else {
        // 整数类型组合规则（保持您的原有逻辑）
        if constexpr (SrcDT == MatDataType::UInt8) {
            return DestDT == MatDataType::UInt8
                   || DestDT == MatDataType::UInt16
                   || DestDT == MatDataType::UInt32
                   || DestDT == MatDataType::UInt64;
        } else if constexpr (SrcDT == MatDataType::Int8) {
            return DestDT == MatDataType::Int8
                   || DestDT == MatDataType::Int16
                   || DestDT == MatDataType::Int32
                   || DestDT == MatDataType::Int64;
        } else if constexpr (SrcDT == MatDataType::UInt16) {
            return DestDT == MatDataType::UInt16
                   || DestDT == MatDataType::UInt32
                   || DestDT == MatDataType::UInt64;
        } else if constexpr (SrcDT == MatDataType::Int16) {
            return DestDT == MatDataType::Int16
                   || DestDT == MatDataType::Int32
                   || DestDT == MatDataType::Int64;
        } else if constexpr (SrcDT == MatDataType::UInt32) {
            return DestDT == MatDataType::UInt32
                   || DestDT == MatDataType::UInt64;
        } else if constexpr (SrcDT == MatDataType::Int32) {
            return DestDT == MatDataType::Int32
                   || DestDT == MatDataType::Int64;
        } else if constexpr (SrcDT == MatDataType::UInt64) {
            return DestDT == MatDataType::UInt64;
        } else if constexpr (SrcDT == MatDataType::Int64) {
            return DestDT == MatDataType::Int64;
        }
        return false;
    }
}
/**
 * @brief 获取浮点扩展因子
 *
 * 这个函数模板用于计算在systolic array中，
 * 不同浮点类型之间的扩展关系（如float16到float32的扩展因子为2）
 * @tparam SrcDT 源数据类型，必须是MatDataType枚举值
 * @tparam DestDT 目标数据类型，必须是MatDataType枚举值
 * @return uint8_t 扩展因子
 */
template<MatDataType SrcDT, MatDataType DestDT>
constexpr uint8_t get_widening_factor() {
    if constexpr (SrcDT == MatDataType::Float16
                  || SrcDT == MatDataType::BFloat16) {
        if constexpr (DestDT == MatDataType::Float32) return 2;
        else if constexpr (DestDT == MatDataType::Float64) return 4;
    } else if constexpr (SrcDT == MatDataType::Float32) {
        if constexpr (DestDT == MatDataType::Float64) return 2;
    }
    return 1;
}

/**
 * @brief 类型分发器：根据运行时枚举值，用对应的编译期类型调用函数func
 *
 * 这个函数模板接受一个MatDataType枚举值和一个函数对象func。
 * 它根据运行时的dtype值，用对应的编译期类型调用func。
 * 这在需要根据数据类型进行不同处理的场景（如模板元编程）中非常有用。
 *
 * @tparam Func 可调用对象类型，接受两个参数：wrapper_type和actual_type
 * @param dtype 运行时的MatDataType枚举值，指定要处理的数据类型
 * @param func 要调用的函数对象，接受wrapper_type和actual_type作为参数
 */
template<typename Func>
void dispatch_mat_type(MatDataType dtype, Func&& func) {
    switch (dtype) {
        case MatDataType::UInt8:
            func(typename
                    mat_type_traits<MatDataType::UInt8>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::UInt8>::actual_type{});
            break;
        case MatDataType::Int8:
            func(typename
                    mat_type_traits<MatDataType::Int8>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::Int8>::actual_type{});
            break;
        case MatDataType::UInt16:
            func(typename
                    mat_type_traits<MatDataType::UInt16>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::UInt16>::actual_type{});
            break;
        case MatDataType::Int16:
            func(typename
                    mat_type_traits<MatDataType::Int16>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::Int16>::actual_type{});
            break;
        case MatDataType::UInt32:
            func(typename
                    mat_type_traits<MatDataType::UInt32>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::UInt32>::actual_type{});
            break;
        case MatDataType::Int32:
            func(typename
                    mat_type_traits<MatDataType::Int32>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::Int32>::actual_type{});
            break;
        case MatDataType::UInt64:
            func(typename
                    mat_type_traits<MatDataType::UInt64>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::UInt64>::actual_type{});
            break;
        case MatDataType::Int64:
            func(typename
                    mat_type_traits<MatDataType::Int64>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::Int64>::actual_type{});
            break;
        case MatDataType::Float16:
            func(typename
                    mat_type_traits<MatDataType::Float16>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::Float16>::actual_type{});
            break;
        case MatDataType::BFloat16:
            func(typename
                    mat_type_traits<MatDataType::BFloat16>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::BFloat16>::actual_type{});
            break;
        case MatDataType::Float32:
            func(typename
                    mat_type_traits<MatDataType::Float32>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::Float32>::actual_type{});
            break;
        case MatDataType::Float64:
            func(typename
                    mat_type_traits<MatDataType::Float64>::wrapper_type{},
                 typename
                    mat_type_traits<MatDataType::Float64>::actual_type{});
            break;
        default:
            throw std::runtime_error("Unsupported data type");
    }
}

}// namespace gem5
#endif // __CPU_SYSTOLIC_ARRAY_TYPES_HH__
