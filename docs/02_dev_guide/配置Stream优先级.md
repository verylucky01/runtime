# 配置Stream优先级

在运行时，Device上的调度器会依据各个Stream的优先级来决定任务的执行顺序。高优先级Stream中待执行的任务将优先于低优先级Stream中的任务得到调度，但不会抢占已处于运行状态的低优先级任务。Device在执行过程中不会动态重新评估任务队列，因此提升Stream的优先级不会中断正在执行的任务。

Stream的优先级主要用于影响任务的调度顺序，而非强制规定严格的执行序列。用户可以通过调整Stream的优先级来引导任务的执行顺序，但无法以此强制保证任务间的绝对顺序。

调用aclrtCreateStreamWithConfig接口创建Stream时可指定Stream的优先级。允许设置的优先级范围，可以通过aclrtDeviceGetStreamPriorityRange接口获取最小优先级、最大优先级。

以下为示例代码，不可以直接拷贝编译运行，仅供参考。

```
// 查询当前设备支持的Stream最小、最大优先级
aclrtDeviceGetStreamPriorityRange(&leastPriority, &greatestPriority);

// 创建具有最高和最低优先级的Stream
aclrtStream stream_high;
aclrtStream stream_low;
aclrtCreateStreamWithConfig(&stream_high, greatestPriority, ACL_STREAM_FAST_LAUNCH);
aclrtCreateStreamWithConfig(&stream_low, leastPriority, ACL_STREAM_FAST_LAUNCH);
```

Stream的优先级在Device范围内生效，而不是在Context范围内生效。

