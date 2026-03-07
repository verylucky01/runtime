# aclrtGetFunctionSize

## 产品支持情况


| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 功能说明

根据核函数句柄获取核函数代码段大小。

## 函数原型

```
aclError aclrtGetFunctionSize(aclrtFuncHandle funcHandle, size_t *aicSize, size_t *aivSize)
```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| funcHandle | 输入 | 核函数句柄。 |
| aicSize | 输出 | 在AI Core或Cube Core上执行算子的代码段大小，单位Byte。<br>如果算子仅在Vector Core上执行，则该值为0。 |
| aivSize | 输出 | 在Vector Core上执行算子的代码段大小，单位Byte。<br>如果算子仅在AI Core或Cube Core上执行，则该值为0。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

