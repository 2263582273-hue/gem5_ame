# GEM5 RISC-V AME 矩阵扩展加速器工作报告

## 一、报告概述

### 1.1 报告对象

本报告面向 gem5 `MinorCPU` 中新增的 RISC-V AME（Array/Matrix Extension）矩阵扩展加速器实现，阅读和分析范围以 `src/cpu/minor/AME` 目录为主，并结合 AME 直接依赖的 `src/cpu/minor/systolicArray`、`BaseMinorCPU.py`、`cpu.hh`、`execute.cc` 等接入代码说明整体架构。

AME 当前实现目标是把 RISC-V 矩阵计算和矩阵访存类指令从 MinorCPU 普通执行路径中分离出来，由独立的 AME 接口、指令队列、脉动阵列计算核心和矩阵访存单元协同完成。CPU 负责识别并提交矩阵指令，AME 负责异步执行，执行完成后通过回调通知 CPU 流水线继续提交对应指令。

### 1.2 报告性质

本文件不是 README，而是项目工作报告。重点不是使用说明，而是对 GEM5 AME 扩展的设计目标、架构原理、模块分工、关键参数、数据通路、控制通路、当前完成情况和后续改进方向进行系统总结。

### 1.3 源码范围

| 路径 | 作用 |
| --- | --- |
| `AME/AMEInterface.py` | 声明 AME 顶层 SimObject 和内部指令队列 SimObject |
| `AME/ame_interface.hh/.cc` | AME 顶层接口、AICB 风格 issue 通道、内存端口、请求回调 |
| `AME/inst_queue.hh/.cc` | AME 内部指令队列和周期调度逻辑 |
| `AME/MMU/MMU.py` | 声明 AME 访存单元和读写 timing 子模块 |
| `AME/MMU/ame_mem_unit.hh/.cc` | 矩阵访存控制单元，解析矩阵访存微指令 |
| `AME/MMU/read_timing_unit.hh/.cc` | 矩阵 load 时序模块 |
| `AME/MMU/write_timing_unit.hh/.cc` | 矩阵 store 时序模块 |
| `AME/packet.hh` | AME 自定义 Packet，携带请求编号和通道号 |
| `AME/req_state.hh` | AME pending 请求状态和响应回调封装 |
| `AME/defines.hh` | AME 数据位置枚举 |
| `systolicArray/*` | 脉动阵列计算核心、PE、Tile 和数据矩阵 |
| `BaseMinorCPU.py`、`cpu.hh`、`execute.cc` | MinorCPU 侧 AME 接入点 |

## 二、项目背景与设计目标

### 2.1 背景

传统标量 CPU 对矩阵乘加、矩阵装载、矩阵存储等操作的执行效率较低。RISC-V 矩阵扩展通过矩阵寄存器、矩阵计算指令和矩阵访存指令向软件暴露更高层次的数据并行能力。为了在 gem5 中评估此类 ISA 扩展和微架构设计，需要在 CPU 模型中增加一个能够执行矩阵指令的独立扩展部件。

现有实现选择在 `MinorCPU` 中挂接 AME，而不是把所有矩阵行为塞进普通功能单元。这样做的主要原因是矩阵计算和矩阵访存具有以下特点：

1. 指令延迟不固定，取决于矩阵维度、元素宽度、阵列规模和访存响应。
2. 执行过程可能持续多个周期，需要独立状态机推进。
3. 矩阵指令主要读写矩阵寄存器，与普通整数/浮点指令的数据通路不同。
4. 访存类矩阵指令需要批量搬运一行或多行矩阵数据，天然适合由专门的 memory unit 管理。

### 2.2 设计目标

AME 扩展的主要目标如下：

1. 在 MinorCPU 中接入矩阵指令的异步执行路径。
2. 提供 AICB 风格的指令 issue 握手机制，表达 `valid`、`ready`、`accept`、读寄存器有效和写回有效等信息。
3. 建立 AME 内部指令队列，把矩阵计算指令和矩阵访存指令分发到不同执行单元。
4. 使用 `SystolicArrayCore` 模拟矩阵乘加类指令的硬件计算过程。
5. 使用 `AMEMemUnit`、`MemUnitReadTiming`、`MemUnitWriteTiming` 模拟矩阵 load/store 的分行、分 cache line 访存行为。
6. 通过 gem5 timing request/response 机制接入内存系统，使 AME 访存请求能够经过 TLB 和 cache/memory。
7. 通过回调机制通知 MinorCPU 对应矩阵指令完成，维持 CPU 流水线的提交语义。

