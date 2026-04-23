/**
 * @file systolic_array_core.cc
 * @brief 脉动阵列核心类实现
 * @author ztt
 */
#include "cpu/minor/systolicArray/systolic_array_core.hh"

#include <algorithm>

#include "cpu/minor/exec_context.hh"
#include "cpu/minor/func_unit.hh"
#include "debug/SystolicArrayCore.hh"

namespace gem5
{

SystolicArrayCore::SystolicArrayCore(const SystolicArrayCoreParams &params)
    : SimObject(params),
      arrayRows_(params.arrayRows),
      arrayCols_(params.arrayCols),
      peOpDelay_(params.opLat),
      peRowsPerTile_(params.peRowsPerTile),
      peColsPerTile_(params.peColsPerTile),
      isConfigured_(false)
{
    tileRows_ = (arrayRows_ + peRowsPerTile_ - 1) / peRowsPerTile_;
    tileCols_ = (arrayCols_ + peColsPerTile_ - 1) / peColsPerTile_;

    // 创建数据矩阵
    horizontalMatrix_ = std::make_unique<HorizontalInputMatrix>
        (this->name() + ".HorizontalInputMatrix", tileRows_, tileCols_);
    verticalMatrix_ = std::make_unique<VerticalInputMatrix>
        (this->name() + ".VerticalInputMatrix", arrayRows_, arrayCols_);
    outputMatrix_ = std::make_unique<OutputMatrix>
        (this->name() + ".OutputMatrix", arrayRows_, arrayCols_);
    /*
    // 数据矩阵的尺寸目前是tile rows = arrary rows下的情况
    // 更加准确的描述是：
    horizontalMatrix_ =
        std::make_unique<HorizontalInputMatrix>(arrayRows_, tileCols_);
    verticalMatrix_ =
        std::make_unique<VerticalInputMatrix>(tileRows_, arrayCols_);
    */
    // 初始化PE阵列（创建Tile）
    initializePEArray();

    // 初始化部分成员变量
    horizontalInputBuffer_.resize(arrayCols_);
    verticalInputBuffer_.resize(arrayRows_);
    accRowIndex_ = 0;
    currentCycle_ = 0;
    accFinished_ = false;

    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::SystolicArrayCore] "
        "Created %ux%u array with PE delay %u\n",
        arrayRows_, arrayCols_, peOpDelay_);
}

void SystolicArrayCore::acceptInstruction(
        minor::MinorDynInstPtr &inst, ExecContextPtr &xc){
    currMinorDynInst_ = inst;
    currInst_ = dynamic_cast<gem5::RiscvISA::MatrixArithMmaInst*>
        (inst->staticInst.get());
    machInst_ = inst->staticInst->getEMI();
    // 解析指令，获取数据类型、是否饱和、位宽扩展因子等
    
    computeSrcType(currInst_->fp, currInst_->sn, currInst_->eew);
    computeDestType(currInst_->fp,
                    currInst_->sn,
                    currInst_->eew,
                    currInst_->widen);
    saturationEnabled_ = currInst_->sa;
    wideningFactor_ = currInst_->widen * 2;// widen:0,1,2 -> 扩展倍数:0,2,4
    isFloat_ = (currInst_->fp != 0);
    computedM_ = currInst_->mtilem;
    computedN_ = currInst_->mtilen;
    computedK_ = currInst_->mtilek;
    computedTileRows_ = (computedM_ + peRowsPerTile_ - 1) / peRowsPerTile_;
    computedTileCols_ = (computedN_ + peColsPerTile_ - 1) / peColsPerTile_;
    // 配置所有PE
    configurePE(srcType_, destType_, saturationEnabled_, wideningFactor_);
    // 根据指令，获取输入、输出寄存器的指针
    /*
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::acceptInstruction] Before getRegOperand\n");
    */

    xc->getRegOperand(currInst_, 1, &tmp_s1);
    xc->getRegOperand(currInst_, 2, &tmp_s2);
    tmp_d0 = (RiscvISA::MatRegContainer *)xc->
        getWritableRegOperand(currInst_, 0);
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::acceptInstruction] "
        "Accepted instruction %s\n", machInst_);
};

