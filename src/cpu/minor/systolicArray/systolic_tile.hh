/**
 * @file systolic_tile.hh
 * @brief 脉动阵列的一个计算单元（Tile）
 *
 * 每个Tile包含多个PE（Process Element），用于执行矩阵乘法操作。
 * 每个PE负责处理一个元素的计算，而Tile则负责组织和管理这些PE。
 * @author ztt
 */
#ifndef __CPU_SYSTOLIC_ARRAY_TILE_HH__
#define __CPU_SYSTOLIC_ARRAY_TILE_HH__

#include <memory>
#include <vector>

#include "cpu/minor/systolicArray/process_element.hh"

namespace gem5
{

class SystolicTile : public Named
{
    public:
        SystolicTile(const std::string &name_, uint64_t opDelay,
                     uint32_t rows, uint32_t cols,
                     uint64_t* horizontalDataA, uint64_t** verticalDataB,
                     __uint128_t** output);
        ~SystolicTile() = default;

    public:
        // 配置接口
        void configure(MatDataType srcType, MatDataType destType,
                    bool saturationEnabled, uint8_t wideningFactor);

        // 数据输入接口
        // 横向数据输入（同行PE共享的dataA）
        void feedHorizontalData(uint64_t data);
        // 纵向数据输入（各PE独立的dataB）
        void feedVerticalData(uint64_t data, uint32_t col);

        // 控制接口
        void advance(bool isLastRound = false);

        // 重置接口
        void reset();

        // 结果输出
        __uint128_t getOutput(uint32_t col) const;

        // 状态查询
        bool isIdle() const;
        //bool canAcceptVerticalData(uint32_t col) const;
        //bool isComputing(uint32_t col) const;
        bool isComputing() const;
        bool isOutputValid() const;

        // 调试信息
        void printState() const;

    private:
        const uint32_t rows_;
        const uint32_t cols_;

        // PE阵列：按列存储，每列一个PE（1×4布局）
        std::vector<std::unique_ptr<ProcessElement>> peArray_;

        // 共享的横向数据指针（所有PE共用）
        uint64_t* horizontalDataA_;

        // 独立的纵向数据指针数组（每个PE一个）
        uint64_t** verticalDataB_;

        // 输出指针数组（每个PE一个）
        __uint128_t** output_;

        // 配置状态
        MatDataType srcType_;
        MatDataType destType_;
        bool saturationEnabled_;
        uint8_t wideningFactor_;
};

} // namespace gem5

#endif // __CPU_SYSTOLIC_ARRAY_TILE_HH__