## 三、总体架构

### 3.1 架构层次

AME 当前采用“CPU 提交，AME 排队，执行单元异步执行，完成回调 CPU”的结构。整体层次如下：

```text
MinorCPU Execute/Commit
        |
        | issue_aicb(valid, hartId, inst, instId, xc, callback)
        v
+-------------------+
|   AMEInterface    |
|  - mem_port       |
|  - pending req Q  |
+---------+---------+
          |
          v
+-------------------+
|     InstQueue     |
|  Instruction_Q    |
|  Memory_Q         |
+-----+-------+-----+
      |       |
      |       |
      v       v
+-----------+  +----------------+
| Systolic  |  |   AMEMemUnit   |
| ArrayCore |  | Read/Write     |
|           |  | Timing Units   |
+-----------+  +--------+-------+
                       |
                       v
                 gem5 Memory System
                 TLB + Cache + Memory
```

### 3.2 控制流概括

矩阵指令从 CPU 到 AME 的控制流如下：

1. MinorCPU 在提交阶段识别 `SystolicMMA` 或矩阵访存类指令。
2. MinorCPU 创建 `ExecContext`，调用 `AMEInterface::issue_aicb()` 发起握手。
3. `AMEInterface` 检查指令类型和队列容量，生成 `ready`、`accept`、`rdValid`、`wbValid`。
4. 如果 `valid && ready`，指令被接受并送入 AME 内部队列。
5. `InstQueue` 启动 ticking，在后续周期中调度计算队列或访存队列。
6. 计算指令送入 `SystolicArrayCore`，访存指令送入 `AMEMemUnit`。
7. 执行单元完成后调用原始 dependency callback，使 MinorCPU 知道矩阵指令已经完成。

### 3.3 数据流概括

计算路径的数据主要在矩阵寄存器和脉动阵列之间流动：

1. `SystolicArrayCore::acceptInstruction()` 从 `ExecContext` 读取源矩阵寄存器。
2. 根据矩阵指令字段解析源数据类型、目标数据类型、矩阵维度和扩展位宽。
3. 每个周期通过 `advance()` 将矩阵数据注入横向和纵向输入矩阵。
4. Tile 和 PE 阵列执行乘加。
5. MAC 结束后执行输出累加。
6. `writeBackOutput()` 把输出矩阵写回目标矩阵寄存器。

访存路径的数据主要在普通内存和矩阵寄存器之间流动：

1. `AMEMemUnit::issue()` 解析矩阵访存微指令，得到基地址、stride、微指令行号、元素宽度和目标/源矩阵寄存器。
2. load 指令由 `MemUnitReadTiming` 按 cache line 合并读请求，从内存读出一行矩阵数据后写入矩阵寄存器对应行。
3. store 指令由 `MemUnitWriteTiming` 先从矩阵寄存器取出一行数据，再按 cache line 合并写请求写回内存。
4. 读写请求通过 `AMEInterface::readAMEMem()`、`writeAMEMem()` 进入 gem5 timing memory system。

## 四、MinorCPU 接入方式

### 4.1 Python 参数接入

`BaseMinorCPU.py` 中新增：

```python
ameInterface = Param.AMEInterface(
    AMEInterface(), "AME by xzc"
)
```

该参数使每个 MinorCPU 实例可以持有一个 AMEInterface 对象。`cpu.hh` 中也增加了：

```cpp
AMEInterface* ameInterface;
```

因此 AME 在 gem5 对象体系中是 CPU 的一个子部件，通过参数系统创建并连接。

### 4.2 提交阶段接入

`execute.cc` 在 commit 逻辑中对矩阵指令做了特殊处理。当指令属于 `SystolicMMA` 或 `SimdUnitStrideLoad` 时，不按普通指令立即提交，而是：