void SystolicArrayCore::initializePEArray()
{
    // 二维向量，先调整行数
    tileArray_.resize(tileRows_);

    for (uint32_t tileRow = 0; tileRow < tileRows_; tileRow++) {
        // 为每行预留列空间
        tileArray_[tileRow].reserve(tileCols_);

        for (uint32_t tileCol = 0; tileCol < tileCols_; tileCol++) {
            // 计算当前Tile在原始阵列中的起始行
            uint32_t startRow = tileRow * peRowsPerTile_;
            uint32_t endRow = std::min(startRow + peRowsPerTile_, arrayRows_);
            uint32_t tileRowsActual = endRow - startRow;

            // 计算当前Tile在原始阵列中的起始列
            uint32_t startCol = tileCol * peColsPerTile_;
            uint32_t endCol = std::min(startCol + peColsPerTile_, arrayCols_);
            uint32_t tileColsActual = endCol - startCol;

            // 为Tile分配数据指针 - 需要处理多行情况
            // 单行情况下，每个tile的水平输入指向横向矩阵的对应位置
            // todo：如果要多行，这里需要像下面多列情况一样处理
            uint64_t* horizontalPtr =
                &(horizontalMatrix_->getData(tileRow, tileCol));

            // 为每个PE分配verticalDataB指针数组（考虑多行多列）
            // 多列情况下，需要取出tileColsActual个指针
            uint64_t** verticalPtrs = new uint64_t*[tileColsActual];
            __uint128_t** outputPtrs = new __uint128_t*[tileColsActual];

            // 遍历Tile内的所有PE位置（考虑单行情况）
            for (uint32_t localCol = 0;
                 localCol < tileColsActual; localCol++) {
                uint32_t globalCol = startCol + localCol;
                verticalPtrs[localCol] =
                    &(verticalMatrix_->getData(startRow, globalCol));
                outputPtrs[localCol] =
                    &(outputMatrix_->getData(startRow, globalCol));
            }

            // 创建Tile（现在支持单行多列）
            auto tile = std::make_unique<SystolicTile>(
                this->name() + ".SystolicTile["
                             + std::to_string(tileRow)
                             + "]["
                             + std::to_string(tileCol) + "]",
                peOpDelay_, tileRowsActual, tileColsActual,
                horizontalPtr, verticalPtrs, outputPtrs);

            // 调整：使用二维索引添加
            tileArray_[tileRow].push_back(std::move(tile));

            // 清理临时指针数组
            delete[] verticalPtrs;
            delete[] outputPtrs;
        }
    }
    dataFinishedMatrix_.resize(tileRows_, std::vector<bool>(tileCols_, false));
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::initializePEArray] "
        "Created %ux%u tile grid\n",
        tileRows_, tileCols_);
}

void SystolicArrayCore::computeSrcType(uint8_t fp, uint8_t sn, uint8_t eew)
{
    srcType_ = No_MatDataType;

    if (fp == 0) {  // 整数类型
        switch (eew) {
            case 0:  // 8位
                srcType_ = (sn == 0) ? UInt8 : Int8;
                break;
            case 1:  // 16位
                srcType_ = (sn == 0) ? UInt16 : Int16;
                break;
            case 2:  // 32位
                srcType_ = (sn == 0) ? UInt32 : Int32;
                break;
            case 3:  // 64位
                srcType_ = (sn == 0) ? UInt64 : Int64;
                break;
            default:
                panic("SystolicArrayCore: "
                       "Invalid eew value %u for integer type", eew);
        }
    } else {  // 浮点类型
        switch (eew) {
            case 0:  // 16位浮点
                srcType_ = Float16;
                break;
            case 1:  // 32位浮点
                srcType_ = Float32;
                break;
            case 2:  // 64位浮点
                srcType_ = Float64;
                break;
            default:
                panic("SystolicArrayCore: "
                       "Invalid eew value %u for floating-point type", eew);
        }
    }

    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::computeSrcType] "
        "fp=%u, sn=%u, eew=%u -> srcType=%u\n",
        fp, sn, eew, srcType_);
}

