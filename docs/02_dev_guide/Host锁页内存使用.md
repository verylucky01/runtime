# Host锁页内存使用

在CANN编程框架中，Host内存可以是**Pageable内存**，也可以是**Page-Locked内存**（也称锁页内存或Pinned内存）：

-   **Pageable内存**，由操作系统统一管理。开发者可使用malloc、mmap等传统接口申请内存，使用free、munmap等接口释放内存**。**当内存压力较大时，Pageable内存会被换出到后备存储提供的交换空间中。当Pageable内存中的数据被传输到Device时，数据首先会被复制到缓冲区，然后通过DMA（Direct Memory Access）通道传输到Device。
-   **Page-Locked内存，**即锁页内存。开发者需要用Runtime提供的API进行锁页内存的申请和释放，例如aclrtMallocHost、aclrtFreeHost等接口**。**对于锁页内存，虚拟页与物理页的映射关系固定，在其生命周期内不会被换出至交换空间。当锁页内存中的数据被传输至Device时，直接通过DMA通道传输，无需经过缓冲区，传输性能更优。

    在Runtime中，**使用锁页内存的好处如下**：

    -   设备可以直接通过DMA访问主机内存，无需经过缓冲区，可以提供更好的传输性能；
    -   数据搬运过程无需CPU参与，可以实现数据的异步传输；
    -   数据的异步传输，使传输过程和计算过程可以相互掩盖，减少传输+计算的整体时长。

锁页内存可直接通过aclrtMallocHost接口申请，示例代码如下。如果需要在申请时指定内存的其他配置，例如自定义模块ID、配置VA（virtual address）一致性等，也可以使用aclrtMallocHostWithCfg接口申请。

```
// 资源初始化
......

// 申请锁页内存
void *hostPtr = NULL;
aclrtMallocHost(&hostPtr, size);

// 申请Device内存
void *devicePtr = NULL;
aclrtMalloc(&devicePtr, size, ACL_MEM_MALLOC_NORMAL_ONLY);

// 内存初始化
......

// 异步H2D
aclrtMemcpyAsync(devicePtr, size, hostPtr, size, ACL_MEMCPY_HOST_TO_DEVICE, stream);

// 等待异步拷贝完成
aclrtSynchronizeStream(stream);

// 资源释放
if (devicePtr) (void)aclrtFree(devicePtr);
if (hostPtr) (void)aclrtFreeHost(hostPtr);
......
```

若使用malloc/mmap等内存管理接口申请Pageable内存，当前Runtime提供了aclrtHostRegisterV2接口，用于将Pageable内存转换为锁页内存，供Device访问。请注意，当OS内核版本为5.10或更低时，此方法会导致异常，此时应通过aclrtMallocHost接口申请锁页内存。

```
// 申请Pageable内存
void *hostPtr = malloc(size);

// 注册为锁页内存(ACL_HOST_REG_PINNED)，并映射到Device(ACL_HOST_REG_MAPPED)
aclrtHostRegisterV2(hostPtr, size, ACL_HOST_REG_PINNED|ACL_HOST_REG_MAPPED);

// 获取Device地址
void *devicePtr = NULL;
aclrtHostGetDevicePointer(hostPtr, &devicePtr, 0);

// 任务通过Device地址访问对应内存
...

// 资源释放
aclrtHostUnregister(hostPtr);
free(hostPtr);
```

若涉及Host内存的VA（virtual address）一致性：

-   Host内存可以通过aclrtHostRegister接口注册到Device，根据Host指针映射得到Device指针，供Device使用；默认情况下，对于同一块Host内存，Host和Device看到的虚拟地址是不同的；
-   如果需要Host和Device的虚拟地址保持一致，可以使用aclrtMallocHostWithCfg接口申请锁页内存，并在aclrtMallocConfig参数中指定ACL\_RT\_MEM\_ATTR\_VA\_FLAG属性，后续再注册获取到的Device虚拟地址即可与Host虚拟地址一致。

