# AME 矩阵扩展加速器架构报告

> 阅读范围：`src/cpu/minor/AME` 目录下的全部源码，包括 `AME/MMU`。
>
> 说明：为了确认 AME 与 MinorCPU 的连接方式，也查看了少量 `AME` 目录外的接入代码。

## 1. 总体概述

AME 当前被设计为挂接在 MinorCPU 上的矩阵扩展加速器接口。它的核心目标是把矩阵相关指令从 MinorCPU 中分离出来，由 AME 自己维护指令队列、驱动矩阵计算核心，并在指令完成后通过回调通知 CPU 流水线继续提交。

目前代码中已经搭好的主要部分是矩阵计算指令路径：MinorCPU 可以识别矩阵指令，将其交给 `AMEInterface`，再由 `InstQueue` 发射到 `SystolicArrayCore` 执行。访存路径的类和接口已经声明，但 `AME/MMU` 目录下的 `.cc` 文件基本还是骨架实现，尚未真正完成矩阵 load/store 的执行逻辑。

## 2. 整体架构

```text
MinorCPU Execute / Commit
        |
        | requestGrant()
        | sendInst()
        v
+------------------+
|   AMEInterface   |
+------------------+
        |
        v
+------------------+
|    InstQueue     |
|  TickedObject    |
+------------------+
   |            |
   |            |
   v            v
Instruction   Memory
 Queue        Queue
   |            |
   v            v
Systolic     AMEMemUnit
ArrayCore    |
             +--> MemUnitReadTiming
             +--> MemUnitWriteTiming
```

核心模块分工如下：

| 模块 | 主要作用 |
| --- | --- |
| `AMEInterface` | AME 顶层入口，负责接收 CPU 发来的矩阵指令，并转发到计算或访存路径 |
| `InstQueue` | AME 内部队列和周期驱动器，维护计算队列与访存队列 |
| `SystolicArrayCore` | 矩阵计算核心，执行 `SystolicMMA` 类指令 |
| `AMEMemUnit` | 预期的矩阵访存控制单元，目前仍未完整实现 |
| `MemUnitReadTiming` | 预期负责矩阵读请求流的时序控制，目前为占位实现 |
| `MemUnitWriteTiming` | 预期负责矩阵写请求流的时序控制，目前为占位实现 |
| `AMEMemPort` | AME 访问普通内存/cache 的端口 |
| `AMERegPort` | AME 访问寄存器空间的端口 |

## 3. 指令流动过程

### 3.1 MinorCPU 到 AME

MinorCPU 在 `execute.cc` 中对矩阵指令进行特殊处理。矩阵指令不会按照普通指令那样进入常规功能单元执行，而是在提交阶段被送入 AME。

大致流程如下：

1. MinorCPU 识别到矩阵相关指令，例如 `SystolicMMA` 或矩阵访存类指令。
2. 该指令进入 MinorCPU 的 in-flight 队列。
3. commit 阶段调用 `cpu.ameInterface->requestGrant(inst)`，询问 AME 是否能接收该指令。
4. 如果 AME 可以接收，则调用 `cpu.ameInterface->sendInst(...)`。
5. `sendInst()` 根据指令类型将其放入 AME 内部队列。
6. MinorCPU 等待 AME 执行完成后触发回调，再继续完成该指令的提交。

### 3.2 AME 内部指令分发

`AMEInterface` 将指令分成两条路径：

| 指令类型 | 队列 | 后续模块 |
| --- | --- | --- |
| 矩阵计算指令 | `Instruction_Queue` | `SystolicArrayCore` |
| 矩阵访存指令 | `Memory_Queue` | `AMEMemUnit` |

对于矩阵计算指令，`InstQueue` 会启动 ticking，并在每个周期推进 `SystolicArrayCore`。当计算核心输出有效后，AME 写回结果，并通过回调通知 MinorCPU 该指令完成。

对于矩阵访存指令，当前架构上已经设计为由 `Memory_Queue` 送入 `AMEMemUnit`，再进一步调用读/写 timing 子模块。但这条路径目前还没有真正实现完整行为。

## 4. 主要代码文件