void SystolicArrayCore::computeDestType(
    uint8_t fp, uint8_t sn, uint8_t eew, uint8_t widen)
{
    destType_ = No_MatDataType;
    uint8_t destWidth = eew + widen;  // 目标位宽 = 源位宽 + 扩展因子

    if (fp == 0) {  // 整数类型
        if (destWidth > 3) {
            panic("SystolicArrayCore: "
                   "Invalid widened integer width %u", destWidth);
        }

        switch (destWidth) {
            case 0:  // 8位
                destType_ = (sn == 0) ? UInt8 : Int8;
                break;
            case 1:  // 16位
                destType_ = (sn == 0) ? UInt16 : Int16;
                break;
            case 2:  // 32位
                destType_ = (sn == 0) ? UInt32 : Int32;
                break;
            case 3:  // 64位
                destType_ = (sn == 0) ? UInt64 : Int64;
                break;
            default:
                panic("SystolicArrayCore: "
                       "Invalid destWidth %u for integer type", destWidth);
        }
    } else {  // 浮点类型
        if (destWidth > 2) {
            panic("SystolicArrayCore: "
                   "Invalid widened floating-point width %u", destWidth);
        }

        switch (destWidth) {
            case 0:  // 16位浮点
                destType_ = Float16;
                break;
            case 1:  // 32位浮点
                destType_ = Float32;
                break;
            case 2:  // 64位浮点
                destType_ = Float64;
                break;
            default:
                panic("SystolicArrayCore: "
                       "Invalid destWidth %u for floating-point type",
                       destWidth);
        }
    }

    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::computeDestType] "
        "fp=%u, sn=%u, eew=%u, widen=%u -> destType=%u\n",
        fp, sn, eew, widen, destType_);
}

void SystolicArrayCore::configurePE(MatDataType srcType, MatDataType destType,
                              bool saturationEnabled, uint8_t wideningFactor)
{
    // 在配置开始时就计算垂直索引步长
    dispatch_mat_type(srcType, [&](auto wrapper_tag, auto actual_tag) {
        using ActualType = decltype(actual_tag);
        constexpr int bytes_per_element = static_cast<int>(sizeof(ActualType));
        constexpr int bits_per_element = bytes_per_element * 8;
        elementsPer64bit_ = 64 / bits_per_element;

        // 静态断言确保位宽整除
        static_assert(64 % bits_per_element == 0,
                     "64 bit must be divisible by element bit width");

        DPRINTF(SystolicArrayCore, "[SystolicArrayCore::configurePE] "
                "srcType=%d, bits_per_element=%d, elementsPer64bit_=%d\n",
                static_cast<int>(srcType),
                bits_per_element,
                elementsPer64bit_);
    });

    // 调整：配置所有Tile（双重循环）
    for (auto& tileRow : tileArray_) {
        for (auto& tile : tileRow) {
            tile->configure
                (srcType, destType, saturationEnabled, wideningFactor);
        }
    }

    isConfigured_ = true;
    horizontalInputIndex_ = 0;
    verticalInputIndex_ = 0;
    accRowIndex_ = 0;
    currentCycle_ = 0;
    accFinished_ = false;

    horizontalInputBuffer_.assign(arrayRows_, 0);
    verticalInputBuffer_.assign(arrayCols_, 0);

    // 初始化数据结束标志矩阵(超出计算范围的行标记为结束)
    for (uint32_t row = 0; row < arrayRows_; row++) {
        for (uint32_t col = 0; col < arrayCols_; col++) {
            if (row >= computedM_ || col >= computedN_)
                dataFinishedMatrix_[row][col] = true;
            else
                dataFinishedMatrix_[row][col] = false;
        }
    }
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::configure] Array configured\n");
}

void SystolicArrayCore::loadHorizontalData(uint32_t col_index)
{
    auto ms1 = tmp_s1.as<uint64_t>();
    int32_t local_col_index = col_index;
    for (uint32_t row = 0; row < arrayRows_; row++) {
        if (local_col_index < 0) {
            horizontalInputBuffer_[row] = 0; // 填充零
        } else {
            if (local_col_index < computedK_)
                horizontalInputBuffer_[row] = ms1[row][local_col_index];
            else{
                horizontalInputBuffer_[row] = 0; // 超出范围填充零
                dataFinishedMatrix_[row][0] = true;
            }
            local_col_index = local_col_index - 1;
        }
    }
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::loadHorizontalData] "
        "Loaded data to column %u\n",
        col_index);
}

