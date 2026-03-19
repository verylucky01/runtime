# aclmdlRIDestroyRegisterCallback

## 产品支持情况


| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 功能说明

注册一个回调函数，该函数将在销毁模型运行实例后被调用。

## 函数原型

```
aclError aclmdlRIDestroyRegisterCallback(aclmdlRI modelRI, aclrtCallback func, void *userData)
```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelRI | 输入 | 模型运行实例。 |
| func | 输入 | 回调函数，回调函数的原型为：<br>typedef void (*aclrtCallback)(void *userData); |
| userData | 输入 | 待传递给回调函数的用户数据指针。<br>可以为NULL,表示不需要额外传递数据。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

