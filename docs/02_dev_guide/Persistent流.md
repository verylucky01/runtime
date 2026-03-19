# Persistent流

非Persistent流上的任务在执行完成之后从Stream出队。如果要多次执行某个任务，需要在非Persistent流上多次下发该任务。

Runtime提供了Persistent流支持任务的持久化。在Persistent流上下发的任务不会被立即执行，任务执行完成后也不会被立即销毁。只有在销毁Persistent流时，相关的任务才会被销毁。

调用aclrtCreateStreamWithConfig接口创建Persistent流，Persistent流需要与模型运行实例创建绑定，支持模型的反复执行。以下为示例代码，不可以直接拷贝编译运行，仅供参考：

```
// 创建Persistent stream
aclrtStream stream;
aclrtCreateStreamWithConfig(&stream, 0, ACL_STREAM_PERSISTENT);

// 构建一个模型运行实例
aclmdlRI modelRI;
aclmdlRIBuildBegin(&modelRI, 0);

// 把Persistent stream绑定到模型运行实例
aclmdlRIBindStream(modelRI, stream, ACL_MODEL_STREAM_FLAG_HEAD);

// 在Persistent流上下发任务
......

// 标记下发任务结束
aclmdlRIEndTask(modelRI, stream);

// 结束模型运行实例构建
aclmdlRIBuildEnd(modelRI, nullptr);

// 在默认stream多次执行模型运行实例
aclmdlRIExecute(modelRI, -1);
aclmdlRIExecute(modelRI, -1);

// 解除绑定
aclmdlRIUnbindStream(modelRI, stream);

// 销毁资源
aclrtDestroyStream(stream);
aclmdlRIDestroy(modelRI);
......
```