void SystolicArrayCore::loadVerticalData(uint32_t row_index)
{
    dispatch_mat_type(srcType_, [&](auto wrapper_tag, auto actual_tag) {
        using WrapperType = decltype(wrapper_tag);
        using ActualType = decltype(actual_tag);
        auto ms2 = tmp_s2.as<ActualType>();
        // 希望后续浮点用实际类型
        // 编译期计算：确定需要多少行数据来组成64位
        constexpr int bytes_per_element = static_cast<int>(sizeof(ActualType));
        constexpr int bits_per_element = bytes_per_element * 8;
        constexpr int elements_per_64bit = 64 / bits_per_element;


        static_assert(64 % bits_per_element == 0,
                     "64 bit must be divisible by element bit width");

        int32_t local_row_index = row_index;
        for (uint32_t col = 0; col < arrayCols_; col++) {
            if (local_row_index < 0) {
                verticalInputBuffer_[col] = 0; // 填充零
            } else {
                if (local_row_index < computedK_) {
                    // 组合多行数据成64位
                    uint64_t combined_value = 0;

                    // 从当前行开始，取elements_per_64bit行数据
                    for (size_t row_offset = 0;
                         row_offset < elements_per_64bit; row_offset++) {
                        int32_t current_row = local_row_index + row_offset;
                        DPRINTF(SystolicArrayCore,
                            "[SystolicArrayCore::loadVerticalData] "
                            "current_row=%d, col=%d\n", current_row, col);
                        if (current_row >= 0 && current_row < computedK_) {
                            // 获取当前数据并移位到正确位置
                            ActualType current_data = ms2[current_row][col];
                            DPRINTF(SystolicArrayCore,
                                "[SystolicArrayCore::loadVerticalData] "
                                "current_data=0x%016llX\n", current_data);
                            uint64_t shifted_data =
                                static_cast<uint64_t>(current_data)
                                << (row_offset * bits_per_element);
                            combined_value |= shifted_data;
                            DPRINTF(SystolicArrayCore,
                                "[SystolicArrayCore::loadVerticalData] "
                                "combined_value=0x%016llX\n", combined_value);
                        }
                        // 如果行索引无效，该位置保持为0（已初始化）
                    }

                    verticalInputBuffer_[col] = combined_value;
                } else {
                    verticalInputBuffer_[col] = 0; // 超出范围填充零
                }
                DPRINTF(SystolicArrayCore,
                    "[SystolicArrayCore::loadVerticalData] "
                    "Loaded data to row %u, col %u, "
                    "combined_value=0x%016llX\n",
                    local_row_index, col, verticalInputBuffer_[col]);
                // 由于tile在包含4列pe，所以这里应该是每4列，数据后移
                if (col % peColsPerTile_ == peColsPerTile_ - 1)
                    local_row_index = local_row_index - elements_per_64bit;
            }
        }

        DPRINTF(SystolicArrayCore,
               "[SystolicArrayCore::loadVerticalData] "
               "Loaded data to row %u, elements_per_64bit=%d\n",
               row_index, elements_per_64bit);
    });
}


void SystolicArrayCore::updateDataFinishedMatrix()
{
    for (int32_t row = computedM_-1; row >= 0; row--) {
        for (int32_t col = computedN_-1; col >= 0; col--) {
            if (row > 0 && col > 0)
                dataFinishedMatrix_[row][col] =
                    dataFinishedMatrix_[row-1][col]
                    && dataFinishedMatrix_[row][col-1];
            else if (row > 0 && col == 0)
                dataFinishedMatrix_[row][col] =
                    dataFinishedMatrix_[row-1][col];
            else if (col > 0 && row == 0)
                dataFinishedMatrix_[row][col] =
                    dataFinishedMatrix_[row][col-1];
        }
    }
}

