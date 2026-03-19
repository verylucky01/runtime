# Stream和Event的行为

在与当前Device无所属关系的Stream上下发算子将会失败，示例代码如下：

```
aclrtSetDevice(0);                 // 指定Device 0作为计算设备
aclrtStream s0;                    
aclrtCreateStream(&s0);            // 在Device 0上创建Stream s0
myKernel<<<8, nullptr, s0>>>();    // 在Device 0上通过Stream s0下发算子

aclrtSetDevice(1);                 // 指定Device 1作为计算设备
aclrtStream s1;
aclrtCreateStream(&s1);            // 在Device 1上创建Stream s1
myKernel<<<8, nullptr, s1>>>();    // 在Device 1上通过Stream s1下发算子

// 算子下发失败
myKernel<<<8, nullptr, s0>>>();    // 在Device 1上通过Stream s0下发算子
```

-   当Stream所属的Device和当前操作的Device不相同时，在此Stream上调用aclrtMemcpyAsync会失败。
-   当Event和Stream关联到不同的Device上时，调用aclrtRecordEvent会失败。
-   当Event和Stream关联到不同的Device上时，调用aclrtStreamWaitEvent会失败。
-   当Event所属的Device和当前操作的Device不相同时，aclrtSynchronizeEvent和aclrtQueryEvent会成功。

