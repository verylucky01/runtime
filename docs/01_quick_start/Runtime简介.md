# CANN Runtime简介

CANN Runtime是CANN软件栈中负责驱动硬件执行与管理AI计算任务的核心组件，它通过提供统一的API，使得上层应用和框架能够高效利用AI处理器的硬件计算资源。其主要功能包括以下几类

-   **Device管理**：提供对计算设备设置、重置、查询和配置等功能。
-   **Memory管理**：提供设备内存和主机内存的申请、释放、内存拷贝等功能。
-   **Context管理**： Context是计算设备的执行上下文环境，提供上下文创建、销毁，切换和配置等功能。
-   **Stream管理**：Stream是Device提供的逻辑任务执行队列，提供Stream创建和销毁、属性查询和配置、Stream同步、Stream状态管理等功能。
-   **Kernel管理**：提供AI Core \(AI Cube 、AI Vector\)、AI CPU等算子注册管理和KernelLaunch功能。
-   **Event管理**：Event是用于设备内Stream间任务同步的事件，提供Event创建和销毁、事件同步、记录事件时间戳信息等功能。
-   **Notify管理**：Notify主要是跨设备间的通知，提供Notify创建和销毁、Record/Wait功能。
-   **运行时全局管理**：提供运行时初始化和进程级配置、DFX配置（例如算子数据Dump、溢出检测等）功能。
-   **ACL Graph**：提供任务捕获或编排方式构建“运行时图”功能，利用“运行时图”任务整体下发能力，节省Host调度耗时提升整体性能。

下图展示了Runtime的功能架构：

![](figures/逻辑架构图.png)

