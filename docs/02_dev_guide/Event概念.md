# Event概念

Event用于同一**Device内**、**不同Stream之间**的任务同步事件。**它支持一个任务等待一个事件**，例如stream2的任务依赖stream1的任务，想保证stream1中的任务先完成，这时可创建一个Event，将该Event插入到stream1中（Event Record任务），在stream2中插入一个等待Event完成的任务（Event Wait任务）；**也支持多个任务等待同一个事件（多等一）**，例如stream2和stream3中的任务都等待Stream1中的Event完成；同时，Event支持记录**事件时间戳**信息。

一个任务等待一个事件的图示如下：

![](figures/Event_一个任务等待一个事件.png)

多个任务等待同一个事件的图示如下：

![](figures/Event_多个任务等待一个事件.png)

