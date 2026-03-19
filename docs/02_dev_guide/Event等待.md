# Event等待

多Stream之间任务的同步等待可以利用Event实现，例如，若stream2的任务依赖stream1的任务，想保证stream1中的任务先完成，这时可创建一个Event，调用aclrtRecordEvent接口将Event插入到stream1中（通常称为Event Record任务），调用aclrtStreamWaitEvent接口在stream2中插入一个等待Event完成的任务（通常称为Event Wait任务）。

以下为调用aclrtStreamWaitEvent接口的示例代码，不可以直接拷贝编译运行，仅供参考：

```
// 创建一个Event
aclrtEvent event;
aclrtCreateEventExWithFlag(&event, ACL_EVENT_SYNC);

// 创建stream1
aclrtStream stream1;
aclrtCreateStream(&stream1);

// 创建stream2
aclrtStream stream2;
aclrtCreateStream(&stream2);

// 在stream1上下发任务
......

// 在stream1末尾添加了一个event
aclrtRecordEvent(event, stream1);

// 在stream2上下发不依赖stream1执行完成的任务
......

// 阻塞stream2运行，直到指定event发生，也就是stream1执行完成
aclrtStreamWaitEvent(stream2, event);

// 在stream2上下发依赖stream1执行完成的任务
......

// 阻塞应用程序运行，直到stream1和stream2中的所有任务都执行完成
aclrtSynchronizeStream(stream1);
aclrtSynchronizeStream(stream2);

// 显式销毁资源
aclrtDestroyStream(stream1);
aclrtDestroyStream(stream2);
aclrtDestroyEvent(event);
......
```