1. 判断当前是否正在等待 AME。
2. 如果尚未送入 AME，则调用 `cpu.ameInterface->issue_aicb(...)`。
3. 如果 AME 接受指令，则设置 `waiting_ame_engine = true`，并暂缓普通提交完成。
4. 当 AME 执行结束后回调设置 `completed_mma_inst = true`。
5. 后续 commit 周期看到完成标志，再把该矩阵指令视为完成。

这种处理保留了 MinorCPU 的顺序提交语义，同时允许矩阵指令在 AME 内部多周期执行。

## 五、AMEInterface 顶层接口

### 5.1 基本职责

`AMEInterface` 是 AME 的顶层 SimObject，承担以下职责：

1. 接收 MinorCPU 发来的矩阵指令。
2. 实现 AICB 风格 issue 握手。
3. 管理 AME 内部指令队列、脉动阵列核心和访存单元。
4. 提供 AME memory port 和 register port。
5. 为每个访存请求分配唯一 `reqId`。
6. 保存 pending request，并在 response 返回时按请求顺序执行回调。

### 5.2 AICB issue 响应结构

`AMEInterface::AICBIssueResp` 包含：

| 字段 | 含义 |
| --- | --- |
| `ready` | AME 当前是否可接收该指令 |
| `accept` | 本次 `valid && ready` 是否握手成功 |
| `rdValid` | 根据源寄存器个数生成的读操作数有效位 |
| `wbValid` | 该指令是否存在目标寄存器写回 |

`issue_aicb()` 的核心逻辑是：

1. 判断指令是否被 AME 支持。
2. 计算源寄存器有效位和写回有效位。
3. 通过 `requestGrant()` 判断队列是否有空间。
4. 若接受，则调用 `sendInst()` 将指令送入内部队列。

### 5.3 指令分类与入队

`sendInst()` 根据静态指令类型分为两类：

| 指令类型 | 判断方式 | 入队位置 | 后续执行单元 |
| --- | --- | --- | --- |
| 矩阵计算指令 | `opClass() == enums::SystolicMMA` | `Instruction_Queue` | `SystolicArrayCore` |
| 矩阵访存指令 | `staticInst->isMemRef()` | `Memory_Queue` | `AMEMemUnit` |

当两个队列原本都为空时，`sendInst()` 会调用 `inst_queue->startTicking(*this)` 启动 AME 内部调度器。

### 5.4 忙闲判断

`AMEInterface::isbussy()` 将以下条件合并为 AME 忙状态：

1. `SystolicArrayCore` 非空闲。
2. `InstQueue` 正在占用。
3. 计算队列非空。
4. 访存队列非空。
5. `AMEMemUnit` 正在执行读或写。

该函数反映了 AME 的全局占用状态，可用于 CPU 或系统级调度判断。

## 六、InstQueue 调度机制

### 6.1 队列结构

`InstQueue` 继承 `TickedObject`，内部包含两个队列：

| 队列 | 作用 |
| --- | --- |
| `Instruction_Queue` | 保存矩阵计算指令，例如 `SystolicMMA` |
| `Memory_Queue` | 保存矩阵访存指令，例如矩阵 load/store 微指令 |

每个队列项为 `QueueEntry`，包含：

1. `MinorDynInstPtr inst`：动态指令。
2. `ExecContextPtr xc`：执行上下文，用于读写寄存器和访问线程上下文。
3. `dependencie_callback`：指令完成后通知 CPU 的回调。
4. `issued`：是否已经发射到执行单元。
5. `completed`：是否已经完成。

### 6.2 计算指令调度

当 `Instruction_Queue` 非空且脉动阵列未占用时：

1. 取队首指令。
2. 调用 `AMEInterface::issue()`。
3. `AMEInterface` 将指令送入 `SystolicArrayCore::acceptInstruction()`。
4. `InstQueue` 每个 tick 调用 `systolicArrayCore->advance()`。
5. 当 `isOutputValid()` 为真时，调用 `writeBackOutput()`。
6. 执行 `dependencie_callback()`。
7. 删除队首项并出队。

### 6.3 访存指令调度

当 `Memory_Queue` 非空时：

