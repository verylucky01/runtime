# aclrtFunctionGetBinary

## 产品支持情况


| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 功能说明

根据核函数句柄获取算子二进制句柄。

## 函数原型

```
aclError aclrtFunctionGetBinary(aclrtFuncHandle funcHandle, aclrtBinHandle *binHandle)
```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| funcHandle | 输入 | 核函数句柄，类型定义请参见[aclrtFuncHandle](api_docs/aclrtFuncHandle.md)。 |
| binHandle | 输出 | 标识算子二进制的句柄，类型定义请参见[aclrtBinHandle](api_docs/aclrtBinHandle.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

