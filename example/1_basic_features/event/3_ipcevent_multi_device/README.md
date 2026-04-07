## 3_ipc_event_multi_device

## 描述
本样例展示了在**多个Device**上通过 **IPC Event** 进行跨进程任务同步。  
- **生产者进程（proc_a）**：运行在Device 0上，创建一个IPC Event，记录事件并导出句柄到文件，然后等待所有消费者进程完成。  
- **消费者进程（proc_b）**：运行在其他Device（如Device 1、Device 2...）上，每个消费者读取事件句柄，打开事件，在Stream中等待该事件，然后记录事件并通知生产者。  

通过这种方式，一个生产者可以同步多个运行在不同Device上的消费者，实现分布式任务协调。

## 支持的产品型号

| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |
| Atlas A5 训练系列产品/Atlas A5 推理系列产品 | √ |

注: 该用例在A5系列产品上运行需要满足UB互联环境。

## 编译运行
环境安装详情以及运行详情请见example目录下的[README](../../README.md)。

## 运行前环境变量
运行 `bash run.sh` 前，请先在同一个 shell 中导入以下环境变量：

```bash
# ${install_root} 替换为 CANN 安装根目录，默认安装在`/usr/local/Ascend`目录
source ${install_root}/cann/set_env.sh
export ASCEND_INSTALL_PATH=${install_root}/cann
```
## CANN RUNTIME API
在该Sample中，涉及的关键功能点及其关键接口，如下所示：
- 初始化
    - 调用aclInit接口初始化AscendCL配置。
    - 调用aclFinalize接口实现AscendCL去初始化。
- Device管理
    - 调用aclrtSetDevice接口指定用于运算的Device。
    - 调用aclrtResetDevice接口复位当前运算的Device，回收Device上的资源。
- Stream管理
    - 调用aclrtCreateStream接口创建Stream。
    - 调用aclrtDestroyStream接口销毁Stream。
    - 调用aclrtSynchronizeStream接口阻塞等待Stream上任务的完成。
- Event管理（IPC扩展）
    - 调用aclrtCreateEventExWithFlag接口创建支持IPC的Event（flag=ACL_EVENT_IPC）。
    - 调用aclrtRecordEvent接口记录Event。
    - 调用aclrtSynchronizeEvent接口阻塞等待Event完成。
    - 调用aclrtQueryEventStatus接口查询Event状态。
    - 调用aclrtIpcGetEventHandle接口获取Event IPC句柄（生产者）。
    - 调用aclrtIpcOpenEventHandle接口打开IPC Event（消费者）。
    - 调用aclrtStreamWaitEvent接口阻塞Stream，等待Event完成。
    - 调用aclrtDestroyEvent接口销毁Event。

## 已知issue

   暂无

