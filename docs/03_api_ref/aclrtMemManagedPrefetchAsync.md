# aclrtMemManagedPrefetchAsync

## 产品支持情况


| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | ☓ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 功能说明

实现统一虚拟内存（Unified Virtual Memory, UVM）的预取。

本接口操作的内存必须是通过[aclrtMemAllocManaged](aclrtMemAllocManaged.md)接口分配的。本接口是异步接口，调用接口成功仅表示任务下发成功，不表示任务执行成功,调用本接口后，需调用同步等待接口（例如，[aclrtSynchronizeStream](aclrtSynchronizeStream.md)）确保内存预取的任务已执行完成。

## 函数原型

```
aclError aclrtMemManagedPrefetchAsync(const void* ptr, size_t size, aclrtMemManagedLocation location, uint32_t flags, aclrtStream stream)
```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ptr | 输入 | 待预取的内存地址，地址范围必须在UVM内存范围之内，即[0x90000000000ULL, 0x90000000000ULL+3T)。 |
| size | 输入 | 待预取的内存长度，单位Byte，要求2MB对齐。取值范围为(0, 3T]。 |
| location | 输入 | 物理内存的位置信息，location参数包含id和type两个成员。<br>类型定义请参见[aclrtMemManagedLocation](aclrtMemManagedLocation.md)。 |
| flags | 输入 | 预留参数。当前固定配置为0。 |
| stream | 输入 | 指定执行内存预取任务的stream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