1. 取队首访存指令。
2. 如果该指令未发射且 `AMEMemUnit` 未占用，则调用 `AMEInterface::issue()`。
3. `AMEInterface` 将访存指令送入 `AMEMemUnit::issue()`。
4. `AMEMemUnit` 进一步启动读或写 timing 子模块。
5. 当读写子模块完成后调用 `finishCurrentMemInst()`，把队首项标记为 `completed`。
6. `InstQueue` 观察到完成标志后执行回调，出队并释放队列项。

该设计使计算路径和访存路径在 AME 内部具有独立的执行状态，但当前队列提交仍以队首项为核心。

## 七、脉动阵列计算核心

### 7.1 SystolicArrayCore 参数

`SystolicArrayCore.py` 中定义的主要参数如下：

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `opLat` | `1` cycle | PE 操作延迟基础参数 |
| `arrayRows` | `8` | 脉动阵列总行数 |
| `arrayCols` | `8` | 脉动阵列总列数 |
| `peRowsPerTile` | `1` | 每个 tile 包含的 PE 行数 |
| `peColsPerTile` | `4` | 每个 tile 包含的 PE 列数 |

由此可得到 tile 网格：

```text
tileRows = ceil(arrayRows / peRowsPerTile)
tileCols = ceil(arrayCols / peColsPerTile)
```

默认配置下，8x8 阵列按 1x4 PE tile 组织，形成 8 行、2 列 tile。

### 7.2 内部组成

`SystolicArrayCore` 内部主要包含：

1. `HorizontalInputMatrix`：横向输入矩阵，用于向 PE 阵列注入左操作数。
2. `VerticalInputMatrix`：纵向输入矩阵，用于向 PE 阵列注入右操作数。
3. `OutputMatrix`：保存 PE 计算输出。
4. `dataFinishedMatrix_`：标记每个 tile 或位置的数据输入是否完成。
5. `tileArray_`：二维 tile 阵列，每个 tile 包含若干 PE。
6. 源矩阵寄存器缓存 `tmp_s1`、`tmp_s2` 和目标矩阵寄存器指针 `tmp_d0`。

### 7.3 指令配置过程

`acceptInstruction()` 接收矩阵计算指令后执行以下工作：

1. 将静态指令转换为 `MatrixArithMmaInst`。
2. 读取指令字段，包括 `fp`、`sn`、`eew`、`widen`、`sa`、`mtilem`、`mtilen`、`mtilek`。
3. 调用 `computeSrcType()` 根据浮点/整数、有符号/无符号和元素宽度确定源类型。
4. 调用 `computeDestType()` 根据源宽度和 widen 字段确定目标类型。
5. 保存饱和计算标志和扩展因子。
6. 读取源矩阵寄存器。
7. 获取目标矩阵寄存器的可写指针。
8. 调用 `configurePE()` 配置所有 tile 和 PE。

### 7.4 数据类型支持

计算核心按 `fp`、`sn`、`eew`、`widen` 解析数据类型：

| 类型类别 | 支持元素 |
| --- | --- |
| 无符号整数 | UInt8、UInt16、UInt32、UInt64 |
| 有符号整数 | Int8、Int16、Int32、Int64 |
| 浮点 | Float16、Float32、Float64 |

目标类型由源元素宽度加 widen 得到。整数目标宽度最大 64 bit，浮点目标宽度最大 64 bit。

### 7.5 周期推进原理

`advance()` 是脉动阵列核心的周期推进函数。其行为分为两个阶段：

1. MAC 尚未结束时：
   - 从源矩阵寄存器加载横向输入数据。
   - 从源矩阵寄存器加载纵向输入数据。
   - 推进横向输入矩阵和纵向输入矩阵。
   - 推进所有 tile。
   - 保持或更新输出矩阵。
   - 更新数据结束标志。
   - 更新下一周期输入索引。

2. MAC 已结束时：
   - 按行执行 `accumulateOutput()`。
   - 所有行累加完成后设置 `accFinished_ = true`。
   - `isOutputValid()` 随后可被 `InstQueue` 用于触发写回。

### 7.6 写回机制

当输出有效后，`writeBackOutput()` 遍历 `computedM_ x computedN_` 的输出矩阵，根据目标数据类型选择对应写回模板：

