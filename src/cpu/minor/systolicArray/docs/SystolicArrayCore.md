# SystolicArrayCore 类详细文档

## 概述

`SystolicArrayCore` 类是 gem5 模拟器中用于实现 RISC-V AME (Advanced Matrix Extension) 指令集的脉动阵列核心。该类继承自 `SimObject`，负责管理和控制整个脉动阵列的计算过程，支持矩阵乘法运算(MMA)的高效执行。

## 类功能分析

`SystolicArrayCore` 类实现了一个完整的脉动阵列计算单元，主要用于加速矩阵乘法运算。它采用了分层的组织结构，将多个处理元素(PE)组织成 Tile，再将多个 Tile 组织成整个阵列。该类支持多种数据类型(整数和浮点数)的矩阵运算，并提供了灵活的配置选项。

## 成员变量说明

### 阵列配置参数
- `arrayRows_`: 脉动阵列的总行数，从配置参数中获取
- `arrayCols_`: 脉动阵列的总列数，从配置参数中获取
- `peOpDelay_`: PE操作延迟周期数，从配置参数中获取
- `peRowsPerTile_`: 每个 Tile 包含的 PE 行数，从配置参数中获取
- `peColsPerTile_`: 每个 Tile 包含的 PE 列数，从配置参数中获取
- `tileRows_`: 脉动阵列的 Tile 行数，计算得出：(arrayRows_ + peRowsPerTile_ - 1) / peRowsPerTile_
- `tileCols_`: 脉动阵列的 Tile 列数，计算得出：(arrayCols_ + peColsPerTile_ - 1) / peColsPerTile_

### 数据矩阵
- `horizontalMatrix_`: 横向输入矩阵，用于存储从左向右流动的数据，类型为 `HorizontalInputMatrix`
- `verticalMatrix_`: 纵向输入矩阵，用于存储从上向下流动的数据，类型为 `VerticalInputMatrix`
- `outputMatrix_`: 输出矩阵，用于存储计算结果，类型为 `OutputMatrix`
- `dataFinishedMatrix_`: 数据输入完成标志矩阵，用于跟踪数据流动状态，二维布尔向量

### 计算单元组织
- `tileArray_`: PE 阵列，按 Tile 组织的二维数组，每个元素是 `SystolicTile` 的智能指针

### 执行状态
- `isConfigured_`: 阵列是否已配置完成，布尔值
- `currentCycle_`: 当前执行周期，无符号整数
- `totalCycles_`: 总执行周期数，无符号整数
- `accRowIndex_`: 当前累加的行索引，无符号整数
- `accFinished_`: 累加过程是否完成，布尔值

### 指令相关
- `currMinorDynInst_`: 当前执行的动态指令指针，类型为 `minor::MinorDynInstPtr`
- `currInst_`: 当前 RISC-V 矩阵算术指令指针，类型为 `RiscvISA::MatrixArithMmaInst*`
- `machInst_`: 机器码，用于获取指令信息，64位无符号整数

### 数据类型配置
- `srcType_`: 源操作数数据类型，枚举值 `MatDataType`
- `destType_`: 目标操作数数据类型，枚举值 `MatDataType`
- `saturationEnabled_`: 是否启用饱和运算，布尔值
- `wideningFactor_`: 数据扩展因子，8位无符号整数
- `isFloat_`: 是否为浮点运算，布尔值

### 寄存器指针
- `tmp_s1`: 源操作数寄存器容器1，类型为 `RiscvISA::MatRegContainer`
- `tmp_s2`: 源操作数寄存器容器2，类型为 `RiscvISA::MatRegContainer`
- `tmp_d0`: 目标操作数寄存器容器指针，类型为 `RiscvISA::MatRegContainer*`

### 计算参数
- `computedM_`: 实际计算的矩阵 M 维度，从指令中获取
- `computedN_`: 实际计算的矩阵 N 维度，从指令中获取
- `computedK_`: 实际计算的矩阵 K 维度，从指令中获取
- `computedTileRows_`: 实际需要的 Tile 行数，计算得出：(computedM_ + peRowsPerTile_ - 1) / peRowsPerTile_
- `computedTileCols_`: 实际需要的 Tile 列数，计算得出：(computedN_ + peColsPerTile_ - 1) / peColsPerTile_