void SystolicArrayCore::accumulateOutput(uint32_t row_index)
{
    // auto outputRow = outputMatrix_->getRowData(row_index);
    // output矩阵是累加的结果，已经都转成了uint64_t格式
    // 寄存器中的数据需要先as转换一下,但是类型必须到具体的模板才能知道，所以要在模板中指定
    // 根据源和目标类型选择计算路径
    switch (srcType_) {
        // 整数类型计算
        case MatDataType::UInt8:
            if (destType_ == MatDataType::UInt8) {
                computeIntegerACC<uint8_t, uint8_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::UInt16) {
                computeIntegerACC<uint8_t, uint16_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::UInt32) {
                computeIntegerACC<uint8_t, uint32_t>
                    (row_index, saturationEnabled_);
            }
            break;

        case MatDataType::Int8:
            if (destType_ == MatDataType::Int8) {
                computeIntegerACC<int8_t, int8_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::Int16) {
                computeIntegerACC<int8_t, int16_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::Int32) {
                computeIntegerACC<int8_t, int32_t>
                    (row_index, saturationEnabled_);
            }
            break;

        case MatDataType::UInt16:
            if (destType_ == MatDataType::UInt16) {
                computeIntegerACC<uint16_t, uint16_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::UInt32) {
                computeIntegerACC<uint16_t, uint32_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::UInt64) {
                computeIntegerACC<uint16_t, uint64_t>
                    (row_index, saturationEnabled_);
            }
            break;

        case MatDataType::Int16:
            if (destType_ == MatDataType::Int16) {
                computeIntegerACC<int16_t, int16_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::Int32) {
                computeIntegerACC<int16_t, int32_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::Int64) {
                computeIntegerACC<int16_t, int64_t>
                    (row_index, saturationEnabled_);
            }
            break;

        case MatDataType::UInt32:
            if (destType_ == MatDataType::UInt32) {
                computeIntegerACC<uint32_t, uint32_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::UInt64) {
                computeIntegerACC<uint32_t, uint64_t>
                    (row_index, saturationEnabled_);
            }
            break;

        case MatDataType::Int32:
            if (destType_ == MatDataType::Int32) {
                computeIntegerACC<int32_t, int32_t>
                    (row_index, saturationEnabled_);
            } else if (destType_ == MatDataType::Int64) {
                computeIntegerACC<int32_t, int64_t>
                    (row_index, saturationEnabled_);
            }
            break;

        case MatDataType::UInt64:
            if (destType_ == MatDataType::UInt64) {
                computeIntegerACC<uint64_t, uint64_t>
                    (row_index, saturationEnabled_);
            }
            break;

        case MatDataType::Int64:
            if (destType_ == MatDataType::Int64) {
                computeIntegerACC<int64_t, int64_t>
                    (row_index, saturationEnabled_);
            }
            break;

        // 浮点数类型计算
        case MatDataType::Float16:
            if (destType_ == MatDataType::Float16) {
                computeFloatACC<float16_t, float16_t>(row_index);
                // 使用bfloat16_t表示bfloat16位模式
            } else if (destType_ == MatDataType::Float32) {
                computeFloatACC<float16_t, float32_t>(row_index);
            } else if (destType_ == MatDataType::Float64) {
                computeFloatACC<float16_t, float64_t>(row_index);
            }
            break;
        // 理论上要支持bfloat16，暂时先用float16代替
        case MatDataType::BFloat16:
            if (destType_ == MatDataType::BFloat16) {
                computeFloatACC<float16_t, float16_t>(row_index);
            } else if (destType_ == MatDataType::Float32) {
                computeFloatACC<float16_t, float32_t>(row_index);
            } else if (destType_ == MatDataType::Float64) {
                computeFloatACC<float16_t, float64_t>(row_index);
            }
            break;

        case MatDataType::Float32:
            if (destType_ == MatDataType::Float32) {
                computeFloatACC<float32_t, float32_t>(row_index);
            } else if (destType_ == MatDataType::Float64) {
                computeFloatACC<float32_t, float64_t>(row_index);
            }
            break;

        case MatDataType::Float64:
            if (destType_ == MatDataType::Float64) {
                computeFloatACC<float64_t, float64_t>(row_index);
            }
            break;

        default:
            //todo
            break;
    }

}

