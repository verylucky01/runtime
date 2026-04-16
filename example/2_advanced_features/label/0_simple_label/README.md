# 0_simple_label

## 概述

本示例演示 CANN Runtime 的 Label 创建与按索引切换能力，适合作为基于模型运行实例的设备端流程控制最小示例。

## 产品支持情况

本样例关键接口在不同产品上的支持情况如下：

| 接口 | Atlas A3 训练系列产品/Atlas A3 推理系列产品 | Atlas A2 训练系列产品/Atlas A2 推理系列产品 |
| --- | --- | --- |
| aclrtCreateLabel | x | √ |
| aclrtCreateLabelList | x | √ |
| aclrtSetLabel | x | √ |
| aclrtSwitchLabelByIndex | x | √ |
| aclmdlRIBuildBegin | √ | √ |
| aclmdlRIBindStream | √ | √ |
| aclmdlRIEndTask | √ | √ |
| aclmdlRIBuildEnd | √ | √ |
| aclmdlRIExecuteAsync | √ | √ |
| aclmdlRIUnbindStream | √ | √ |
| aclmdlRIDestroy | √ | √ |

## 功能说明

- 创建持久化 Stream，并将其绑定到模型运行实例。
- 创建两个 Label，并把它们组织成 LabelList。
- 在设备内存中准备分支索引，在绑定后的持久化 Stream 上录入 SwitchLabelByIndex 与 SetLabel 任务。
- 执行模型运行实例并完成资源销毁。

## 说明

`aclrtSetLabel` 只支持已经通过 `aclmdlRIBindStream` 绑定到模型运行实例的 `ACL_STREAM_PERSISTENT` 类型 Stream。若直接对普通 Stream 调用该接口，运行时可能返回 `ACL_ERROR_RT_STREAM_MODEL (107005)`。

## 编译运行

环境安装详情以及运行详情请见 example 目录下的 [README](../../../README.md)。

运行步骤如下：

```bash
# ${install_root} 替换为 CANN 安装根目录，默认安装在`/usr/local/Ascend`目录
source ${install_root}/cann/set_env.sh
export ASCEND_INSTALL_PATH=${install_root}/cann

# 编译运行
bash run.sh
```

## CANN RUNTIME API

在该Sample中，涉及的关键功能点及其关键接口，如下所示：
- 初始化
    - 调用aclInit接口初始化AscendCL配置。
    - 调用aclFinalize接口实现AscendCL去初始化。
- Device与Context管理
    - 调用aclrtSetDevice接口指定用于运算的Device。
    - 调用aclrtCreateContext接口创建Context。
    - 调用aclrtDestroyContext接口销毁Context。
    - 调用aclrtResetDeviceForce接口强制复位当前运算的Device，回收Device上的资源。
- Stream管理
    - 调用aclrtCreateStreamWithConfig接口创建持久化Stream。
    - 调用aclrtCreateStream接口创建执行模型运行实例的Stream。
    - 调用aclrtSynchronizeStream接口阻塞等待Stream上任务执行完成。
    - 调用aclrtDestroyStream接口销毁Stream。
- 内存管理
    - 调用aclrtMalloc接口申请Device内存存放分支索引。
    - 调用aclrtFree接口释放Device上的内存。
- 数据传输
    - 调用aclrtMemcpy接口将Host侧的分支索引写入Device内存。
- 模型运行实例管理
    - 调用aclmdlRIBuildBegin和aclmdlRIBuildEnd接口开始和结束模型运行实例构建。
    - 调用aclmdlRIBindStream和aclmdlRIUnbindStream接口绑定和解绑持久化Stream。
    - 调用aclmdlRIEndTask接口标记绑定Stream上的任务下发结束。
    - 调用aclmdlRIExecuteAsync接口异步执行模型运行实例。
    - 调用aclmdlRIDestroy接口销毁模型运行实例。
- Label创建与切换
    - 调用aclrtCreateLabel和aclrtDestroyLabel接口创建并释放Label。
    - 调用aclrtCreateLabelList和aclrtDestroyLabelList接口组装并释放LabelList。
    - 调用aclrtSetLabel接口在Stream上设置Label。
    - 调用aclrtSwitchLabelByIndex接口根据Device内存中的分支索引执行Label切换。

## 已知 issue

暂无。