### 数据缓存
- `horizontalInputBuffer_`: 横向输入数据缓存，向量类型
- `verticalInputBuffer_`: 纵向输入数据缓存，向量类型
- `horizontalInputIndex_`: 横向输入数据索引，无符号整数
- `verticalInputIndex_`: 纵向输入数据索引，无符号整数
- `elementsPer64bit_`: 每个64位字包含的元素数，无符号整数

## 成员函数详细说明

### 构造函数和析构函数

#### `SystolicArrayCore(const SystolicArrayCoreParams &params)`
- **功能**: 初始化脉动阵列核心，创建数据矩阵和PE阵列
- **参数**: `params` - 包含阵列配置参数的结构体
- **实现细节**:
  - 从参数中获取阵列尺寸和PE配置
  - 计算Tile行列数
  - 创建横向、纵向和输出矩阵
  - 调用`initializePEArray()`初始化PE阵列
  - 初始化数据缓存和状态变量
- **使用示例**: 在创建模拟器时通过参数配置创建脉动阵列核心

### 配置接口

#### `configurePE(MatDataType srcType, MatDataType destType, bool saturationEnabled, uint8_t wideningFactor)`
- **功能**: 配置所有PE的数据类型和计算参数
- **参数**:
  - `srcType`: 源操作数数据类型
  - `destType`: 目标操作数数据类型
  - `saturationEnabled`: 是否启用饱和运算
  - `wideningFactor`: 数据扩展因子
- **实现细节**:
  - 根据数据类型计算每个64位字包含的元素数
  - 遍历所有Tile并配置其PE
  - 重置状态变量和数据缓存
  - 初始化数据结束标志矩阵
- **使用示例**: 在接收新指令后，根据指令参数配置PE

### 指令接口

#### `acceptInstruction(minor::QueuedInst &inst, minor::ExecContext* xc)`
- **功能**: 接受并处理新的矩阵运算指令
- **参数**:
  - `inst`: 待处理的队列指令引用
  - `xc`: 执行上下文指针
- **实现细节**:
  - 从指令中提取动态指令和静态指令指针
  - 获取机器码用于指令解析
  - 调用`computeSrcType`和`computeDestType`计算数据类型
  - 从指令中提取饱和标志、扩展因子等信息
  - 获取矩阵维度信息
  - 调用`configurePE`配置PE阵列
  - 从执行上下文中获取源和目标寄存器指针
- **使用示例**: 当CPU需要执行矩阵运算指令时调用此函数

### 执行控制

#### `advance()`
- **功能**: 推进脉动阵列一个周期的计算
- **实现细节**:
  - 检查阵列是否已配置，未配置则报错
  - 判断MAC是否完成：
    - 若完成且累加未完成，执行累加操作
    - 若完成且累加也完成，设置累加完成标志
  - 若MAC未完成：
    - 调用`loadHorizontalData`和`loadVerticalData`加载数据
    - 更新横向和纵向矩阵的数据流动
    - 遍历所有Tile并推进其计算状态
    - 更新输出矩阵
    - 调用`updateDataFinishedMatrix`更新数据结束标志
    - 更新下一个周期的输入索引
- **使用示例**: 在每个时钟周期调用此函数以推进计算

#### `reset()`
- **功能**: 重置整个阵列的状态
- **实现细节**:
  - 重置所有数据矩阵
  - 遍历所有Tile并重置其状态
  - 重置配置标志和状态变量
  - 清空数据缓存
  - 重置数据结束标志矩阵
- **使用示例**: 在开始新的计算前或需要重置状态时调用

### 结果获取

#### `getOutputRow(uint32_t row)`
- **功能**: 获取指定行的输出数据
- **参数**: `row` - 行索引
- **返回值**: 包含该行所有列数据的向量
- **实现细节**:
  - 检查行索引是否越界
  - 调用输出矩阵的`getRowData`方法获取数据