template<typename SrcT, typename DestT>
void SystolicArrayCore::computeIntegerACC(
    uint32_t row_index, bool saturationEnabled)
{
    auto md = (*tmp_d0).as<DestT>();
    for (uint32_t i = 0; i < computedN_; i++) {
        DestT result;
        if (saturationEnabled) {
            // 饱和运算：使用宽类型检测溢出
            using WideT = std::conditional_t<
                std::is_signed_v<DestT>, __int128_t, __uint128_t>;

            WideT wide_result = static_cast<WideT>(md[row_index][i]) +
                static_cast<WideT>(outputMatrix_->getData(row_index, i));
            DPRINTF(SystolicArrayCore,
                "[SystolicArrayCore::computeIntegerACC] "
                "SATURATION: Starting saturation check, "
                "sum=%lld, saturationEnabled=%d\n",
                static_cast<long long>(wide_result), saturationEnabled);
            // 检查是否超出DestT的范围
            auto tmax = std::numeric_limits<DestT>::max();
            auto tlow = std::numeric_limits<DestT>::lowest();
            DPRINTF(SystolicArrayCore,
                "[SystolicArrayCore::computeIntegerACC] "
                "SATURATION: Input value=%lld, tmax=%lld, tlow=%lld\n",
                static_cast<long long>(wide_result),
                static_cast<long long>(tmax),
                static_cast<long long>(tlow));

            if (wide_result > static_cast<WideT>(tmax)) {
                result = tmax;
                DPRINTF(SystolicArrayCore,
                    "[SystolicArrayCore::computeIntegerACC] "
                    "SATURATION: Overflow detected, result set to max=%lld\n",
                    static_cast<long long>(result));
            } else if (wide_result < static_cast<WideT>(tlow)) {
                result = tlow;
                DPRINTF(SystolicArrayCore,
                    "[SystolicArrayCore::computeIntegerACC] "
                    "SATURATION: Underflow detected, result set to min=%lld\n",
                    static_cast<long long>(result));
            } else {
                result = static_cast<DestT>(wide_result);
            }

        } else {
            // 非饱和运算：直接使用DestT进行包装算术
            result = md[row_index][i] +
                    static_cast<DestT>(outputMatrix_->getData(row_index, i));
            // 这里会发生自然的溢出截断，这是正确的行为
            DPRINTF(SystolicArrayCore,
                "[SystolicArrayCore::computeIntegerACC] "
                "SATURATION: No saturation applied, result=%lld\n",
                static_cast<long long>(result));
        }
        outputMatrix_->setData(row_index, i, static_cast<__uint128_t>(result));
    }

}
// 显示实例化模板类
template void SystolicArrayCore::computeIntegerACC<uint8_t, uint8_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<uint8_t, uint16_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<uint8_t, uint32_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int8_t, int8_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int8_t, int16_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int8_t, int32_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<uint16_t, uint16_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<uint16_t, uint32_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<uint16_t, uint64_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int16_t, int16_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int16_t, int32_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int16_t, int64_t>
    (uint32_t row_index, bool saturationEnabled);

template void SystolicArrayCore::computeIntegerACC<uint32_t, uint32_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<uint32_t, uint64_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int32_t, int32_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int32_t, int64_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<uint64_t, uint64_t>
    (uint32_t row_index, bool saturationEnabled);
template void SystolicArrayCore::computeIntegerACC<int64_t, int64_t>
    (uint32_t row_index, bool saturationEnabled);

template<typename SrcT, typename DestT>
void SystolicArrayCore::computeFloatACC(uint32_t row_index)
{
    using tt =decltype(DestT::v);
    auto md = (*tmp_d0).as<tt>();
    DestT result;
    for (uint32_t i = 0; i < computedN_; i++) {
        DestT valA = ftype<DestT>(md[row_index][i]);
        DestT valB = ftype<DestT>
            (static_cast<tt>(outputMatrix_->getData(row_index, i)));
        result = fadd<DestT>(valA, valB);
        outputMatrix_->setData
            (row_index, i, static_cast<__uint128_t>(result.v));
    }
}
// 显示实例化模板类
template void SystolicArrayCore::computeFloatACC<float16_t, float16_t>
    (uint32_t row_index);
template void SystolicArrayCore::computeFloatACC<float16_t, float32_t>
    (uint32_t row_index);
template void SystolicArrayCore::computeFloatACC<float16_t, float64_t>
    (uint32_t row_index);
template void SystolicArrayCore::computeFloatACC<float32_t, float32_t>
    (uint32_t row_index);
template void SystolicArrayCore::computeFloatACC<float32_t, float64_t>
    (uint32_t row_index);
template void SystolicArrayCore::computeFloatACC<float64_t, float64_t>
    (uint32_t row_index);

void SystolicArrayCore::advance()
{
    // 检查阵列是否已配置，未配置就advance则报错
    if (!isConfigured_) {
        panic("SystolicArrayCore: Array not configured before advance");
    }
    if (isMacFinished()){
        // 表示脉动阵列的计算结束，应该进入累加过程
        if (accRowIndex_ < computedM_){
            // 累加未完成，执行累加操作
            accumulateOutput(accRowIndex_);
            accRowIndex_++;
            DPRINTF(SystolicArrayCore,
                "[SystolicArrayCore::advance] "
                "Accumulation step for row %u completed\n",
                accRowIndex_);
        }else{
            // 累加完成，标记累加结束
            accFinished_ = true;
        }
    }else{
        // 进行脉动阵列计算过程
        // 1. 更新数据矩阵（脉动流动）
        // 首先将矩阵寄存器中的数据更新到相应的阵列矩阵的输入buffer中
        loadHorizontalData(horizontalInputIndex_);
        loadVerticalData(verticalInputIndex_);
        // 然后将阵列矩阵的输入buffer更新到矩阵中
        horizontalMatrix_->advance(horizontalInputBuffer_);
        verticalMatrix_->advance(verticalInputBuffer_);

        // 2. 推进所有Tile
        for (uint32_t tileRow = 0; tileRow < tileRows_; tileRow++) {
            for (uint32_t tileCol = 0; tileCol < tileCols_; tileCol++) {
                tileArray_[tileRow][tileCol]->
                    advance(dataFinishedMatrix_[tileRow][tileCol]);
            }
        }

        // 3. 输出矩阵保持（或根据需要更新）
        outputMatrix_->advance();

        // 4. 更新数据结束标志矩阵
        updateDataFinishedMatrix();

        // 5. 更新下一个周期的输入
        horizontalInputIndex_++;
        // 垂直索引不是+1，而是+64/数据位宽
        verticalInputIndex_ += elementsPer64bit_;
    }

}