1. 整数结果通过 `writeBackInt<T>()` 写回矩阵寄存器。
2. 浮点结果通过 `writeBackFloat<T>()` 写回矩阵寄存器。
3. 如果动态指令启用了 traceData，则把目标矩阵寄存器数据写入 trace。

## 八、矩阵访存单元 AMEMemUnit

### 8.1 模块职责

`AMEMemUnit` 是矩阵访存路径的控制模块。它不直接执行每个 cache line 的读写，而是负责：

1. 接收 AME 队列发来的矩阵访存指令。
2. 判断指令是否是合法的矩阵微指令。
3. 解析基地址、stride、微指令行号、元素宽度和矩阵寄存器编号。
4. 计算本条微指令实际访问的有效地址。
5. 根据 load/store 类型启动读或写 timing 子模块。
6. 在子模块完成时标记当前访存指令完成。

### 8.2 地址计算

`AMEMemUnit::issue()` 中的地址计算如下：

```cpp
RegVal base = inst.xc->getRegOperand(static_inst, 0);
RegVal stride = inst.xc->getRegOperand(static_inst, 1);
Addr ea = base + stride * curinst->microIdx;
```

含义为：

1. `rs1` 保存矩阵访存起始地址。
2. `rs2` 保存每行 stride，当前实现中也作为本条微指令搬运的总字节数。
3. `microIdx` 表示当前矩阵访存微指令对应的矩阵行号。
4. 有效地址 `ea` 等于基地址加上 `microIdx` 行偏移。

因此，当前 AME 访存模型以“每条微指令搬运矩阵寄存器的一行”为基本粒度。

### 8.3 元素宽度解析

`eew` 字段到字节数的映射为：

| `eew` | 元素字节数 `DST_SIZE` |
| --- | --- |
| `0x0` | 1 byte |
| `0x1` | 2 bytes |
| `0x2` | 4 bytes |
| `0x3` | 8 bytes |

如果出现其他值，则触发 panic。

### 8.4 load/store 分派

`AMEMemUnit` 根据 `static_inst->isLoad()` 判断方向：

| 方向 | 子模块 | 数据方向 |
| --- | --- | --- |
| load | `MemUnitReadTiming` | memory 到 matrix register |
| store | `MemUnitWriteTiming` | matrix register 到 memory |

当前 `AMEMemUnit::isOccupied()` 通过读写子模块的 `occupied` 状态判断访存单元是否忙。

## 九、MemUnitReadTiming 读时序模块

### 9.1 参数

`MemUnitReadTiming` 是 `TickedObject`，参数如下：

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `channel` | `0` | AME memory channel 编号 |
| `cacheLineSize` | `8` | 内存/cache line 粒度，单位 byte |
| `VRF_LineSize` | `16` | 矩阵寄存器行大小，单位 byte |

### 9.2 初始化

`initialize()` 设置以下状态：

1. `amewrapper`：指向顶层 AMEInterface。
2. `ea`：本条微指令访问起始地址。
3. `size`：本行搬运总字节数。
4. `DST_SIZE`：单个元素字节数。
5. `regid`：目标矩阵寄存器。
6. `data_from`：数据来源，当前主要为 `Location::mem`。
7. `xc`：执行上下文。
8. `numcount = size / DST_SIZE`：本行元素个数。
9. `readIndex = 0`：当前读到的元素索引。

初始化后模块会先尝试执行一次 `readFunction()`。如果未完成，则启动 ticking。

### 9.3 cache line 合并读

`readFunction()` 的核心思想是按元素顺序读取，但尽量把同一 cache line 内的连续元素合并为一次内存读：

1. 根据 `readIndex` 计算当前元素地址。
2. 对齐得到 `line_addr`。
3. 继续检查后续元素是否仍位于同一 cache line。
4. 记录每个元素在 cache line 内的偏移 `line_offsets`。
5. 对 `line_addr` 发起一次大小为 `line_size` 的读请求。
6. response 返回后，从 cache line 数据中按偏移切出各元素。
7. 每个元素调用 `on_item_load()` 入队。

该实现可以减少同一行内连续元素的访存请求数量，更接近硬件 DMA 或 burst 访存行为。

### 9.4 写入矩阵寄存器

当最后一个元素读完时，`on_item_load()`：

