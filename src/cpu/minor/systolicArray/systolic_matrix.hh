#ifndef __CPU_SYSTOLIC_ARRAY_SYSTOLIC_MATRIX_HH__
#define __CPU_SYSTOLIC_ARRAY_SYSTOLIC_MATRIX_HH__

#include <cstdarg>
#include <cstdint>
#include <vector>

#include "base/logging.hh"
#include "base/named.hh"
#include "base/trace.hh"
#include "base/types.hh"

namespace gem5
{
// === 基类：脉动矩阵 ===
template<typename T>
class SystolicMatrixBase: public Named
{
public:
    SystolicMatrixBase(const std::string &name_, uint32_t rows, uint32_t cols);
    virtual ~SystolicMatrixBase() = default;

public:
    // === 基础操作 ===
    virtual void advance(const std::vector<T>& dataIn) = 0;
    virtual void advance() = 0;  // 纯虚函数，子类实现具体脉动逻辑
    void reset();

    // === 数据访问 ===
    T& getData(uint32_t row, uint32_t col);
    void setData(uint32_t row, uint32_t col, T data);

    // === 批量操作 ===
    void setRowData(uint32_t row, const std::vector<T>& data);
    void setColData(uint32_t col, const std::vector<T>& data);
    std::vector<T>& getRowData(uint32_t row);
    std::vector<T> getColData(uint32_t col);

    // === 状态查询 ===
    uint32_t getRows() const { return rows_; }
    uint32_t getCols() const { return cols_; }
    bool isValidPosition(uint32_t row, uint32_t col) const;

    // === 调试 ===
    virtual std::string getMatrixType() const = 0;
    void printMatrix() const;

protected:
    uint32_t rows_;
    uint32_t cols_;
    std::vector<std::vector<T>> matrix_;

    void validatePosition(uint32_t row, uint32_t col) const;
    void initializeMatrix();
};

typedef SystolicMatrixBase<uint64_t> SystolicMatrixBase64;
typedef SystolicMatrixBase<__uint128_t> SystolicMatrixBase128;

class HorizontalInputMatrix : public SystolicMatrixBase64
{
public:
    HorizontalInputMatrix(const std::string &name_,
                          uint32_t rows, uint32_t cols);
    ~HorizontalInputMatrix() = default;
public:
    // 重写advance：数据从左向右脉动
    void advance(const std::vector<uint64_t>& dataIn) override;
    void advance() override;

    // 横向矩阵特有接口：设置第一列（新输入）(实际未使用，advance即可)
    void setFirstColumn(const std::vector<uint64_t>& newData);

    // 获取矩阵类型
    std::string getMatrixType() const override { return "HorizontalInput"; }
private:
    using SystolicMatrixBase64::setColData; // 隐藏不合适的接口
};

class VerticalInputMatrix : public SystolicMatrixBase64
{
public:
    VerticalInputMatrix(const std::string &name_,
                        uint32_t rows, uint32_t cols);
    ~VerticalInputMatrix() = default;
public:
    // 重写advance：数据从上向下脉动
    void advance(const std::vector<uint64_t>& dataIn) override;
    void advance() override;

    // 纵向矩阵特有接口：设置第一行（新输入）（实际未使用，advance即可）
    void setFirstRow(const std::vector<uint64_t>& newData);

    // 获取矩阵类型
    std::string getMatrixType() const override { return "VerticalInput"; }
private:
    using SystolicMatrixBase64::setRowData; // 隐藏不合适的接口
};

class OutputMatrix : public SystolicMatrixBase128
{
public:
    OutputMatrix(const std::string &name_, uint32_t rows, uint32_t cols);
    ~OutputMatrix() = default;
public:
    // 重写advance：输出矩阵不需要脉动，从pe中获取新的输出
    void advance(const std::vector<__uint128_t>& dataIn) override;
    void advance() override;

    // 获取矩阵类型
    std::string getMatrixType() const override { return "Output"; }
};

} // namespace gem5

#endif // __CPU_SYSTOLIC_ARRAY_SYSTOLIC_MATRIX_HH__
