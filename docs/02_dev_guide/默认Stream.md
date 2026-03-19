# 默认Stream

在调用aclrtSetDevice接口或aclrtCreateContext接口时，Runtime会自动创建一个默认Stream。每个Context拥有一个默认Stream。如果不同的Host线程使用相同的Context，它们将共享同一个默认Stream。

对于需要传入Stream参数的API（如aclrtMemcpyAsync），如果使用默认Stream作为入参，则直接传入nullptr。对于没有Stream入参的API（如aclrtMemcpy），则不使用默认Stream。

默认Stream不能显式调用aclrtDestroyStream接口销毁。在调用aclrtResetDevice或aclrtResetDeviceForce接口释放资源时，默认Stream会被自动销毁。

以下是在默认Stream上下发计算任务的代码示例，不可以直接拷贝编译运行，仅供参考。

```
// 指定Device（接口内部自动创建默认Stream）
aclrtSetDevice(0);

// 在默认stream上下发Host->Device复制任务、MyKernel任务、和Device->Host复制任务
aclrtMemcpyAsync(devPtrIn, size, hostPtr, hostSize, ACL_MEMCPY_HOST_TO_DEVICE, nullptr);
myKernel<<<8, nullptr, nullptr>>>(devPtrIn, devPtrOut, size);
aclrtMemcpyAsync(hostPtr, hostSize, devPtrOut, size, ACL_MEMCPY_DEVICE_TO_HOST, nullptr);

// 同步默认stream
aclrtStreamSynchronize(nullptr);

// 复位Device（接口内部自动销毁默认Stream）
aclrtResetDevice(0);
```