1. 获取当前矩阵访存指令。
2. 动态转换为 `MatrixMicroInst`。
3. 通过 `getWritableRegOperand()` 获取目标矩阵寄存器。
4. 根据 `DST_SIZE` 选择 `uint8_t`、`uint16_t`、`uint32_t` 或 `uint64_t` 视图。
5. 把 `dataQ` 中的元素写入 `row_idx = microIdx` 对应的矩阵寄存器行。
6. 调用 `AMEMemUnit::finishCurrentMemInst()`。
7. 清除 `occupied` 并释放临时数据队列。

## 十、MemUnitWriteTiming 写时序模块

### 10.1 参数

`MemUnitWriteTiming` 参数与读模块一致：

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `channel` | `0` | AME memory channel 编号 |
| `cacheLineSize` | `8` | 内存/cache line 粒度，单位 byte |
| `VRF_LineSize` | `16` | 矩阵寄存器行大小，单位 byte |

### 10.2 初始化与源数据准备

`initialize()` 先读取源矩阵寄存器：

1. 获取当前矩阵访存指令。
2. 转换为 `MatrixMicroInst`。
3. 通过 `getRegOperand()` 读取源矩阵寄存器。
4. 根据 `microIdx` 选择要 store 的矩阵行。
5. 按 `DST_SIZE` 选择元素类型。
6. 调用 `queueStoreRowFromMatReg()` 把该行所有元素复制到 `dataQ`。

这一步把矩阵寄存器行转换成字节元素队列，后续写请求只需从 `dataQ` 取数据。

### 10.3 cache line 合并写

`writeFunction()` 与读模块类似，也是按 cache line 合并：

1. 根据 `writeIndex` 计算当前元素地址和 cache line 地址。
2. 找出同一 cache line 内的连续元素。
3. 计算实际要写的连续字节区间。
4. 从 `dataQ` 复制对应元素到临时连续 buffer。
5. 调用 `AMEInterface::writeAMEMem()` 发起 timing write request。
6. 写响应返回后调用 `on_item_store()`。
7. 如果是最后一批元素，则完成本条访存指令。

### 10.4 完成处理

最后一批写请求返回时，`on_item_store()`：

1. 确认当前访存指令仍存在。
2. 输出调试信息。
3. 调用 `AMEMemUnit::finishCurrentMemInst()`。
4. 清除 `occupied`。
5. 释放 `dataQ`。

## 十一、AME 内存访问与响应匹配

### 11.1 AMEPacket

`AMEPacket` 继承自 gem5 `Packet`，增加：

| 字段 | 含义 |
| --- | --- |
| `reqId` | AME 内部分配的唯一请求编号 |
| `channel` | 请求所属通道 |

`reqId` 用于在 response 返回时定位对应 pending request，`channel` 用于重试队列和多通道扩展。

### 11.2 Pending Request 队列

`AMEInterface` 使用 `AME_PendingReqQ` 保存尚未完成的请求。读写请求分别封装为：

| 类型 | 回调形式 |
| --- | --- |
| `AME_R_ReqState` | `callback(uint8_t *data, uint8_t size)` |
| `AME_W_ReqState` | `callback()` |

每次读写请求发出前：

1. 分配 `uniqueReqId`。
2. 创建对应 `AME_ReqState`。
3. 插入 `AME_PendingReqQ`。
4. 调用端口发起 timing request。

### 11.3 响应处理

`recvTimingResp()` 收到 response 后：

1. 根据 packet 的 `reqId` 在 pending 队列中查找请求。
2. 找到后把 packet 绑定到对应 pending state。
3. 从队首开始，按请求发出顺序执行所有已经匹配的回调。
4. 删除 pending state 和 packet。

该策略允许 memory system 乱序返回响应，但 AME 回调仍按请求提交顺序执行，降低上层状态机复杂度。

### 11.4 地址翻译与 memory port

`AMEMemPort::startTranslation()` 使用线程上下文中的 MMU：

```cpp
tc->getMMUPtr()->translateTiming(req, tc, translation, mode);
```

随后如果 `translation->fault == NoFault`，则创建 `AMEPacket` 并调用 `sendTimingReq()` 发往 memory system。当前实现要求一次请求不能跨 page：

