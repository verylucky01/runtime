# aclmdlRIDestroyUnregisterCallback

## 产品支持情况


| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 功能说明

取消通过[aclmdlRIDestroyRegisterCallback](aclmdlRIDestroyRegisterCallback.md)接口注册的回调函数。取消注册之后，销毁模型运行实例时不再调用该回调函数。

## 函数原型

```
aclError aclmdlRIDestroyUnregisterCallback(aclmdlRI modelRI, aclrtCallback func)
```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelRI | 输入 | 模型运行实例。 |
| func | 输入 | 回调函数，回调函数的原型为：<br>typedef void (*aclrtCallback)(void *userData); |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

