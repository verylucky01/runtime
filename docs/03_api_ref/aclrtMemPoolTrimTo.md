# aclrtMemPoolTrimTo

**须知：本接口为试验特性，后续版本可能会存在变更，不支持应用于商用产品中。**

## 产品支持情况


| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | x |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 功能说明

收缩内存池，保留指定大小的内存。

在调用aclrtMemPoolFreeAsync接口释放内存时，内存仅归还给内存池，而不会实际释放物理内存，这可能导致内存持续被占用，进而使得aclrtMemPoolMallocAsync接口无法申请新的内存。此时，可以调用aclrtMemPoolTrimTo接口主动收缩内存池，而不影响当前正在使用的物理内存。

## 函数原型

```
aclError aclrtMemPoolTrimTo(aclrtMemPool memPool, size_t minBytesToKeep)
```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| memPool | 输入 | 内存池实例，类型为 [aclrtMemPool](aclrtMemPool.md)。 |
| minBytesToKeep | 输入 | 内存池期望收缩到的目标内存大小，单位Byte。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
