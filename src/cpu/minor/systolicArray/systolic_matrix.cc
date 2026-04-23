/**
 * @file systolic_matrix.cc
 * @brief 脉动矩阵类实现
 * @author ztt
 */
#include "cpu/minor/systolicArray/systolic_matrix.hh"

#include <cassert>
#include <sstream>

#include "debug/SystolicMatrix.hh"

namespace gem5
{
// === 基础矩阵 ===
template<typename T>
SystolicMatrixBase<T>::SystolicMatrixBase(
    const std::string &name_, uint32_t rows, uint32_t cols)
    : Named(name_), rows_(rows), cols_(cols)
{
    initializeMatrix();
    /*
    DPRINTF(SystolicMatrix,
        "[SystolicMatrixBase::SystolicMatrixBase] Created %ux%u matrix\n",
        rows, cols);
    */
}

template<typename T>
void SystolicMatrixBase<T>::initializeMatrix()
{
    matrix_.resize(rows_);
    for (auto& row : matrix_) {
        row.resize(cols_, 0);
    }
}

template<typename T>
void SystolicMatrixBase<T>::validatePosition(uint32_t row, uint32_t col) const
{
    if (row >= rows_ || col >= cols_) {
        panic("SystolicMatrixBase: "
              "Invalid position (%u, %u), matrix size is %ux%u",
              row, col, rows_, cols_);
    }
}
template<typename T>
bool SystolicMatrixBase<T>::isValidPosition(uint32_t row, uint32_t col) const
{
    return (row < rows_ && col < cols_);
}

template<typename T>
void SystolicMatrixBase<T>::reset()
{
    for (auto& row : matrix_) {
        std::fill(row.begin(), row.end(), 0);
    }
    /*
    DPRINTF(SystolicMatrix,
        "[SystolicMatrixBase::reset] Matrix reset to zeros\n");
    */
}

template<typename T>
T& SystolicMatrixBase<T>::getData(uint32_t row, uint32_t col)
{
    validatePosition(row, col);
    return matrix_[row][col];
}

template<typename T>
void SystolicMatrixBase<T>::setData(uint32_t row, uint32_t col, T data)
{
    validatePosition(row, col);
    matrix_[row][col] = data;
}

template<typename T>
void SystolicMatrixBase<T>::setRowData(
    uint32_t row, const std::vector<T>& data)
{
    validatePosition(row, 0);
    if (data.size() != cols_) {
        panic("SystolicMatrixBase: "
              "Row data size (%zu) doesn't match cols (%u)",
              data.size(), cols_);
    }
    matrix_[row] = data;
}

template<typename T>
void SystolicMatrixBase<T>::setColData(
    uint32_t col, const std::vector<T>& data)
{
    validatePosition(0, col);
    if (data.size() != rows_) {
        panic("SystolicMatrixBase: "
              "Column data size (%zu) doesn't match rows (%u)",
              data.size(), rows_);
    }
    for (uint32_t row = 0; row < rows_; row++) {
        matrix_[row][col] = data[row];
    }
}

template<typename T>
std::vector<T>& SystolicMatrixBase<T>::getRowData(uint32_t row)
{
    validatePosition(row, 0);
    return matrix_[row];
}

template<typename T>
std::vector<T> SystolicMatrixBase<T>::getColData(uint32_t col)
{
    validatePosition(0, col);
    std::vector<T> column;
    column.reserve(rows_);
    for (uint32_t row = 0; row < rows_; row++) {
        column.push_back(matrix_[row][col]);
    }
    return column;
}

template<typename T>
void SystolicMatrixBase<T>::printMatrix() const
{
#ifdef DEBUG
    DPRINTF(SystolicMatrix, "[printMatrix] %s Matrix %ux%u:\n",
             getMatrixType().c_str(), rows_, cols_);
    for (uint32_t row = 0; row < rows_; row++) {
        std::stringstream ss;
        ss << "  Row " << row << ": ";
        for (uint32_t col = 0; col < cols_; col++) {
            ss << "0x" << std::hex << matrix_[row][col] << " ";
        }
        DPRINTF(SystolicMatrix, "%s\n", ss.str().c_str());
    }
#endif
}

template class SystolicMatrixBase<uint64_t>; // 显式实例化模板类
template class SystolicMatrixBase<__uint128_t>; // 显式实例化模板类

// === 横向输入矩阵 ===
HorizontalInputMatrix::HorizontalInputMatrix(
    const std::string &name_, uint32_t rows, uint32_t cols)
    : SystolicMatrixBase64(name_, rows, cols)
{
    DPRINTF(SystolicMatrix,
        "[HorizontalInputMatrix] Created %ux%u horizontal matrix\n",
        rows, cols);
}

void HorizontalInputMatrix::advance(const std::vector<uint64_t>& dataIn)
{
    // 数据从左向右脉动：col -> col + 1
    for (uint32_t row = 0; row < this->rows_; row++) {
        for (int32_t col = this->cols_ - 1; col > 0; col--) {
            this->matrix_[row][col] = this->matrix_[row][col - 1];
        }
    }
    // 更新第一列（新输入）
    if (dataIn.size() != this->rows_) {
        panic("HorizontalInputMatrix: "
              "New data size (%zu) doesn't match rows (%u)",
              dataIn.size(), this->rows_);
    }
    for (uint32_t row = 0; row < this->rows_; row++) {
        this->matrix_[row][0] = dataIn[row];
    }
    DPRINTF(SystolicMatrix,
        "[HorizontalInputMatrix::advance] Data flowed rightward\n");
}
void HorizontalInputMatrix::advance()
{
    // 不使用无输入的advance
    // 默认情况：保持不变
    DPRINTF(SystolicMatrix,
        "[HorizontalInputMatrix::advance] Matrix remains unchanged\n");
}

// 前面advance函数已经实现了这个功能，这个函数实际上并不是很有必要
void HorizontalInputMatrix::setFirstColumn(
    const std::vector<uint64_t>& newData)
{
    if (newData.size() != this->rows_) {
        panic("HorizontalInputMatrix: "
              "New data size (%zu) doesn't match rows (%u)",
              newData.size(), this->rows_);
    }

    for (uint32_t row = 0; row < this->rows_; row++) {
        this->matrix_[row][0] = newData[row];
    }

    DPRINTF(SystolicMatrix,
        "[HorizontalInputMatrix::setFirstColumn] "
        "Set first column with new data\n");
}



// === 纵向输入矩阵 ===
VerticalInputMatrix::VerticalInputMatrix(
    const std::string &name_, uint32_t rows, uint32_t cols)
    : SystolicMatrixBase64(name_, rows, cols)
{
    DPRINTF(SystolicMatrix,
        "[VerticalInputMatrix] Created %ux%u vertical matrix\n", rows, cols);
}

void VerticalInputMatrix::advance(const std::vector<uint64_t>& dataIn)
{
    // 数据从上向下脉动：row -> row+1
    for (uint32_t col = 0; col < this->cols_; col++) {
        for (int32_t row = this->rows_ - 1; row > 0; row--) {
            this->matrix_[row][col] = this->matrix_[row - 1][col];
        }
    }
    // 设置第一行（新输入）
    if (dataIn.size() != this->cols_) {
        panic("VerticalInputMatrix: "
              "New data size (%zu) doesn't match cols (%u)",
              dataIn.size(), this->cols_);
    }

    for (uint32_t col = 0; col < this->cols_; col++) {
        this->matrix_[0][col] = dataIn[col];
    }
    DPRINTF(SystolicMatrix,
        "[VerticalInputMatrix::advance] Data flowed downward\n");
}

void VerticalInputMatrix::advance()
{
    // 不使用无输入的advance
    // 默认情况：保持不变
    DPRINTF(SystolicMatrix,
        "[VerticalInputMatrix::advance] Matrix remains unchanged\n");
}
void VerticalInputMatrix::setFirstRow(const std::vector<uint64_t>& newData)
{
    if (newData.size() != this->cols_) {
        panic("VerticalInputMatrix: "
              "New data size (%zu) doesn't match cols (%u)",
              newData.size(), this->cols_);
    }

    for (uint32_t col = 0; col < this->cols_; col++) {
        this->matrix_[0][col] = newData[col];
    }

    DPRINTF(SystolicMatrix,
        "[VerticalInputMatrix::setFirstRow] Set first row with new data\n");
}

// === 输出矩阵 ===
OutputMatrix::OutputMatrix(
    const std::string &name_, uint32_t rows, uint32_t cols)
    : SystolicMatrixBase128(name_, rows, cols)
{
    DPRINTF(SystolicMatrix,
        "[OutputMatrix] Created %ux%u output matrix\n", rows, cols);
}


void OutputMatrix::advance(const std::vector<__uint128_t>& dataIn)
{
    // 输出矩阵不进行脉动，可以选择清空或保持
    // 这里选择保持不变
    DPRINTF(SystolicMatrix,
        "[OutputMatrix::advance] Output matrix remains unchanged\n");
}
void OutputMatrix::advance()
{
    // 输出矩阵不进行脉动，可以选择清空或保持
    // 这里选择保持不变
    DPRINTF(SystolicMatrix,
        "[OutputMatrix::advance] Output matrix remains unchanged\n");
}

} // namespace gem5
