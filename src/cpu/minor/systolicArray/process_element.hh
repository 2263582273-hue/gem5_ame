/**
 * @file process_element.hh
 * @brief ProcessElement 类，用于 systolic array 中的计算单元
 * @author ztt
 */
#ifndef __CPU_SYSTOLIC_ARRAY_PE_HH__
#define __CPU_SYSTOLIC_ARRAY_PE_HH__

#include <cmath>
#include <cstdint>
#include <deque>
#include <iostream>
#include <limits>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

// 引入softfloat.hh文件，用于fp的计算库
#include <internals.h>
#include <softfloat.h>
#include <specialize.h>

#include "base/named.hh"
#include "base/trace.hh"

// 引入types.hh文件，用于定义矩阵数据类型
#include "cpu/minor/systolicArray/types.hh"

// 引入TimeBuffer所在的类
#include "cpu/timebuf.hh"
#include "debug/OutputBuffer.hh"

namespace gem5
{
/**
 * @brief OutputBuffer 类，用于 process element 中的输出缓冲区
 * @author ztt
 * Buffer不能基于MinorBuffer, 因为基本数据类型没有Bubble
 * 需要基于Timebuffer来构造
 */
template <typename T>
class OutputBuffer : public TimeBuffer<T>, public Named
{
    protected:
        typename TimeBuffer<T>::wire pushWire;
        typename TimeBuffer<T>::wire popWire;
        std::vector<bool> validFlags; // 用于标记每个位置的数据是否有效
    public:
        T output; // 存储输出数据
        unsigned int occupancy; // 当前有效数据数量
    public:
        OutputBuffer(const std::string &name, uint64_t opDelay) :
            TimeBuffer<T>(opDelay, 0),
            Named(name),
            pushWire(this->getWire(0)),
            popWire(this->getWire(-opDelay)),
            validFlags(opDelay + 1, false),
            occupancy(0)
        {
            DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                "[OutputBuffer::OutputBuffer] Constructor: opDelay=%d\n",
                opDelay);
        }
        ~OutputBuffer() = default;

    public:
        T &front(){
            return *popWire;
        }

        void push(T &data){
            *pushWire = data;
            validFlags[this->base] = true;
            occupancy++;
            DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                "[OutputBuffer::push] Data pushed: "
                "base=%d, occupancy=%u, validFlags[%d]=true\n",
                this->base, occupancy, this->base);
        };
        bool isPopable(){
            int oldestIndex = -static_cast<int>(this->past);
            int vectorIndex = this->calculateVectorIndex(oldestIndex);
            DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                "[OutputBuffer::isPopable] "
                "oldestIndex=%d, vectorIndex=%d, valid=%d\n",
                oldestIndex, vectorIndex, validFlags[vectorIndex]);
            return validFlags[vectorIndex];
        };
        bool advance(){
            DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                "[OutputBuffer::advance] "
                "Start: occupancy=%u, past=%u\n",
                occupancy, this->past);
            // 计算最老数据的索引（past个周期前写入的数据）
            int oldestIndex = -static_cast<int>(this->past);
            // 映射到实际存储位置（计算环形索引）
            int vectorIndex = this->calculateVectorIndex(oldestIndex);
            DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                "[OutputBuffer::advance] "
                "oldestIndex=%d, vectorIndex=%d, valid=%d\n",
                oldestIndex, vectorIndex, validFlags[vectorIndex]);
            // 检查是否有有效数据
            if (!validFlags[vectorIndex]) {
                TimeBuffer<T>::advance();
                DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                    "[OutputBuffer::advance] "
                    "No valid data at index %d\n", vectorIndex);
                return false;  // 无效，无输出
            }

            // 读取数据并清零有效性标记
            output = *popWire;
            validFlags[vectorIndex] = false;
            occupancy--;
            // 打印弹出数据(专用于128位数据，如果是其它Type需要修改)
            DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                "[OutputBuffer::advance] "
                "Data popped: "
                "index=%d, occupancy=%u, output=0x%016llx%016llx\n",
                vectorIndex, occupancy,
                (uint64_t)(output >> 64), (uint64_t)output);
            TimeBuffer<T>::advance();
            DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                "[OutputBuffer::advance] "
                "Advance Complete: new base=%d, new past=%u\n",
                this->base, this->past);
            return true;
        };

    public:
        // 暴露调整接口
        void setDelay(int new_delay) {
            DPRINTFS(OutputBuffer, static_cast<Named *>(this),
                "[OutputBuffer::setDelay] "
                "Start: old_delay=%d, new_delay=%d, occupancy=%u\n",
                static_cast<int>(this->past), new_delay, occupancy);
            // 调整父类缓冲区
            this->resize(new_delay, 0);
            // 重置有效性标记
            validFlags.assign(new_delay + 1, false);
            // 更新 wire 指针
            pushWire = this->getWire(0);
            popWire = this->getWire(-new_delay);
            occupancy = 0;
        }

};

