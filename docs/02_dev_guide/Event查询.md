# Event查询

调用aclrtQueryEventStatus接口可查询指定Event是否完成，非阻塞接口，Event状态包括ACL\_EVENT\_RECORDED\_STATUS\_COMPLETE（所有任务都已经执行完成）、ACL\_EVENT\_RECORDED\_STATUS\_NOT\_READY（有未执行完的任务）。

以下是通过Event实现多线程内存池复用管理机制的代码示例，不可以直接拷贝编译运行，仅供参考。

1.  在A线程中创建内存池，算子所用的内存来源于内存池，在算子后面插入Event Record任务。

    ```
    // 申请内存池
    ......
    // 创建Stream
    aclrtStream stream;
    aclrtCreateStream(&stream);
    
    // 创建Event
    aclrtEvent event;
    aclrtCreateEventExWithFlag(&event, ACL_EVENT_CAPTURE_STREAM_PROGRESS);
    
    // 在Stream中下发计算任务
    ......
    // 在算子所在stream插入event
    aclrtRecordEvent(event, stream);
    ```

2.  在B线程中调用查询接口，如果查询的Event已经完成，则代表Event Record前面的算子内存都可以被安全的复用。

    ```
    aclrtEventRecordedStatus status;
    // 查询线程A的event是否完成
    aclrtQueryEventStatus(event, &status);
    if (status == ACL_EVENT_RECORDED_STATUS_COMPLETE) {
    // event已完成，算子占用的内存可以复用
    } else {
    // 算子并未执行完成，该算子占用的内存不能被复用
    }
    
    ```

