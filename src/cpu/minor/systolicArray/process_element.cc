/**
 * @file process_element.cc
 * @brief 实现脉动阵列中的处理元素（PE）
 * @author ztt
 */
#include "cpu/minor/systolicArray/process_element.hh"

#include <cmath>
#include <cstring>
#include <iostream>

#include "debug/ProcessElement.hh"

namespace gem5
{
ProcessElement::ProcessElement(
    const std::string &name_,
    uint64_t opDelay,
    uint64_t* dataA,
    uint64_t* dataB,
    __uint128_t* output):
      Named(name_),
      outputBuffer_(name_ + ".outputBuffer", opDelay),
      srcType_(MatDataType::Int8),
      destType_(MatDataType::Int32),
      saturationEnabled_(false),
      wideningFactor_(1),
      isConfigured_(false),
      dataA_(dataA),
      dataB_(dataB),
      elementsPerWord_(0),
      wordsNeeded_(0),
      wordsCollectedA_(0),
      wordsCollectedB_(0),
      //dotProduct_(0),
      outputRegister_(output),
      outputValid_(false),
      computeFinished_(false)
{
    dataBufferA_.reserve(8); // 最多需要8个64位字（64-bit数据类型）
    dataBufferB_.reserve(8);
    DPRINTF(ProcessElement,
        "[ProcessElement::ProcessElement] Constructor: opDelay=%d\n",
        opDelay);
}

void ProcessElement::configure(MatDataType srcType, MatDataType destType,
     bool saturationEnabled, uint8_t wideningFactor)
{
    DPRINTF(ProcessElement,
        "[ProcessElement::configure] "
        "Configuring: "
        "srcType=%d, destType=%d, saturation=%d, widening=%d\n",
        static_cast<int>(srcType),
        static_cast<int>(destType),
        saturationEnabled,
        wideningFactor);
    srcType_ = srcType;
    destType_ = destType;
    saturationEnabled_ = saturationEnabled;
    wideningFactor_ = wideningFactor;

    // 计算数据收集参数
    calculateDataCollectionParams();

    // 根据计算类型配置outputBuffer_延迟
    configureComputeDelay();
    outputBuffer_.setDelay(computeCycles_);

    // 重置状态
    outputValid_ = false;
    computeFinished_ = false;
    *outputRegister_ = 0;
    wordsCollectedA_ = 0;
    wordsCollectedB_ = 0;
    dataBufferA_.clear();
    dataBufferB_.clear();
    msat_ = false;

    // 标记为已配置
    isConfigured_ = true;
    DPRINTF(ProcessElement,
        "[ProcessElement::configure] Configuration complete, advance start\n");
}

// 延迟配置方法
void ProcessElement::configureComputeDelay()
{
    DPRINTF(ProcessElement,
        "[ProcessElement::configureComputeDelay] "
        "Starting delay configuration\n");
    // 基础延迟配置表
    struct DelayConfig
    {
        MatDataType srcType;
        bool saturationEnabled;
        bool isFloat;
        uint8_t wideningFactor;
        int baseDelay;
    };

    // 延迟配置表
    // todo: 这里需要根据实际硬件特性调整,目前只是初版
    static const std::vector<DelayConfig> delayConfigs = {
        // 整数等宽运算（1周期基础）
        {MatDataType::UInt8, false, false, 1, 1},
        {MatDataType::UInt16, false, false, 1, 1},
        {MatDataType::UInt32, false, false, 2, 1},
        {MatDataType::UInt64, false, false, 4, 1},

        {MatDataType::Int8, false, false, 1, 1},
        {MatDataType::Int16, false, false, 1, 1},
        {MatDataType::Int32, false, false, 1, 1},
        {MatDataType::Int64, false, false, 1, 1},

        // 整数等宽饱和运算额外+1周期
        {MatDataType::UInt8, true, false, 1, 2},
        {MatDataType::UInt16, true, false, 1, 2},
        {MatDataType::UInt32, true, false, 1, 2},
        {MatDataType::UInt64, true, false, 1, 2},

        {MatDataType::Int8, true, false, 1, 2},
        {MatDataType::Int16, true, false, 1, 2},
        {MatDataType::Int32, true, false, 1, 2},
        {MatDataType::Int64, true, false, 1, 2},

        // 加宽运算延迟增加
        // 两倍加宽不饱和
        {MatDataType::UInt8, false, false, 2, 3},
        {MatDataType::UInt16, false, false, 2, 3},
        {MatDataType::UInt32, false, false, 2, 3},
        {MatDataType::Int8, false, false, 2, 3},
        {MatDataType::Int16, false, false, 2, 3},
        {MatDataType::Int32, false, false, 2, 3},
        // 两倍加宽饱和
        {MatDataType::UInt8, true, false, 2, 3},
        {MatDataType::UInt16, true, false, 2, 3},
        {MatDataType::UInt32, true, false, 2, 3},
        {MatDataType::Int8, true, false, 2, 3},
        {MatDataType::Int16, true, false, 2, 3},
        {MatDataType::Int32, true, false, 2, 3},

        // 四倍加宽饱和
        {MatDataType::UInt8, true, false, 4, 3},
        {MatDataType::UInt16, true, false, 4, 3},
        {MatDataType::Int8, true, false, 4, 3},
        {MatDataType::Int16, true, false, 4, 3},
        // 四倍加宽不饱和
        {MatDataType::UInt8, false, false, 4, 3},
        {MatDataType::UInt16, false, false, 4, 3},
        {MatDataType::Int8, false, false, 4, 3},
        {MatDataType::Int16, false, false, 4, 3},

        // 浮点运算（3周期基础）
        // 等宽
        {MatDataType::Float16, false, true, 1, 3},
        {MatDataType::Float32, false, true, 1, 4},
        {MatDataType::Float64, false, true, 1, 5},
        // 加宽
        {MatDataType::Float16, false, true, 2, 4},
        {MatDataType::Float32, false, true, 2, 5},
        {MatDataType::Float64, false, true, 4, 6}
    };

    // 查找匹配的配置
    computeCycles_ = 1; // 默认值
    for (const auto& config : delayConfigs) {
        if (config.isFloat == isFloatingPoint(srcType_) &&
            config.srcType == srcType_ &&
            config.saturationEnabled == saturationEnabled_ &&
            config.wideningFactor == wideningFactor_) {
            computeCycles_ = config.baseDelay;
            break;
        }
    }
    DPRINTF(ProcessElement,
        "[ProcessElement::configureComputeDelay] "
        "Setting computeCycles=%d for srcType=%d\n",
        computeCycles_, static_cast<int>(srcType_));
}

// 计算数据收集的相关参数
void ProcessElement::calculateDataCollectionParams()
{
    size_t srcBitWidth = getTypeBitWidth(srcType_);

    // 计算每个64位字包含的元素数量
    elementsPerWord_ = 64 / srcBitWidth;

    // 计算需要多少个64位字来收集8个元素
    wordsNeeded_ = (TARGET_ELEMENTS + elementsPerWord_ - 1) / elementsPerWord_;

    DPRINTF(ProcessElement,
        "[ProcessElement::calculateDataCollectionParams] "
        "srcBitWidth=%zu, elementsPerWord=%u, wordsNeeded=%u\n",
        srcBitWidth, elementsPerWord_, wordsNeeded_);
}

// 将横向数据输入dataBufferA
void ProcessElement::feedDataA(uint64_t* data)
{
    assert (wordsCollectedA_ < wordsNeeded_);
    dataBufferA_.push_back(*data);
    wordsCollectedA_++;

    DPRINTF(ProcessElement,
        "[ProcessElement::feedDataA] "
        "Data=0x%016lx,wordsCollectedA=%u/%u\n",
        *data, wordsCollectedA_, wordsNeeded_);
}

// 将纵向数据输入dataBufferB
void ProcessElement::feedDataB(uint64_t* data)
{
    assert (wordsCollectedB_ < wordsNeeded_);
    dataBufferB_.push_back(*data);
    wordsCollectedB_++;

    DPRINTF(ProcessElement,
        "[ProcessElement::feedDataB] "
        "Data=0x%016lx,wordsCollectedB=%u/%u\n",
        *data, wordsCollectedB_, wordsNeeded_);
}

// 推进计算和输出缓冲区
void ProcessElement::advance(bool dataFinished)
{
    DPRINTF(ProcessElement,
        "[ProcessElement::advance] advance start, dataFinished=%d\n",
        dataFinished);

    debugPrintState();
    DPRINTF(ProcessElement,
        "[ProcessElement::advance] Output Buffer Occupancy=%u\n",
        outputBuffer_.occupancy);
    // 输出缓冲区推进
    if (outputBuffer_.advance()) {
        /*
         * true表示有数据已经经历computeCycles_个周期延迟
         * todo：目前，直接在数据完整的时候将结果累加到output_register_中，
         * 所以这里并不需要对pop的数据进行处理，因为已经处理过了
         * todo: 之后，可以考虑在延迟结束的时候再加到output_register_中，
         * 形式上看更加符合流水线实际
         * 如果已完成最后一轮计算，且输出缓冲区为空，说明计算已经完成
        */

        if (outputBuffer_.occupancy == 0 && computeFinished_){
            outputValid_ = true;
            DPRINTF(ProcessElement,
                "[ProcessElement::advance] Output Valid=%d\n",
                outputValid_);
        }
    }
    // 每个周期读取输入数据
    if (!dataFinished){
        feedDataA(dataA_);
        feedDataB(dataB_);
    }
    // 如果dataBuffer中数据足够，将数据送到计算路径中计算
    if ((wordsCollectedA_ >= wordsNeeded_ && wordsCollectedB_ >= wordsNeeded_)
        || dataFinished) {
        DPRINTF(ProcessElement,
            "[ProcessElement::advance] "
            "Data collection complete, Starting MAC calculation\n");
        if (wordsCollectedA_ > 0 && wordsCollectedB_ > 0){
            // 除非dataFinished且无任何有效数据，否则进入到此分支就应该执行MAC计算
            executeMACCalculation();
            outputBuffer_.push(*outputRegister_);
            DPRINTF(ProcessElement,
                "[ProcessElement::advance] "
                "Data finished signal received, but data incomplete. "
                "Padding with zeros.\n");
        }

        // 如果已经是最后一轮数据，标记计算完成
        if (dataFinished){
            computeFinished_ = true;
            DPRINTF(ProcessElement,
                "[ProcessElement::advance] Compute Finished=%d\n",
                computeFinished_);
        }
        // 重置数据收集状态，准备下一轮数据收集
        wordsCollectedA_ = 0;
        wordsCollectedB_ = 0;
        dataBufferA_.clear();
        dataBufferB_.clear();
        msat_ = false;
    }
    DPRINTF(ProcessElement,
        "[ProcessElement::advance] advance end\n");
}

// 获取输出结果
__uint128_t ProcessElement::getOutput() const
{
    DPRINTF(ProcessElement,
        "[ProcessElement::getOutput] Returning output 0x%016lx\n",
        *outputRegister_);
    return *outputRegister_;
}

// 重置pe状态
void ProcessElement::reset()
{
    if (outputValid_) {
        DPRINTF(ProcessElement,
            "[ProcessElement::reset] Resetting state\n");
        // 准备下一轮计算
        outputValid_ = false;
        computeFinished_ = false;
        wordsCollectedA_ = 0;
        wordsCollectedB_ = 0;
        dataBufferA_.clear();
        dataBufferB_.clear();
        *outputRegister_ = 0;
        msat_ = false;
        isConfigured_ = false;
    }
}

bool ProcessElement::isIdle() const
{
    bool idle = !isConfigured_;
    DPRINTF(ProcessElement,
        "[ProcessElement::isIdle] isIdle=%d\n", idle);
    return idle;
}

bool ProcessElement::isComputing() const
{
    bool computing = isConfigured_ && !computeFinished_;
    DPRINTF(ProcessElement,
        "[ProcessElement::isComputing] isComputing=%d\n",
        computing);
    return computing;
}

bool ProcessElement::isOutputValid() const
{
    bool valid = outputValid_;
    DPRINTF(ProcessElement,
        "[ProcessElement::isOutputValid] isOutputValid=%d\n",
        valid);
    return valid;
}

// === 解包方法：从buffer的64位字中提取多个元素,直到提取出8个元素 ===
template<typename T>
std::vector<T> ProcessElement::unpackOperands(
    const std::vector<uint64_t>& packedData, bool isFloat) const
{
    // 无论T是什么类型，都返回std::vector<T>，这是函数的契约
    std::vector<T> elements;
    elements.reserve(TARGET_ELEMENTS);

    // 计算基本参数
    constexpr size_t typeSize = sizeof(T);
    constexpr size_t bitsPerElement = typeSize * 8;
    // 防止除零错误，并处理单个元素超过64位的特殊情况
    constexpr size_t elementsPerWord = (bitsPerElement == 0) ?
        1 : (bitsPerElement > 64) ? 1 : 64 / bitsPerElement;

    uint32_t elementsExtracted = 0;

    // 预先计算掩码，避免重复计算
    constexpr uint64_t mask = (bitsPerElement >= 64) ?
        ~0ULL : (1ULL << bitsPerElement) - 1;

    // 认为传进来64bit数据
    for (uint64_t word : packedData) {
        for (size_t i = 0;
            i < elementsPerWord && elementsExtracted < TARGET_ELEMENTS;
            i++) {
            // 计算移位量并提取位段
            uint64_t shiftAmount = i * bitsPerElement;
            uint64_t elementBits = (word >> shiftAmount) & mask;

            T element;

            // 核心修改：使用if constexpr进行编译时类型分发
            // 只有在编译时条件为true的代码块才会被实例化
            // std::is_same_v用于在编译时判断两个类型是否完全相同
            if constexpr (std::is_same_v<T, float16_t>
                || std::is_same_v<T, float32_t>
                || std::is_same_v<T, float64_t>){
                element.v = static_cast<decltype(T::v)>(elementBits);
            }else{
                element = static_cast<T>(elementBits);
            }

            elements.push_back(element);
            elementsExtracted++;

            // 提前退出内层循环
            if (elementsExtracted >= TARGET_ELEMENTS) break;
        }

        // 提前退出外层循环
        if (elementsExtracted >= TARGET_ELEMENTS) break;
    }

    // 安全的补零操作
    // 使用resize而不是while循环，更加高效
    // T{} 表示T类型的默认值，对于自定义类型会调用默认构造函数
    elements.resize(TARGET_ELEMENTS, T{});

    return elements;
}
template std::vector<uint8_t> ProcessElement::unpackOperands<uint8_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<int8_t> ProcessElement::unpackOperands<int8_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<uint16_t> ProcessElement::unpackOperands<uint16_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<int16_t> ProcessElement::unpackOperands<int16_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<uint32_t> ProcessElement::unpackOperands<uint32_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<int32_t> ProcessElement::unpackOperands<int32_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<uint64_t> ProcessElement::unpackOperands<uint64_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<int64_t> ProcessElement::unpackOperands<int64_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<float16_t> ProcessElement::unpackOperands<float16_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<float32_t> ProcessElement::unpackOperands<float32_t>
    (const std::vector<uint64_t>&, bool) const;
template std::vector<float64_t> ProcessElement::unpackOperands<float64_t>
    (const std::vector<uint64_t>&, bool) const;

// === 执行MAC计算 ===
void ProcessElement::executeMACCalculation()
{
    if (dataBufferA_.size() < wordsNeeded_
        || dataBufferB_.size() < wordsNeeded_) {
        // 数据不足，补足零
        while (dataBufferA_.size() < wordsNeeded_) {
            dataBufferA_.push_back(0);
        }
        while (dataBufferB_.size() < wordsNeeded_) {
            dataBufferB_.push_back(0);
        }
    }

    dispatchCalculation();
}

// === 使用类型分发调度计算 ===
void ProcessElement::dispatchCalculation()
{
    dispatch_mat_type(srcType_,
                      [this](auto src_wrapper, auto src_actual) {
        dispatch_mat_type(destType_,
                          [this](auto dest_wrapper, auto dest_actual) {
            // 获取枚举值对应的标准数据类型
            using SrcT = decltype(src_wrapper);
            using DestT = decltype(dest_wrapper);
            // 通过实际类型获取对应的枚举值
            constexpr MatDataType
                src_dt = std_type_to_mat_data_type<SrcT>::value;
            constexpr MatDataType
                dest_dt = std_type_to_mat_data_type<DestT>::value;

            // 检查类型组合是否有效
            if constexpr (!is_valid_combination<src_dt, dest_dt>()) {
                // 无效组合，输出0
                *outputRegister_ = 0;
                return;
            } else{
                // 根据是否为整数类型选择不同的计算路径
                if constexpr (mat_type_traits<src_dt>::is_integer) {
                    // 整数计算路径
                    if (saturationEnabled_) {
                        computeIntegerMAC<SrcT, DestT, true>();
                    } else {
                        computeIntegerMAC<SrcT, DestT, false>();
                    }
                } else {
                    // 浮点数计算路径
                    constexpr uint8_t
                        widening = get_widening_factor<src_dt, dest_dt>();
                    computeFloatMAC<SrcT, DestT, widening>();
                }
            }

        });
    });
}

// === 整数MAC计算 ===
template<typename SrcT, typename DestT, bool saturationEnabled>
void ProcessElement::computeIntegerMAC()
{
    bool isFloat = false;
    auto elementsA = unpackOperands<SrcT>(dataBufferA_, isFloat);
    auto elementsB = unpackOperands<SrcT>(dataBufferB_, isFloat);

    // 非饱和情况下，使用DestT作为累加类型；
    // 饱和情况下，使用更大位宽的类型进行累加以避免中间溢出
    using tt = typename std::conditional_t<
        !saturationEnabled,
        DestT,
        std::conditional_t<
            (std::is_same_v<SrcT, int8_t> || std::is_same_v<SrcT, int16_t> ||
            std::is_same_v<SrcT, int32_t> || std::is_same_v<SrcT, int64_t>),
            __int128_t, __uint128_t>>;
    tt sum = 0;
    tt varA, varB;
    // 执行8元素点积
    for (int i = 0; i < TARGET_ELEMENTS; i++) {
        varA = static_cast<tt>(elementsA[i]);
        varB = static_cast<tt>(elementsB[i]);
        sum += varA * varB;
         DPRINTF(ProcessElement,
            "[ProcessElement::computeIntegerMAC] INT_MAC: "
            "element[%d] A=%lld, B=%lld, sum=%lld->%lld\n",
            i, static_cast<long long>(varA), static_cast<long long>(varB),
            static_cast<long long>(sum));
    }

    *outputRegister_ = static_cast<__uint128_t>(static_cast<tt>(sum)
                       + static_cast<tt>(*outputRegister_));
}
template void ProcessElement::computeIntegerMAC<uint8_t, uint8_t, false>();
template void ProcessElement::computeIntegerMAC<uint8_t, uint16_t, false>();
template void ProcessElement::computeIntegerMAC<uint8_t, uint32_t, false>();
template void ProcessElement::computeIntegerMAC<int8_t, int8_t, false>();
template void ProcessElement::computeIntegerMAC<int8_t, int16_t, false>();
template void ProcessElement::computeIntegerMAC<int8_t, int32_t, false>();
template void ProcessElement::computeIntegerMAC<uint16_t, uint16_t, false>();
template void ProcessElement::computeIntegerMAC<uint16_t, uint32_t, false>();
template void ProcessElement::computeIntegerMAC<uint16_t, uint64_t, false>();
template void ProcessElement::computeIntegerMAC<int16_t, int16_t, false>();
template void ProcessElement::computeIntegerMAC<int16_t, int32_t, false>();
template void ProcessElement::computeIntegerMAC<int16_t, int64_t, false>();
template void ProcessElement::computeIntegerMAC<uint32_t, uint32_t, false>();
template void ProcessElement::computeIntegerMAC<uint32_t, uint64_t, false>();
template void ProcessElement::computeIntegerMAC<int32_t, int32_t, false>();
template void ProcessElement::computeIntegerMAC<int32_t, int64_t, false>();
template void ProcessElement::computeIntegerMAC<uint64_t, uint64_t, false>();
template void ProcessElement::computeIntegerMAC<int64_t, int64_t, false>();

template void ProcessElement::computeIntegerMAC<uint8_t, uint8_t, true>();
template void ProcessElement::computeIntegerMAC<uint8_t, uint16_t, true>();
template void ProcessElement::computeIntegerMAC<uint8_t, uint32_t, true>();
template void ProcessElement::computeIntegerMAC<int8_t, int8_t, true>();
template void ProcessElement::computeIntegerMAC<int8_t, int16_t, true>();
template void ProcessElement::computeIntegerMAC<int8_t, int32_t, true>();
template void ProcessElement::computeIntegerMAC<uint16_t, uint16_t, true>();
template void ProcessElement::computeIntegerMAC<uint16_t, uint32_t, true>();
template void ProcessElement::computeIntegerMAC<uint16_t, uint64_t, true>();
template void ProcessElement::computeIntegerMAC<int16_t, int16_t, true>();
template void ProcessElement::computeIntegerMAC<int16_t, int32_t, true>();
template void ProcessElement::computeIntegerMAC<int16_t, int64_t, true>();
template void ProcessElement::computeIntegerMAC<uint32_t, uint32_t, true>();
template void ProcessElement::computeIntegerMAC<uint32_t, uint64_t, true>();
template void ProcessElement::computeIntegerMAC<int32_t, int32_t, true>();
template void ProcessElement::computeIntegerMAC<int32_t, int64_t, true>();
template void ProcessElement::computeIntegerMAC<uint64_t, uint64_t, true>();
template void ProcessElement::computeIntegerMAC<int64_t, int64_t, true>();

// === 浮点数MAC计算 ===
template<typename SrcT, typename DestT, uint8_t wideningFactor>
void ProcessElement::computeFloatMAC()
{
    // 浮点运算，elements是结构体
    bool isFloat = true;
    auto elementsA = unpackOperands<SrcT>(dataBufferA_, isFloat);
    auto elementsB = unpackOperands<SrcT>(dataBufferB_, isFloat);

    DestT sum;
    sum.v = static_cast<decltype(DestT::v)>(0);

    for (int i = 0; i < TARGET_ELEMENTS; i++) {
        if constexpr(wideningFactor == 1)
        {
            // elements 元素已经是对应浮点struct类型，可以直接相乘
            DestT temp = fmul<DestT>(elementsA[i], elementsB[i]);
            sum = fadd<DestT>(sum, temp);
        }else if constexpr(wideningFactor == 2){
            DestT valA = fwiden(elementsA[i]);
            DestT valB = fwiden(elementsB[i]);
            DestT temp = fmul<DestT>(valA, valB);
            sum = fadd<DestT>(sum, temp);
        }else if constexpr(wideningFactor == 4){
            DestT valA = fquad(elementsA[i]);
            DestT valB = fquad(elementsB[i]);
            DestT temp = fmul<DestT>(valA, valB);
            sum = fadd<DestT>(sum, temp);
        }
    }
    DestT result = fadd<DestT>
        (sum, ftype<DestT>(static_cast<decltype(DestT::v)>(*outputRegister_)));
    // 浮点运算通常不需要饱和处理
    *outputRegister_ = static_cast<__uint128_t>(result.v);
}
template void ProcessElement::computeFloatMAC<float16_t, float16_t, 1>();
template void ProcessElement::computeFloatMAC<float16_t, float32_t, 2>();
template void ProcessElement::computeFloatMAC<float16_t, float64_t, 4>();
template void ProcessElement::computeFloatMAC<float32_t, float32_t, 1>();
template void ProcessElement::computeFloatMAC<float32_t, float64_t, 2>();
template void ProcessElement::computeFloatMAC<float64_t, float64_t, 1>();


bool ProcessElement::isFloatingPoint(MatDataType type) const
{
    return type == MatDataType::Float16 ||
           type == MatDataType::BFloat16 ||
           type == MatDataType::Float32 ||
           type == MatDataType::Float64;
}

bool ProcessElement::isSignedType(MatDataType type) const
{
    return type == MatDataType::Int8 ||
           type == MatDataType::Int16 ||
           type == MatDataType::Int32 ||
           type == MatDataType::Int64;
}

size_t ProcessElement::getTypeBitWidth(MatDataType type) const
{
    switch (type) {
        case MatDataType::UInt8:
        case MatDataType::Int8:
            return 8;
        case MatDataType::UInt16:
        case MatDataType::Int16:
        case MatDataType::Float16:
        case MatDataType::BFloat16:
            return 16;
        case MatDataType::UInt32:
        case MatDataType::Int32:
        case MatDataType::Float32:
            return 32;
        case MatDataType::UInt64:
        case MatDataType::Int64:
        case MatDataType::Float64:
            return 64;
        default:
            return 32;
    }
}

// === 调试打印状态 ===
void ProcessElement::debugPrintState() const
{
    // 调试输出，可用于验证逻辑
    // todo：目前暂时未设置，可按需设置
}
} // namespace gem5
