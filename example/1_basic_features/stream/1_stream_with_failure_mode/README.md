# 1_stream_with_failure_mode

## 描述
本样例展示Stream设置遇错即停并且模拟运行核函数时发生错误的场景。

## 产品支持情况

本样例支持以下产品：

| 产品 | 是否支持 |
| --- | --- |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 说明
本样例包含三部分内容：普通 Stream 上的遇错即停演示、`aclrtStreamStop` 辅助演示、`aclrtStreamAbort` 辅助演示。

- `aclrtStreamStop` 在 Atlas A2/Atlas A3 上要求目标 Stream 通过 `aclrtCreateStreamWithConfig(..., ACL_STREAM_DEVICE_USE_ONLY)` 创建，因此样例会单独创建一条 device-use-only Stream 并在其上提交长时核任务后调用 `aclrtStreamStop`。
- `aclrtStreamAbort` 不支持 `ACL_STREAM_DEVICE_USE_ONLY` Stream，因此样例会在普通 Stream 上单独演示 `aclrtStreamAbort`。
- 如果当前环境不满足 `stop/abort` 辅助演示的额外前置条件，样例会打印 `WARN` 并跳过对应辅助演示；主流程中的 `aclrtSetStreamFailureMode` 演示仍然会继续执行。
- `aclrtStreamAbort` 演示后再次调用 `aclrtSynchronizeStream` 时，可能返回 stream abort 类状态码，这属于该辅助演示的预期结果，样例会按说明记录日志而不会将整个流程判定为失败。

## 编译运行
环境安装详情以及运行详情请见example目录下的[README](../../../README.md)。


运行步骤如下：

```bash
# ${install_root} 替换为 CANN 安装根目录，默认安装在`/usr/local/Ascend`目录
source ${install_root}/cann/set_env.sh
export ASCEND_INSTALL_PATH=${install_root}/cann

# ${ascend_name} 替换为昇腾AI处理器的型号，可通过 npu-smi info 查看 Name 字段并去掉空格获得，例如 ascend910b3
export SOC_VERSION=${ascend_name}

# 部分样例中涉及调用AscendC算子，需配置AscendC编译器ascendc.cmake所在的路径，如 ${install_root}/cann/aarch64-linux/tikcpp/ascendc_kernel_cmake
# 可在CANN包安装路径下查找ascendc_kernel_cmake，例如find ./ -name ascendc_kernel_cmake，并将${cmake_path}替换为ascendc_kernel_cmake所在路径
export ASCENDC_CMAKE_DIR=${cmake_path}

# 编译运行
bash run.sh
```
## CANN RUNTIME API
在该Sample中，涉及的关键功能点及其关键接口，如下所示：
- 初始化
    - 调用aclInit接口初始化AscendCL配置。
    - 调用aclFinalize接口实现AscendCL去初始化。
- Device管理
    - 调用aclrtSetDevice接口指定用于运算的Device。
    - 调用aclrtResetDeviceForce接口强制复位当前运算的Device，回收Device上的资源。
- Context管理
    - 调用aclrtCreateContext接口创建Context。
    - 调用aclrtDestroyContext接口销毁Context。
- Stream管理
    - 调用aclrtCreateStream接口创建Stream。
    - 调用aclrtCreateStreamWithConfig接口创建`ACL_STREAM_DEVICE_USE_ONLY`类型的Stream。
    - 调用aclrtSynchronizeStream可以阻塞等待Stream上任务的完成。
    - 调用aclrtSetStreamFailureMode可以设置Stream执行任务遇到错误的操作，默认为遇错继续，可以设置为遇错即停。
    - 调用aclrtRegStreamStateCallback接口注册Stream状态回调。
    - 调用aclrtStreamStop和aclrtStreamAbort接口演示辅助Stream上的停止 / 中止行为。
    - 调用aclrtDestroyStreamForce接口强制销毁Stream，丢弃所有任务。
- 内存管理
    - 调用aclrtMalloc接口申请Device上的内存。
    - 调用aclrtFree接口释放Device上的内存。
- 数据传输
    - 调用aclrtMemcpy接口通过内存复制的方式实现数据传输。

## 已知issue

   暂无
