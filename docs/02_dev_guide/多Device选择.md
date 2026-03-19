# 多Device选择

一个Host搭配多Device的场景下，用户可以在Host侧应用程序中通过aclrtGetDeviceCount接口来获取当前Host上搭配的Device数量，Device按照0、1、2、... 的顺序排布。

以下为获取Device信息的代码示例，不可以直接拷贝编译运行，仅供参考：

```
// 获取Device数量及其对应的属性信息
uint32_t deviceCount;
aclrtGetDeviceCount(&deviceCount);
uint32_t deviceId;
for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
    // 按需查询设备属性信息
    aclrtDevAttr attr = ACL_DEV_ATTR_VECTOR_CORE_NUM;
    int64_t value;
    aclrtGetDeviceInfo(deviceId, attr, &value);
}
```

此时，可以随时通过aclrtSetDevice接口按**线程粒度**切换Device（不会影响其他线程）。指定Device后，后续的内存分配、Kernel执行等操作均在该Device上进行，且Stream、Event等也与当前指定的Device相关联。用户可以通过aclrtResetDevice接口释放资源，但更推荐使用aclrtResetDeviceForce接口一次性清理Device上的资源，包括默认Context、默认Stream以及在默认Context下创建的所有Stream。如果默认Context或默认Stream下的任务尚未完成，系统会等待任务完成后才释放资源。在用户程序中，若使用aclrtResetDevice接口，则需确保aclrtSetDevice和aclrtResetDevice接口的调用次数成对出现。

多Device选择的接口调用流程如下图所示：

![](figures/同步等待流程_多Device场景.png)

以下是多Device选择的代码示例，不可以直接拷贝编译运行，仅供参考：

```
// 指定Device 0作为计算设备，并将Device 0的默认Context作为当前线程的默认Context
aclrtSetDevice(0);
aclrtStream s0;                    
aclrtCreateStream(&s0);
// 执行任务1
......

// 指定Device 1作为计算设备，并将Device1的默认Context作为当前线程的默认Context
aclrtSetDevice(1);
aclrtStream s1;
aclrtCreateStream(&s1);
// 执行任务2
......

// 复位Device 1，释放计算资源，线程默认Context被释放
// 如需进行继续运行任务，需要显示指定device&context
aclrtResetDeviceForce(1);

// 复位device0，释放计算资源
aclrtResetDeviceForce(0);
```