```cpp
assert(page1 == page2);
```

如果 `sendTimingReq()` 失败，packet 会进入 `laCachePktQs[channel]`，等待 `recvReqRetry()` 重新发送。

## 十二、关键参数汇总

### 12.1 AMEInterface 参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `systolicArrayCore` | `SystolicArrayCore()` | AME 计算核心 |
| `instQueueSize_` | `1` | AME 内部可接收的指令队列深度 |
| `inst_queue` | `InstQueue()` | AME 内部调度器 |
| `system` | `Parent.any` | 用于获取 requestor id |
| `ame_mmu` | `AMEMemUnit()` | AME 访存单元 |
| `mem_port` | RequestPort | AME 普通内存访问端口 |

### 12.2 InstQueue 参数

`InstQueue` 当前没有显式 Python 参数，主要作为 `TickedObject` 使用。队列深度由 `AMEInterface.instQueueSize_` 限制。

### 12.3 SystolicArrayCore 参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `opLat` | `1` | PE 操作延迟 |
| `arrayRows` | `8` | 阵列行数 |
| `arrayCols` | `8` | 阵列列数 |
| `peRowsPerTile` | `1` | 每 tile PE 行数 |
| `peColsPerTile` | `4` | 每 tile PE 列数 |

### 12.4 AMEMemUnit 参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `memReader` | `MemUnitReadTiming()` | 读 timing 子模块 |
| `memWriter` | `MemUnitWriteTiming()` | 写 timing 子模块 |

### 12.5 读写 timing 参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `channel` | `0` | AME 访存通道 |
| `cacheLineSize` | `8` | cache line 合并粒度 |
| `VRF_LineSize` | `16` | 矩阵寄存器行大小 |

## 十三、当前实现完成情况

### 13.1 已完成内容

1. 完成 AME 作为 MinorCPU 参数对象的接入。
2. 完成 `AMEInterface` 顶层 SimObject 声明和 C++ 实现。
3. 完成 AICB 风格 issue 握手接口。
4. 完成矩阵计算指令和矩阵访存指令的入队分流。
5. 完成 `InstQueue` 的 tick 驱动调度逻辑。
6. 完成矩阵计算指令到 `SystolicArrayCore` 的发射。
7. 完成脉动阵列核心的参数化创建、PE/tile 组织、数据注入、MAC 推进、累加和写回。
8. 完成 `AMEMemUnit` 对矩阵访存微指令的解析。
9. 完成矩阵 load 的按 cache line 合并读、响应拆分、矩阵寄存器写入和完成回调。
10. 完成矩阵 store 的矩阵寄存器行读取、按 cache line 合并写和完成回调。
11. 完成 AME memory port 的 timing request/response 框架。
12. 完成 AME pending request 的 `reqId` 匹配和有序回调。
13. 增加 `AMEInterface` 和 `AMEMMU` 调试标志。

### 13.2 当前限制

1. `instQueueSize_` 默认值为 1，AME 默认一次只接收很少的未完成指令。
2. `AMEMemPort` 当前只创建 1 个 channel，多通道扩展接口存在但没有充分使用。
3. `AMERegPort` 已实现读写端口，但主路径目前以普通 memory port 和矩阵寄存器直接读写为主。
4. `Location::matrix_rf` 分支在读写 timing 模块中仍标记为未完工。
5. 访存请求要求不跨 page，否则触发断言。
6. `Tlb_Translation::markDelayed()` 尚未实现，对复杂 timing TLB 延迟处理还不完整。
7. `readAMEMem()` 和 `readAMEReg()` 的回调 size 参数类型为 `uint8_t`，对于超过 255 byte 的请求存在表达能力限制。
8. `MemUnitReadTiming` 当前按完整 `line_size` 读取 cache line，读放大可能较明显。
9. `MemUnitWriteTiming` 写请求按实际连续字节区间写，不一定写完整 cache line，读写粒度不完全对称。
10. `InstQueue` 中计算队列和访存队列的仲裁策略较简单，还没有复杂乱序、优先级或资源冲突策略。

## 十四、架构特点与设计评价

### 14.1 优点

