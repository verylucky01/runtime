# aclmdlRIKernelTaskParams

```
typedef struct aclmdlRIKernelTaskParams {
    aclrtFuncHandle funcHandle;  // 核函数句柄
    aclrtLaunchKernelCfg* cfg;   // 下发任务的配置信息
    void* args;                  // 存放核函数所有入参数据的地址指针
    uint32_t isHostArgs;         // 标识args的内存属性，0：device内存，1：host内存，该参数在查询接口中固定为0
    size_t argsSize;             // args参数值的大小，单位为Byte
    uint32_t numBlocks;          // 指定核函数将会在几个核上执行
    uint32_t rsv[10];            // 预留参数
} aclmdlRIKernelTaskParams;
```