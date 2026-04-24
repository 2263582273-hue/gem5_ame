# AME Architecture README

## 1. 总体概述

AME 是挂接在 gem5 `MinorCPU` 上的矩阵扩展执行部件，用于承接矩阵计算和矩阵访存类指令。当前实现采用“CPU 提交，AME 异步执行，完成后回调 CPU”的基本模式：`MinorCPU` 在issue提交阶段将矩阵指令发送给 `AMEInterface`，AME 再根据指令类型分发到计算路径或访存路径。

目前，矩阵计算路径已经具备较完整的执行框架；矩阵 load 路径已经能够从内存读出数据并写入目标矩阵寄存器；store 路径仍处于骨架阶段。

## 2. 整体架构和核心模块

整体结构如下：

```text
MinorCPU Execute: Issue/Commit
        |
        v
   AMEInterface
        |
        v
     InstQueue
      /     \
     /       \
Systolic   AMEMemUnit
ArrayCore     |
              +-- MemUnitReadTiming
              +-- MemUnitWriteTiming
```

核心模块职责如下：

- `AMEInterface`：AME 顶层入口，负责接收 CPU 发来的矩阵指令，提供 AICB 风格的 `Issue` 接口，并把指令送入 AME 内部队列。
- `InstQueue`：AME 内部调度器，维护计算队列和访存队列，并在每个 tick 推进相应执行单元。
- `SystolicArrayCore`：矩阵计算核心，执行 `SystolicMMA` 类指令，并将结果写回矩阵寄存器。
- `AMEMemUnit`：矩阵访存控制单元，负责解析矩阵访存微指令并驱动读写时序子模块。
- `MemUnitReadTiming`：矩阵 load 路径的时序控制器，负责发起内存读取、缓存读回元素、并在完成后写入矩阵寄存器。
- `MemUnitWriteTiming`：矩阵 store 路径预留模块，目前仅保留骨架。
- `AMEMemPort` / `AMERegPort`：AME 访问普通内存和寄存器空间的底层端口。

## 3. 指令执行的控制流

AME 指令的控制流分为 CPU 侧提交和 AME 侧执行两段。

1. `MinorCPU` 在 `execute.cc` 的提交逻辑中识别矩阵相关指令。
2. CPU 调用 `AMEInterface::issue_aicb()`，以 AICB `Issue` 通道的形式向 AME 发起一次握手。
3. `issue_aicb()` 同拍给出 `ready`、`accept`、`rdValid`、`wbValid`；若握手成功，则立即调用 `sendInst()` 将指令正式送入 AME。
4. `sendInst()` 将指令按类型放入 `Instruction_Queue` 或 `Memory_Queue`。
5. `InstQueue` 驱动对应执行单元：
   - 计算指令进入 `SystolicArrayCore`
   - 访存指令进入 `AMEMemUnit`
6. 指令完成后，AME 通过回调通知 `MinorCPU`，CPU 再解除等待状态并完成commit。

这个模型的关键点是：CPU 不直接参与 AME 内部执行过程，只在发射时握手、在完成时接收通知。

## 4. 访存（load）指令的数据流

当前 load 路径的数据流如下：

1. `AMEMemUnit::issue()` 解析矩阵 load 微指令，提取基地址、stride、目标寄存器、元素宽度等信息。
2. `MemUnitReadTiming` 根据 `ea + DST_SIZE * index` 逐元素生成访存地址，并按 cache line 聚合读取。
3. 读请求通过 `AMEInterface::readAMEMem()` 发往内存系统。
4. 内存返回整行数据后，`MemUnitReadTiming` 从该行中切出对应元素，依次压入 `dataQ`。
5. 当本条微指令的所有元素读完后，`on_item_load()`：
   - 从 `AMEMemUnit` 取得当前正在执行的 memory instruction
   - 通过该指令关联的 `ExecContext` 获取目标矩阵寄存器的可写指针
   - 将 `dataQ` 中的元素写入目标矩阵寄存器对应的 `microIdx` 行
   - 打印写回后的寄存器内容
6. 写回完成后，访存指令被标记为完成，`InstQueue` 在后续周期中弹出该指令并回调 CPU。

当前实现中，load 路径已经具备“内存读取 -> 元素缓存 -> 写回矩阵寄存器”的完整基本闭环。

## 5. 和 CPU 的接口设置

AME 与 `MinorCPU` 的接口以 `AMEInterface` 为中心，主要包括两类：

- 指令接口
  - `issue_aicb(...)`：AICB `Issue` 通道风格接口，输入 `valid/hartId/inst/instId` 等信息，输出 `AICBIssueResp`
  - `sendInst(...)`：在握手成功后接收指令并送入 AME 内部队列
  - `requestGrant(...)`：内部容量判断，目前作为 `ready` 的一部分使用

- 数据与执行上下文接口
  - `ExecContextPtr`：CPU 在发射指令时一并传给 AME，用于后续读取源寄存器、获取目标寄存器写口、访问线程上下文和地址翻译信息
  - 完成回调：AME 执行结束后通过回调通知 CPU 更新 `completed_inst`、`waiting_ame_engine` 等状态

在接口分工上：

- CPU 负责识别矩阵指令、发起 `Issue` 握手、等待完成
- AME 负责内部排队、执行、访存和结果写回

这种划分保持了 `MinorCPU` 与 AME 的解耦，也为后续扩展 kill、route table 和更完整的 AICB 协议建模留出了空间。