| 文件 | 内容 |
| --- | --- |
| `AME/AMEInterface.py` | 声明 `AMEInterface` 和 `InstQueue` 两个 SimObject |
| `AME/ame_interface.hh/.cc` | AME 顶层接口、CPU 侧指令接收、内存/寄存器请求端口 |
| `AME/inst_queue.hh/.cc` | AME 内部队列和 tick 调度逻辑 |
| `AME/packet.hh` | AME 自定义 packet，携带 request id 和 channel |
| `AME/req_state.hh` | AME pending 请求状态，用于响应匹配和回调 |
| `AME/defines.hh` | 矩阵访存相关枚举定义 |
| `AME/MMU/MMU.py` | 声明 `AMEMemUnit`、`MemUnitReadTiming`、`MemUnitWriteTiming` |
| `AME/MMU/ame_mem_unit.hh/.cc` | AME 矩阵访存单元，当前 `.cc` 基本未实现 |
| `AME/MMU/read_timing_unit.hh/.cc` | 矩阵读 timing 单元，当前 `.cc` 基本未实现 |
| `AME/MMU/write_timing_unit.hh/.cc` | 矩阵写 timing 单元，当前 `.cc` 基本未实现 |
| `AME/SConscript`、`AME/MMU/SConscript` | 构建 AME 和 MMU 相关源码 |

## 5. 当前已经具备的能力

当前代码已经具备以下基础能力：

- AME 作为 MinorCPU 的一个参数对象接入。
- MinorCPU 中已经加入矩阵指令的特殊处理逻辑。
- `AMEInterface` 能接收矩阵指令。
- `InstQueue` 能作为 `TickedObject` 驱动 AME 内部执行。
- `SystolicMMA` 类指令可以进入 `Instruction_Queue`，并被发射到 `SystolicArrayCore`。
- `SystolicArrayCore` 完成后，可以通过回调通知 MinorCPU。
- AME 内部已经有 pending request 框架，用于匹配内存/寄存器访问响应。
- AME 已经声明了普通内存端口 `AMEMemPort` 和寄存器端口 `AMERegPort`。

## 6. 尚未实现或仍是骨架的部分

目前未完成的重点集中在 `AME/MMU` 目录下的 `.cc` 文件。它们已经声明了未来需要的类和接口，但还没有真正实现矩阵访存功能。

另外，当前 `isbussy()` 和 `requestGrant()` 主要围绕计算队列实现，对访存队列和 MMU 忙闲状态考虑还不完整。后续实现访存路径时，需要同步补齐这些状态判断。

## 7. 预期的矩阵访存实现方向

AME 访存路径可以按以下方式补全：

1. `AMEMemUnit::issue()` 负责解析矩阵访存指令。
   - 判断是 load 还是 store。
   - 提取元素宽度、tile 参数、基地址、stride、操作数量等信息。
   - 根据访存类型选择读单元或写单元。

2. `MemUnitReadTiming` 负责读请求流。
   - 支持 unit-stride load。
   - 后续扩展 strided load 和 indexed load。
   - 通过 `AMEInterface::readAMEMem()` 访问内存。
   - 将读回数据交给矩阵寄存器或后续计算模块。

3. `MemUnitWriteTiming` 负责写请求流。
   - 支持 unit-stride store。
   - 后续扩展 strided store 和 indexed store。
   - 通过 `AMEInterface::writeAMEMem()` 写回内存。

4. 完成信号需要回传到 `InstQueue`。
   - `AMEMemUnit` 操作完成后清除 busy 状态。
   - `InstQueue` 弹出 `Memory_Queue` 队首。
   - 执行原始 dependency callback，通知 MinorCPU 访存指令完成。

5. 地址翻译需要恢复。
   - 当前 `AMEMemPort::startTranslation()` 中真正的 TLB timing translation 被注释掉。
   - 后续需要补齐 delayed translation 和 fault 处理，保证虚拟内存场景下正确执行。

## 8. 当前进度总结

总体来看，AME 目前已经完成了“矩阵计算指令从 MinorCPU 分发到 AME，再由 AME 驱动脉动阵列核心执行”的基础框架。`AMEInterface`、`InstQueue` 和 `SystolicArrayCore` 之间的关系已经比较清楚，计算路径有初步可用的调度逻辑。

尚未完成的是矩阵访存路径。`AME/MMU` 目录下的 `.hh` 文件已经把访存单元、读 timing 单元、写 timing 单元的结构搭出来了，但 `.cc` 文件基本还是空实现。后续主要工作应集中在 `AMEMemUnit::issue()`、`MemUnitReadTiming`、`MemUnitWriteTiming`、访存完成回调和地址翻译这几个部分。

在当前状态下，可以认为 AME 的计算路径已经有了雏形，访存路径仍处于设计和占位阶段。