- **使用示例**: 获取计算结果的某一行数据

#### `getOutputColumn(uint32_t col)`
- **功能**: 获取指定列的输出数据
- **参数**: `col` - 列索引
- **返回值**: 包含该列所有行数据的向量
- **实现细节**:
  - 检查列索引是否越界
  - 调用输出矩阵的`getColData`方法获取数据
- **使用示例**: 获取计算结果的某一列数据

#### `getOutput(uint32_t row, uint32_t col)`
- **功能**: 获取指定位置的输出数据
- **参数**:
  - `row` - 行索引
  - `col` - 列索引
- **返回值**: 指定位置的输出数据，128位无符号整数
- **实现细节**:
  - 检查位置是否越界
  - 调用输出矩阵的`getData`方法获取数据
- **使用示例**: 获取计算结果的特定元素

#### `writeBackOutput()`
- **功能**: 将计算结果写回到目标寄存器
- **实现细节**:
  - 检查累加是否完成，未完成则报错
  - 遍历所有计算位置：
    - 从输出矩阵获取数据
    - 根据目标数据类型调用相应的写回函数
    - 对于整数类型调用`writeBackInt`
    - 对于浮点类型调用`writeBackFloat`
  - 更新跟踪数据(如果存在)
- **使用示例**: 在计算完成后将结果写回寄存器

### 状态查询

#### `isIdle()`
- **功能**: 检查阵列是否处于空闲状态
- **返回值**: 布尔值，true表示空闲
- **实现细节**:
  - 遍历所有Tile
  - 检查每个Tile是否空闲
  - 仅当所有Tile都空闲时返回true
- **使用示例**: 判断是否可以接受新指令

#### `isComputing()`
- **功能**: 检查阵列是否正在计算
- **返回值**: 布尔值，true表示正在计算
- **实现细节**:
  - 遍历所有Tile
  - 检查是否有任一Tile正在计算
  - 任一Tile计算中即返回true
- **使用示例**: 判断计算是否正在进行

#### `isMacFinished()`
- **功能**: 检查矩阵乘累加运算是否完成
- **返回值**: 布尔值，true表示MAC完成
- **实现细节**:
  - 检查最后一个Tile(computedTileRows_-1, computedTileCols_-1)的输出是否有效
  - 仅当最后一个PE的输出有效时返回true
- **使用示例**: 判断MAC阶段是否完成，可以开始累加

#### `isOutputValid()`
- **功能**: 检查输出数据是否有效
- **返回值**: 布尔值，true表示输出有效
- **实现细节**:
  - 检查累加过程是否完成
  - 仅当累加完成时返回true
- **使用示例**: 判断是否可以写回结果

### 调试接口

#### `printArrayState()`
- **功能**: 打印阵列当前状态
- **实现细节**:
  - 打印当前周期和阵列尺寸信息
  - 调用各矩阵的`printMatrix`方法打印数据矩阵状态
- **使用示例**: 调试时查看阵列状态

#### `printDataFlows()`
- **功能**: 打印数据流动情况
- **实现细节**:
  - 打印当前周期信息
  - 打印横向数据流(从左到右)
  - 打印纵向数据流(从上到下)
- **使用示例**: 调试时查看数据流动情况

### 私有方法

#### `initializePEArray()`
- **功能**: 初始化PE阵列和Tile
- **实现细节**:
  - 调整tileArray_的行数
  - 为每行预留列空间
  - 遍历所有Tile位置：
    - 计算当前Tile在原始阵列中的起始和结束行列
    - 计算实际Tile尺寸
    - 为Tile分配数据指针
    - 创建Tile对象并添加到阵列中
  - 初始化数据结束标志矩阵
- **使用示例**: 在构造函数中调用，完成PE阵列初始化

#### `computeSrcType(uint8_t fp, uint8_t sn, uint8_t eew)`
- **功能**: 根据指令参数计算源数据类型
- **参数**:
  - `fp`: 浮点标志，0表示整数，非0表示浮点
  - `sn`: 符号标志，0表示无符号，非0表示有符号
  - `eew`: 元素宽度编码，0=8位，1=16位，2=32位，3=64位