/**
 * @brief ProcessElement 类，用于 systolic array 中的计算单元
 * @author ztt
 */
class ProcessElement : public Named
{
public:
    ProcessElement(const std::string &name_,
                   uint64_t opDelay,
                   uint64_t* dataA,
                   uint64_t* dataB,
                   __uint128_t* output);
    ~ProcessElement() = default;

public:
    // 配置接口
    void configure(MatDataType srcType, MatDataType destType,
                   bool saturationEnabled, uint8_t wideningFactor);

    // 数据输入
    void feedDataA(uint64_t* data);
    void feedDataB(uint64_t* data);

    // 推进计算
    void advance(bool dataFinished);

    // 结果输出
    __uint128_t getOutput() const;

    // 清零状态
    void reset();

    // 状态查询
    bool isIdle() const;
    bool isComputing() const;
    bool isOutputValid() const;

private:
    // 输出延迟buffer
    OutputBuffer<__uint128_t> outputBuffer_;

    // === 配置状态 ===
    MatDataType srcType_;
    MatDataType destType_;
    bool saturationEnabled_;
    uint8_t wideningFactor_;
    static constexpr uint32_t TARGET_ELEMENTS = 8; // 目标：一次处理8个元素的向量点积
    bool isConfigured_ = false;

    // === 数据输入 ===
    uint64_t* dataA_ = nullptr;
    uint64_t* dataB_ = nullptr;

    // === 数据收集状态 ===
    uint32_t elementsPerWord_;    // 每个64位字包含的元素数
    uint32_t wordsNeeded_;        // 需要收集的64位字数
    uint32_t wordsCollectedA_;    // 已收集的A数据字数
    uint32_t wordsCollectedB_;    // 已收集的B数据字数

    std::vector<uint64_t> dataBufferA_; // 数据收集缓冲区A
    std::vector<uint64_t> dataBufferB_; // 数据收集缓冲区B

    // === 计算状态 ===
    __uint128_t* outputRegister_;
    bool outputValid_; // 输出是否有效
    bool computeFinished_; // 计算是否完成

    int computeCycles_; // 计算所需的总周期数

    bool msat_; // 是否饱和

private:
    // === 内部辅助方法 ===
    // 计算数据收集参数
    void calculateDataCollectionParams();
    // 配置计算延迟
    void configureComputeDelay();

    // 解包操作数（从多个64位字）
    template<typename T>
    std::vector<T> unpackOperands(
        const std::vector<uint64_t>& packedData, bool isFloat) const;

    // 执行计算的核心方法
    void executeMACCalculation();

    // 类型特定的计算分发
    void dispatchCalculation();

    // 整数计算模板
    template<typename SrcT, typename DestT, bool saturationEnabled>
    void computeIntegerMAC();

    // 浮点计算模板
    template<typename SrcT, typename DestT, uint8_t wideningFactor>
    void computeFloatMAC();

    // 类型特性查询
    bool isFloatingPoint(MatDataType type) const;
    bool isSignedType(MatDataType type) const;
    size_t getTypeBitWidth(MatDataType type) const;

    // 调试信息
    void debugPrintState() const;
};

} // namespace gem5

#endif // __CPU_SYSTOLIC_ARRAY_PE_HH__
