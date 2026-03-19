# aclmdlRITaskParams

```
typedef struct aclmdlRITaskParams {
    aclmdlRITaskType type;     // 任务类型
    uint32_t rsv0[3];          // 预留参数
    aclrtTaskGrp taskGrp;      // 用于更新aclGraph模型的taskGrp句柄
    void* opInfoPtr;           // 存放算子shape信息的地址指针
    size_t opInfoSize;         // 算子shape信息的大小，单位为Byte
    uint8_t rsv1[32];          // 预留参数

    union {
        uint8_t rsv2[128];     // 预留参数
        struct aclmdlRIKernelTaskParams kernelTaskParams;   // 算子类型任务的参数
    };
} aclmdlRITaskParams;
```