void SystolicArrayCore::reset()
{
    horizontalMatrix_->reset();
    verticalMatrix_->reset();
    outputMatrix_->reset();

    for (auto& tileRow : tileArray_) {
        for (auto& tile : tileRow) {
            tile->reset();
        }
    }

    isConfigured_ = false;
    horizontalInputBuffer_.assign(arrayRows_, 0);
    verticalInputBuffer_.assign(arrayCols_, 0);
    horizontalInputIndex_ = 0;
    verticalInputIndex_ = 0;
    dataFinishedMatrix_.resize(tileRows_, std::vector<bool>(tileCols_, false));

    DPRINTF(SystolicArrayCore, "[SystolicArrayCore::reset] Array reset\n");
}

std::vector<__uint128_t> SystolicArrayCore::getOutputRow(uint32_t row) const
{
    if (row >= arrayRows_) {
        panic("SystolicArrayCore: Row %u out of bounds", row);
    }

    return outputMatrix_->getRowData(row);
}

std::vector<__uint128_t> SystolicArrayCore::getOutputColumn(uint32_t col) const
{
    if (col >= arrayCols_) {
        panic("SystolicArrayCore: Column %u out of bounds", col);
    }

    return outputMatrix_->getColData(col);
}

__uint128_t SystolicArrayCore::getOutput(uint32_t row, uint32_t col) const
{
    if (row >= arrayRows_ || col >= arrayCols_) {
        panic("SystolicArrayCore: Position (%u, %u) out of bounds", row, col);
    }

    return outputMatrix_->getData(row, col);
}

template<typename T>
void SystolicArrayCore::writeBackInt(
    uint32_t row, uint32_t col, __uint128_t data)
{
    auto md = (*tmp_d0).as<T>();
    md[row][col] = static_cast<T>(data);
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::writeBackInt] "
        "Wrote back data 0x%016lx to (%u, %u)\n",
        static_cast<long long>(data), row, col);
}
template void SystolicArrayCore::writeBackInt<uint8_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackInt<int8_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackInt<uint16_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackInt<int16_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackInt<uint32_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackInt<int32_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackInt<uint64_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackInt<int64_t>
    (uint32_t row, uint32_t col, __uint128_t data);

template<typename T>
void SystolicArrayCore::writeBackFloat(
    uint32_t row, uint32_t col, __uint128_t data)
{
    using tt = decltype(T::v);

    auto md = (*tmp_d0).as<tt>();
    md[row][col] = static_cast<tt>(data);
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::writeBackFloat] "
        "Wrote back data 0x%016lx to (%u, %u)\n",
        static_cast<long long>(data), row, col);
}
template void SystolicArrayCore::writeBackFloat<float16_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackFloat<float32_t>
    (uint32_t row, uint32_t col, __uint128_t data);
template void SystolicArrayCore::writeBackFloat<float64_t>
    (uint32_t row, uint32_t col, __uint128_t data);