1. AME 与 MinorCPU 解耦较清晰，CPU 只负责识别、提交和等待完成。
2. `AMEInterface` 集中管理 issue、端口和 pending request，顶层边界明确。
3. 计算路径和访存路径分队列管理，便于后续扩展独立的调度策略。
4. 脉动阵列核心采用参数化阵列和 tile 结构，具备调整阵列规模的基础。
5. 读写 timing 模块实现了按 cache line 合并，具备基本的访存时序建模意识。
6. pending request 机制允许 memory response 乱序返回，同时保持上层回调有序。
7. load/store 都以矩阵寄存器一行为基本粒度，和矩阵微指令的 `microIdx` 模型匹配。

### 14.2 风险与不足

1. CPU commit 阶段对 AME 等待状态的处理依赖 `waiting_ame_engine` 和 `completed_mma_inst`，需要进一步验证异常、flush、分支误预测等情况下的状态恢复。
2. 当前 AME 访存路径只处理部分矩阵访存形式，对 stride、transpose、indexed 或更复杂布局的支持还需要扩展。
3. TLB timing translation 的延迟和 fault 处理还不够完整。
4. `InstQueue` 对计算和访存的并行性利用有限，默认队列深度也限制了吞吐。
5. 脉动阵列 `reset()` 在调度代码中被注释提示曾导致 segment fault，说明执行完成后的状态清理仍需仔细验证。
6. 统计项目前只包含 cache line 读写请求数量，缺少 AME 指令数、队列等待周期、计算周期、访存阻塞周期等性能分析指标。

## 十五、后续工作建议

### 15.1 功能完善

1. 完善 `Location::matrix_rf` 分支，支持矩阵寄存器文件内部搬运或寄存器端口访问。
2. 完善 TLB delayed translation，正确处理 `markDelayed()`、延迟完成事件和 fault。
3. 支持跨 page 矩阵访存请求，将大请求拆分到不同 page。
4. 扩展矩阵访存指令类型，覆盖 load、store、transpose、stride、widen 等组合。
5. 增加 AME flush/取消机制，以适配 CPU 分支误预测和异常路径。

### 15.2 性能建模

1. 增加 AME 指令计数、计算指令计数、访存指令计数。
2. 增加队列占用周期、队列等待周期、执行周期和内存阻塞周期统计。
3. 将 `cacheLineSize` 与系统 cache line 参数联动，避免手工默认值不一致。
4. 增加多 channel 访存配置，验证多请求并发能力。
5. 扩展 `instQueueSize_`，并实现更明确的计算/访存仲裁策略。

### 15.3 正确性验证

1. 编写矩阵 load/store 单元测试，覆盖 8/16/32/64 bit 元素。
2. 编写矩阵乘加测试，覆盖整数、浮点、有符号、无符号和 widen。
3. 构造非 cache line 对齐地址测试，验证合并读写和偏移拆分。
4. 构造跨 cache line 但不跨 page 的访存测试。
5. 构造异常地址、TLB fault 和 cache back pressure 测试。
6. 对比标量参考模型，验证矩阵寄存器最终结果。

## 十六、总结

当前 GEM5 AME 扩展已经形成较完整的矩阵加速器微架构雏形。系统从 MinorCPU 提交阶段接入，通过 `AMEInterface` 提供 AICB 风格指令握手，再由 `InstQueue` 将矩阵计算和矩阵访存指令分发到 `SystolicArrayCore` 与 `AMEMemUnit`。计算路径已经具备参数化脉动阵列、数据类型解析、多周期推进、累加和矩阵寄存器写回能力；访存路径已经具备矩阵微指令解析、按行搬运、按 cache line 合并读写、pending request 匹配和完成回调能力。

从架构原理看，AME 扩展采用了与 CPU 解耦的异步协处理器模型，适合表达矩阵指令长延迟、多周期和高数据并行的特征。从实现状态看，当前代码已经不只是接口骨架，而是具备计算和基本访存闭环。但在 TLB 延迟、跨页访问、多通道并发、异常/flush、统计指标和完整矩阵访存指令覆盖方面仍有后续完善空间。

总体而言，该 AME 扩展为在 gem5 中研究 RISC-V 矩阵 ISA、脉动阵列微架构和矩阵访存时序提供了可继续迭代的基础框架。
