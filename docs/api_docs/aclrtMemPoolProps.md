# aclrtMemPoolProps

```
typedef struct {
    aclrtMemAllocationType allocType;
    aclrtMemHandleType handleType;
    aclrtMemLocation location;
    size_t maxSize;
    unsigned char reserved[32];
} aclrtMemPoolProps;
```

| 成员名称 | 描述 |
| --- | --- |
| allocType | 内存分配类型，类型为 [aclrtMemAllocationType](aclrtMemAllocationType.md)。<br>当前仅支持 ACL_MEM_ALLOCATION_TYPE_PINNED，表示锁页内存。|
| handleType | handle 类型，类型为 [aclrtMemHandleType](aclrtMemHandleType.md)。 |
| location | 内存所在位置，类型为 [aclrtMemLocation](aclrtMemLocation.md)。<br>type当前仅支持配置为 ACL_MEM_LOCATION_TYPE_DEVICE。|
| maxSize | 内存池的大小，单位Byte。 |
| reserved | 保留字段，当前必须为全 0 字符串。 |