void SystolicArrayCore::writeBackOutput()
{
    if (!accFinished_){
        panic("SystolicArrayCore: "
              "writeBackOutput called before accumulation finished");
    }
    // 将输出矩阵的数据写回主内存
    for (uint32_t row = 0; row < computedM_; row++) {
        for (uint32_t col = 0; col < computedN_; col++) {
            __uint128_t data = outputMatrix_->getData(row, col);
            switch(destType_){
                case MatDataType::UInt8:
                    writeBackInt<uint8_t>(row, col, data);
                    break;
                case MatDataType::Int8:
                    writeBackInt<int8_t>(row, col, data);
                    break;
                case MatDataType::UInt16:
                    writeBackInt<uint16_t>(row, col, data);
                    break;
                case MatDataType::Int16:
                    writeBackInt<int16_t>(row, col, data);
                    break;
                case MatDataType::UInt32:
                    writeBackInt<uint32_t>(row, col, data);
                    break;
                case MatDataType::Int32:
                    writeBackInt<int32_t>(row, col, data);
                    break;
                case MatDataType::UInt64:
                    writeBackInt<uint64_t>(row, col, data);
                    break;
                case MatDataType::Int64:
                    writeBackInt<int64_t>(row, col, data);
                    break;
                case MatDataType::BFloat16:
                    writeBackFloat<float16_t>(row, col, data);
                    break;
                case MatDataType::Float16:
                    writeBackFloat<float16_t>(row, col, data);
                    break;
                case MatDataType::Float32:
                    writeBackFloat<float32_t>(row, col, data);
                    break;
                case MatDataType::Float64:
                    writeBackFloat<float64_t>(row, col, data);
                    break;
                default:
                    panic("SystolicArrayCore: Unsupported MatDataType");
            }
        }
    }
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::writeBack] Write back data\n");
    if (currMinorDynInst_->traceData) {
        DPRINTF(SystolicArrayCore,
            "[SystolicArrayCore::writeBack] Write trace data\n");
             currMinorDynInst_->traceData->
                 setData(RiscvISA::matRegClass, tmp_d0);
    };
}

bool SystolicArrayCore::isIdle() const
{
    // 阵列空闲当且仅当所有PE都空闲
    // todo: 后续如果想要早点释放空闲资源，这里需要修改
    for (const auto& tileRow : tileArray_) {
        for (const auto& tile : tileRow) {
            if (!tile->isIdle()) {
                return false;
            }
        }
    }
    return true;
}

bool SystolicArrayCore::isComputing() const
{
    // 阵列正在计算当且仅当任一PE正在计算
    for (const auto& tileRow : tileArray_) {
        for (const auto& tile : tileRow) {
            if (tile->isComputing()) {
                return true;
            }
        }
    }
    return false;
}

bool SystolicArrayCore::isMacFinished() const
{
    // 仅当最后一个PE的输出有效时，整个阵列乘累加部分计算完成
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::isMacFinished::debug] enter \n");
    if (tileArray_[computedTileRows_-1][computedTileCols_-1]->isOutputValid())
        return true;
    return false;
}

bool SystolicArrayCore::isOutputValid() const
{
    // 仅当累加过程完成，整个阵列输出才有效
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::isOutputValid::debug] enter\n");
    if (accFinished_)
        return true;
    return false;
}
void SystolicArrayCore::printArrayState() const
{
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::printArrayState] "
        "Systolic Array %ux%u State (Cycle %u):\n",
        arrayRows_, arrayCols_, currentCycle_);

    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::printArrayState] "
        "Horizontal Data Matrix:\n");
    horizontalMatrix_->printMatrix();

    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::printArrayState] "
        "Vertical Data Matrix:\n");
    verticalMatrix_->printMatrix();

    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::printArrayState] "
        "Output Matrix:\n");
    outputMatrix_->printMatrix();
}

void SystolicArrayCore::printDataFlows() const
{
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::printDataFlows] "
        "Data Flows at Cycle %u:\n", currentCycle_);

    // 打印横向数据流
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::printDataFlows] "
        "Horizontal Flow (Left->Right):\n");
    for (uint32_t row = 0; row < arrayRows_; row++) {
        std::stringstream ss;
        ss << "  Row " << row << ": ";
        for (uint32_t col = 0; col < arrayCols_; col++) {
            ss << "0x" << std::hex <<
                horizontalMatrix_->getData(row, col) << " ";
        }
        DPRINTF(SystolicArrayCore, "%s\n", ss.str().c_str());
    }

    // 打印纵向数据流
    DPRINTF(SystolicArrayCore,
        "[SystolicArrayCore::printDataFlows] "
        "Vertical Flow (Top->Bottom):\n");
    for (uint32_t col = 0; col < arrayCols_; col++) {
        std::stringstream ss;
        ss << "  Col " << col << ": ";
        for (uint32_t row = 0; row < arrayRows_; row++) {
            ss << "0x" << std::hex <<
                verticalMatrix_->getData(row, col) << " ";
        }
        DPRINTF(SystolicArrayCore, "%s\n", ss.str().c_str());
    }
}

} // namespace gem5
