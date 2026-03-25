# aclrtMemManagedPrefetchBatchAsync

## 产品支持情况


| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | ☓ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 功能说明

实现统一虚拟内存（Unified Virtual Memory, UVM）的批量预取。

本接口操作的内存必须是通过[aclrtMemAllocManaged](aclrtMemAllocManaged.md)接口分配的。本接口是异步接口，调用接口成功仅表示任务下发成功，不表示任务执行成功,调用本接口后，需调用同步等待接口（例如，[aclrtSynchronizeStream](aclrtSynchronizeStream.md)）确保内存预取的任务已执行完成。

## 函数原型

```
aclError aclrtMemManagedPrefetchBatchAsync(const void** ptrs, size_t* sizes, size_t count, aclrtMemManagedLocation* prefetchLocs, size_t* prefetchLocIdxs, size_t numPrefetchLocs, uint64_t flags, aclrtStream stream)
```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ptrs | 输入 | 待预取的内存地址数组，每个地址的范围都必须在UVM内存范围之内，即[0x90000000000ULL, 0x90000000000ULL+3T)。 |
| sizes | 输入 | 内存预取长度数组，用于存放每一段要预取的UVM内存长度，单位Byte。每段长度都要求2MB对齐，取值范围为(0, 3T]。 |
| count | 输入 | ptrs和sizes数组的长度。 |
| prefetchLocs | 输入 | 物理内存的位置信息数组，每个位置信息都包含id和type两个成员。<br>类型定义请参见[aclrtMemManagedLocation](aclrtMemManagedLocation.md)。 |
| prefetchLocIdxs | 输入 | 物理内存预取信息索引数组，用于指定prefetchLocs数组中的每个物理地址适用的预取范围。对于prefetchLocs[k]指定的物理地址，将预取ptrs数组中从第prefetchLocIdxs[k]个下标到第prefetchLocIdxs[k+1] – 1个下标指向元素的UVM内存地址，同时对于prefetchLocs[numPrefetchLocs -1]指定的物理地址，将预取ptrs数组中从第prefetchLocIdxs[numPrefetchLocs -1]个下标到第count - 1个下标指向元素的UVM内存地址。 |
| numPrefetchLocs | 输入 | prefetchLocs和prefetchLocIdxs数组的长度。 |
| flags | 输入 | 预留参数。当前固定配置为0。 |
| stream | 输入 | 指定执行内存预取任务的stream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

-   将ptrs中指定的数据预取到prefetchLocs中指定的物理内存区域，每个预取操作的大小由sizes指定，ptrs、sizes这两个数组必须具有count指定的相同长度。
-   在预取操作批处理中，prefetchLocs数组中的每个条目可应用于多个预取操作，具体通过prefetchLocIdxs数组指定对应物理内存区域需要预取的起始UVM地址索引。prefetchLocs和prefetchLocIdxs这两个数组必须具有numPrefetchLocs指定的相同长度。例如：若批处理包含ptrs/sizes列出的10个预取操作，其中前6个需要被预取到一块物理内存区域，后4个需要被预取到另一块物理内存区域，则numPrefetchLocs为2，prefetchLocIdxs为\{0,6\}，prefetchLocs包含两组物理内存的位置信息。注意，prefetchLocIdxs的首个条目必须为0，且每个条目必须大于前一个条目，最后一个条目应小于count。此外numPrefetchLocs必须小于等于count。

