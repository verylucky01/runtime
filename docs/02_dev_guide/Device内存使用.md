# Device内存使用

在昇腾异构计算编程中，典型的使用场景是：通过aclrtMallocHost接口申请Host内存，通过aclrtMalloc接口申请Device内存，通过aclrtMemcpy（同步）/aclrtMemcpyAsync（异步）接口将数据从Host拷贝到Device上，算子执行过程中使用Device内存进行计算并保存结果。

以下是一段简单的示例代码。在示例代码中，两个张量从Host内存被拷贝到Device内存，在Device侧完成计算，再将结果从Device内存拷贝到Host内存：

```
int main(void)
{
    int32_t deviceId = 0;
    int64_t N = 16; 
    const size_t bytes = static_cast<size_t>(N) * sizeof(float);
    aclrtStream stream = nullptr;

    // STEP 1: 初始化、Stream创建
    aclInit(nullptr);
    aclrtSetDevice(deviceId);
    aclrtCreateStream(&stream);

    // STEP 2: 申请Host内存
    void* hostA = nullptr;
    void* hostB = nullptr;
    void* hostOut = nullptr;

    aclrtMallocHost(&hostA, bytes);
    aclrtMallocHost(&hostB, bytes);
    aclrtMallocHost(&hostOut, bytes);

    // 输入数据初始化
    ...

    // STEP 3: 申请Device内存
    void* deviceA = nullptr;
    void* deviceB = nullptr;
    void* deviceOut = nullptr;

    // 第三个参数aclrtMemMallocPolicy表示申请内存时的内存页分配策略
    // ACL_MEM_MALLOC_HUGE_FIRST 表示大页内存优先，其余定义参考API文档
    aclrtMalloc(&deviceA, bytes, ACL_MEM_MALLOC_HUGE_FIRST);
    aclrtMalloc(&deviceB, bytes, ACL_MEM_MALLOC_HUGE_FIRST);
    aclrtMalloc(&deviceOut, bytes, ACL_MEM_MALLOC_HUGE_FIRST);

    // STEP 4: 将输入数据从Host内存传输到Device内存
    aclrtMemcpy(deviceA, bytes, hostA, bytes, ACL_MEMCPY_HOST_TO_DEVICE);
    aclrtMemcpy(deviceB, bytes, hostB, bytes, ACL_MEMCPY_HOST_TO_DEVICE);

    // STEP 5: 执行计算操作，比如aclnnAdd，将计算结果保存在Device内存
    // ...

    // STEP 6: 将计算结果从Device内存传输到Host内存
    aclrtMemcpy(hostOut, bytes, deviceOut, bytes, ACL_MEMCPY_DEVICE_TO_HOST);

    // STEP 7: 在Host侧处理计算结果
    // ...

    // STEP 8: 资源清理
    // STEP 8.1: 释放Host和Device内存
    if (deviceA) (void)aclrtFree(deviceA);
    if (deviceB) (void)aclrtFree(deviceB);
    if (deviceOut) (void)aclrtFree(deviceOut);
    if (hostA) (void)aclrtFreeHost(hostA);
    if (hostB) (void)aclrtFreeHost(hostB);
    if (hostOut) (void)aclrtFreeHost(hostOut);

    // STEP 8.2: 清理设备和进程资源
    if (stream) (void)aclrtDestroyStream(stream);
    (void)aclrtResetDevice(deviceId);
    (void)aclFinalize();

    return 0;
}
```

