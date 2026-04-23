/**
 * @file systolic_tile.cc
 * @brief Tile的具体实现
 * @author ztt
 */
#include "cpu/minor/systolicArray/systolic_tile.hh"

#include "debug/SystolicTile.hh"

namespace gem5
{

SystolicTile::SystolicTile(const std::string &name_, uint64_t opDelay,
                             uint32_t rows, uint32_t cols,
                             uint64_t* horizontalDataA,
                             uint64_t** verticalDataB,
                             __uint128_t** output)
    : Named(name_),
      rows_(rows),
      cols_(cols),
      horizontalDataA_(horizontalDataA),
      verticalDataB_(verticalDataB),
      output_(output),
      srcType_(MatDataType::Int8),
      destType_(MatDataType::Int32),
      saturationEnabled_(false),
      wideningFactor_(1)
{
    // 验证尺寸：目前必须为1×4阵列
    // todo：列扩展应该比较简单，行扩展可能较复杂
    assert(rows_ == 1 && cols_ == 4);
    assert(horizontalDataA_ != nullptr);
    assert(verticalDataB_ != nullptr);
    assert(output_ != nullptr);
    // 初始化PE阵列（每列一个PE）
    for (uint32_t col = 0; col < cols_; col++) {
        // 使用外部传入的指针：共享的dataA，独立的dataB
        assert(verticalDataB_[col] != nullptr);

        peArray_.push_back(std::make_unique<ProcessElement>(
            this->name() + ".PE" + "[" + std::to_string(col) + "]",
            opDelay, horizontalDataA_, verticalDataB_[col], output_[col]));
    }

    DPRINTF(SystolicTile,
        "[SystolicTile::SystolicTile] Created 1×4 tile with %u columns\n",
        cols_);
    DPRINTF(SystolicTile,
        "[SystolicTile::SystolicTile] HorizontalDataA ptr=%p\n",
        horizontalDataA_);
    for (uint32_t col = 0; col < cols_; col++) {
        DPRINTF(SystolicTile,
            "[SystolicTile::SystolicTile] VerticalDataB[%u] ptr=%p\n",
            col, verticalDataB_[col]);
    }
}

void SystolicTile::configure(MatDataType srcType, MatDataType destType,
                            bool saturationEnabled, uint8_t wideningFactor)
{
    DPRINTF(SystolicTile,
        "[SystolicTile::configure] "
        "Configuring tile: "
        "srcType=%d, destType=%d, saturation=%d, widening=%d\n",
        static_cast<int>(srcType),
        static_cast<int>(destType),
        saturationEnabled,
        wideningFactor);

    srcType_ = srcType;
    destType_ = destType;
    saturationEnabled_ = saturationEnabled;
    wideningFactor_ = wideningFactor;

    // 配置所有PE
    for (auto& pe : peArray_) {
        pe->configure(srcType, destType, saturationEnabled, wideningFactor);
    }
}

void SystolicTile::feedHorizontalData(uint64_t data)
{
    // 更新共享的横向数据（所有PE都会看到这个更新）
    *horizontalDataA_ = data;

    DPRINTF(SystolicTile,
        "[SystolicTile::feedHorizontalData] "
        "Horizontal DataA=0x%016lx\n", data);
}

void SystolicTile::feedVerticalData(uint64_t data, uint32_t col)
{
    assert(col < cols_);

    // 更新指定列的纵向数据
    *verticalDataB_[col] = data;

    DPRINTF(SystolicTile,
        "[SystolicTile::feedVerticalData] "
        "Col=%u, Vertical DataB=0x%016lx\n",
        col, data);
}

void SystolicTile::advance(bool isLastRound)
{
    DPRINTF(SystolicTile,
        "[SystolicTile::advance] isLastRound=%d\n", isLastRound);

    // 推进所有PE
    for (uint32_t col = 0; col < cols_; col++) {
        peArray_[col]->advance(isLastRound);
    }

    // 打印状态用于调试
    printState();
}

__uint128_t SystolicTile::getOutput(uint32_t col) const
{
    assert(col < cols_);
    __uint128_t result = peArray_[col]->getOutput();
    DPRINTF(SystolicTile,
        "[SystolicTile::getOutput] Col=%u, Result=0x%016llx%016llx\n",
        col, (uint64_t)(result >> 64), (uint64_t)result);
    return result;
}

bool SystolicTile::isOutputValid() const
{
    DPRINTF(SystolicTile,
        "[SystolicTile::isOutputValid::Debug] enter\n");
    bool valid = true;
    for (uint32_t col = 0; col < cols_; col++) {
        DPRINTF(SystolicTile,
            "[SystolicTile::isOutputValid::Debug] enter col=%u\n", col);
        if (!peArray_[col]->isOutputValid()) {
            DPRINTF(SystolicTile,
                "[SystolicTile::isOutputValid::Debug] Col Invalid\n");
            valid = false;
            break;
        }
    }
    DPRINTF(SystolicTile,
        "[SystolicTile::isOutputValid] Valid=%d\n", valid);
    return valid;
}

void SystolicTile::reset()
{
    DPRINTF(SystolicTile, "[SystolicTile::reset] Resetting tile\n");

    // 重置共享数据
    *horizontalDataA_ = 0;

    // 重置独立数据
    for (uint32_t col = 0; col < cols_; col++) {
        *verticalDataB_[col] = 0;
        peArray_[col]->reset();
    }
}

bool SystolicTile::isIdle() const
{
    bool idle = true;
    for (uint32_t col = 0; col < cols_; col++) {
        if (!peArray_[col]->isIdle()) {
            idle = false;
            break;
        }
    }
    DPRINTF(SystolicTile, "[SystolicTile::isIdle] Idle=%d\n", idle);
    return idle;
}

bool SystolicTile::isComputing() const
{
    // 如果任一PE仍在计算，则认为整个Tile忙碌
    bool computing = false;
    for (const auto& pe : peArray_) {
        if (pe->isComputing()) {
            computing = true;
            break;
        }
    }
    DPRINTF(SystolicTile,
        "[SystolicTile::isComputing] Computing=%d\n", computing);
    return computing;
}

void SystolicTile::printState() const
{
#ifdef DEBUG
    DPRINTF(SystolicTile, "[SystolicTile::printState] Tile State:\n");
    DPRINTF(SystolicTile, "  Shared DataA=0x%016lx\n", *horizontalDataA_);
    for (uint32_t col = 0; col < cols_; col++) {
        DPRINTF(SystolicTile,
            "Col[%u]: DataB=0x%016lx, OutputValid=%d, "
            "Output=0x%016lx, Computing=%d\n",
            col, *verticalDataB_[col], peArray_[col]->isOutputValid(),
            peArray_[col]->getOutput(), peArray_[col]->isComputing());
    }
#endif
}

} // namespace gem5
