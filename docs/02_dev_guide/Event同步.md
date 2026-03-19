# Event同步

调用aclrtSynchronizeEvent接口阻塞当前主机线程直到指定的Event事件完成。以下为示例代码，不可以直接拷贝编译运行，仅供参考：

```
// 创建Event
aclrtEvent event;
aclrtCreateEventExWithFlag(&event, ACL_EVENT_CAPTURE_STREAM_PROGRESS);

// 创建Stream
aclrtStream stream;
aclrtCreateStream(&stream);

// 在Stream上下发任务
......

// 在Stream上记录Event
aclrtRecordEvent(event, stream);

// 阻塞应用程序运行直到Event发生
aclrtSynchronizeEvent(event);

// 显式销毁资源
aclrtDestroyStream(stream);
aclrtDestroyEvent(event);
```