- **实现细节**:
  - 根据fp判断是整数还是浮点类型
  - 根据sn和eew确定具体的数据类型
  - 设置srcType_成员变量
- **使用示例**: 在acceptInstruction中调用，解析指令中的源数据类型

#### `computeDestType(uint8_t fp, uint8_t sn, uint8_t eew, uint8_t widen)`
- **功能**: 根据指令参数计算目标数据类型
- **参数**:
  - `fp`: 浮点标志，0表示整数，非0表示浮点
  - `sn`: 符号标志，0表示无符号，非0表示有符号
  - `eew`: 元素宽度编码，0=8位，1=16位，2=32位，3=64位
  - `widen`: 扩展因子，表示目标位宽比源位宽宽多少级
- **实现细节**:
  - 计算目标位宽 = 源位宽 + 扩展因子
  - 根据fp判断是整数还是浮点类型
  - 根据sn和目标位宽确定具体的数据类型
  - 设置destType_成员变量
- **使用示例**: 在acceptInstruction中调用，解析指令中的目标数据类型

#### `loadHorizontalData(uint32_t col_index)`
- **功能**: 从左矩阵寄存器加载横向数据
- **参数**: `col_index` - 要加载的列索引
- **实现细节**:
  - 从tmp_s1寄存器获取矩阵数据
  - 遍历所有行：
    - 计算当前行应该加载的列索引(递减)
    - 如果列索引有效且在范围内，加载对应数据
    - 否则填充0
    - 如果超出计算范围，设置数据结束标志
  - 将数据存储到horizontalInputBuffer_
- **使用示例**: 在advance函数中调用，加载横向输入数据

#### `loadVerticalData(uint32_t row_index)`
- **功能**: 从右矩阵寄存器加载纵向数据
- **参数**: `row_index` - 要加载的行索引
- **实现细节**:
  - 使用dispatch_mat_type根据数据类型分发处理
  - 从tmp_s2寄存器获取矩阵数据
  - 计算每个64位字包含的元素数
  - 遍历所有列：
    - 计算当前列应该加载的行索引
    - 如果行索引有效且在范围内：
      - 组合多行数据成64位
      - 从当前行开始，取elements_per_64bit行数据
      - 将每个元素移位到正确位置并组合
    - 否则填充0
    - 每peColsPerTile_列，行索引递减elements_per_64bit
  - 将数据存储到verticalInputBuffer_
- **使用示例**: 在advance函数中调用，加载纵向输入数据

#### `updateDataFinishedMatrix()`
- **功能**: 更新数据加载完成标志矩阵
- **实现细节**:
  - 从右下角开始遍历矩阵
  - 根据相邻位置的状态更新当前位置：
    - 如果有上方和左方，则两者都完成时当前完成
    - 如果只有上方，则上方完成时当前完成
    - 如果只有左方，则左方完成时当前完成
- **使用示例**: 在advance函数中调用，更新数据流动状态

#### `accumulateOutput(uint32_t row_index)`
- **功能**: 累加输出矩阵到结果寄存器
- **参数**: `row_index` - 要累加的行索引
- **实现细节**:
  - 根据源和目标类型选择计算路径
  - 对于整数类型，调用相应的`computeIntegerACC`模板函数
  - 对于浮点类型，调用相应的`computeFloatACC`模板函数
- **使用示例**: 在advance函数中MAC完成后调用，执行累加操作

#### `computeIntegerACC(uint32_t row_index, bool saturationEnabled)` (模板函数)
- **功能**: 整数累加计算模板
- **参数**:
  - `row_index` - 要累加的行索引
  - `saturationEnabled` - 是否启用饱和运算
- **实现细节**:
  - 获取目标寄存器数据
  - 遍历该行的所有列：
    - 如果启用饱和运算：
      - 使用宽类型进行计算以检测溢出
      - 检查结果是否超出目标类型范围
      - 根据溢出情况设置最大值或最小值
    - 否则直接使用目标类型进行计算
    - 将结果存储到输出矩阵
