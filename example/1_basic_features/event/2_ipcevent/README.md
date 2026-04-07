## 2_ipcevent_sample

## 描述
本样例展示了两个进程之间通过 **IPC Event** 进行任务同步。  
- 进程A（生产者）：创建IPC事件，记录事件并导出句柄，然后等待消费者完成。  
- 进程B（消费者）：导入IPC事件句柄，等待事件，完成工作后记录事件通知生产者。  

该示例使用二进制文件传递事件句柄，展示了IPC事件的核心用法：创建、导出、导入、等待、记录、查询和销毁。

## 支持的产品型号
| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |
| Atlas A5 训练系列产品/Atlas A5 推理系列产品 | √ |

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
    - 调用aclrtResetDevice接口复位Device。
- Stream管理
    - 调用aclrtCreateStream接口创建Stream。
    - 调用aclrtDestroyStream接口销毁Stream。
    - 调用aclrtSynchronizeStream接口阻塞等待Stream任务完成。
- Event管理（IPC扩展）
    - 调用aclrtCreateEventExWithFlag接口创建支持IPC的事件（flag=ACL_EVENT_IPC）。
    - 调用aclrtRecordEvent接口记录事件。
    - 调用aclrtSynchronizeEvent接口阻塞等待事件完成。
    - 调用aclrtQueryEventStatus接口查询事件状态。
    - 调用aclrtIpcGetEventHandle接口获取事件IPC句柄（生产者）。
    - 调用aclrtIpcOpenEventHandle接口打开IPC事件（消费者）。
    - 调用aclrtStreamWaitEvent接口阻塞流，等待事件完成。
    - 调用aclrtDestroyEvent接口销毁事件。

## 已知issue

   暂无