- **使用示例**: 在accumulateOutput中根据数据类型调用

#### `computeFloatACC(uint32_t row_index)` (模板函数)
- **功能**: 浮点累加计算模板
- **参数**: `row_index` - 要累加的行索引
- **实现细节**:
  - 获取目标寄存器数据
  - 遍历该行的所有列：
    - 将寄存器数据和输出矩阵数据转换为浮点类型
    - 使用fadd函数进行浮点加法
    - 将结果存储到输出矩阵
- **使用示例**: 在accumulateOutput中根据数据类型调用

#### `writeBackInt(uint32_t row, uint32_t col, __uint128_t data)` (模板函数)
- **功能**: 整数结果写回模板
- **参数**:
  - `row` - 行索引
  - `col` - 列索引
  - `data` - 要写回的数据
- **实现细节**:
  - 获取目标寄存器数据
  - 将数据转换为指定类型并写入寄存器
- **使用示例**: 在writeBackOutput中根据目标数据类型调用

#### `writeBackFloat(uint32_t row, uint32_t col, __uint128_t data)` (模板函数)
- **功能**: 浮点结果写回模板
- **参数**:
  - `row` - 行索引
  - `col` - 列索引
  - `data` - 要写回的数据
- **实现细节**:
  - 获取目标寄存器数据(使用浮点类型的内部表示)
  - 将数据转换为指定类型并写入寄存器
- **使用示例**: 在writeBackOutput中根据目标数据类型调用

## 工作流程

1. **初始化阶段**：
   - 创建PE阵列和Tile结构
   - 初始化数据矩阵和缓存
   - 设置默认参数

2. **指令接收阶段**：
   - 通过`acceptInstruction`接收新的矩阵运算指令
   - 解析指令参数，确定数据类型和计算参数
   - 配置PE阵列的计算参数

3. **数据加载阶段**：
   - 从源寄存器加载矩阵数据到输入缓存
   - 将数据分发到PE阵列的输入矩阵
   - 更新数据流动状态

4. **计算执行阶段**：
   - 通过`advance`方法周期性推进计算
   - 数据在PE阵列中脉动流动
   - 每个PE执行乘累加运算

5. **结果收集阶段**：
   - 从输出矩阵收集计算结果
   - 执行必要的累加和类型转换
   - 将结果写回到目标寄存器

## 数据类型支持

该类支持多种数据类型的矩阵运算：
- 整数类型：Int8, UInt8, Int16, UInt16, Int32, UInt32, Int64, UInt64
- 浮点类型：Float16, BFloat16, Float32, Float64

不同类型之间支持自动扩展和转换，并可选择是否启用饱和运算。

## 依赖关系

`SystolicArrayCore` 类依赖于以下组件：
- `SystolicTile`: 脉动阵列Tile类，用于组织和管理PE
- `ProcessElement`: 处理元素类，执行实际的计算操作
- `HorizontalInputMatrix`: 横向输入矩阵，管理从左向右流动的数据
- `VerticalInputMatrix`: 纵向输入矩阵，管理从上向下流动的数据
- `OutputMatrix`: 输出矩阵，收集和存储计算结果

## 使用示例

```cpp
// 创建脉动阵列核心
SystolicArrayCoreParams params;
params.arrayRows = 8;
params.arrayCols = 8;
params.peRowsPerTile = 1;
params.peColsPerTile = 4;

SystolicArrayCore* saCore = new SystolicArrayCore(params);

// 配置PE为int8类型，启用饱和运算
saCore->configurePE(MatDataType::Int8, MatDataType::Int32, true, 4);

// 接收并执行矩阵乘法指令
saCore->acceptInstruction(inst, execContext);

// 周期性推进计算
while (!saCore->isMacFinished()) {
    saCore->advance();
}

// 获取计算结果
auto result = saCore->getOutput(0, 0);
```

这个类为 gem5 模拟器提供了完整的脉动阵列实现，支持高效的矩阵运算模拟，对于研究AI加速器和矩阵运算硬件的工作原理非常有价值。
