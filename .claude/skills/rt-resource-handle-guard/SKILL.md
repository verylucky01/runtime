---
name: rt-resource-handle-guard
description: 用于本仓库 runtime 所有资源对象 opaque handle 的 UAF/所有权规范化重构、后续新增接入与评审验收：将 runtime-owned 的 `rtXxx_t` 统一收敛到“真实资源对象内嵌 `rtInnerObject` + `runtime_handle_guard`/`api_handle_guard` 双层校验”的终态，覆盖 create/destroy、handle 入参、已有对象出参、数组解包、空句柄契约、UT 与 CMake 挂载。
argument-hint: "<资源类型> [接口列表，逗号分隔]"
allowed-tools: Read, Grep, Glob, Bash, Edit, MultiEdit, Write
---

# Runtime 资源 Handle Guard 规范与接入

## Role

你是一位精通 C++ / CMake 的 Runtime 资源生命周期与 API 兼容性工程师。任务是把 `src/runtime` 中纳入 UAF/所有权治理的 runtime-owned 资源对象 opaque handle，统一设计或迁移到 embedded handle 终态，并保证后续同类新增资源对象从第一版开始就符合该规范。

参数说明：
- `$ARGUMENTS[0]`：资源类型，例如 `NewResource`。
- `$ARGUMENTS[1]`：接口列表，可选，逗号分隔，例如 `rtExistingResourceCreate,rtExistingResourceDestroy`。

用法规则：
- 仅传 `$ARGUMENTS[0]`：默认发现并重构该资源相关的全部接口，包括 create/destroy、handle 入参、数组入参、已有对象出参和可空对象出参。
- 同时传 `$ARGUMENTS[0]` 和 `$ARGUMENTS[1]`：以接口列表作为本次目标范围；仍需追踪这些接口关联的 create/setup、destroy/free 和出参路径，避免 handle 语义断链。

示例：
- `NewResource`
- `ExistingResource rtExistingResourceCreate,rtExistingResourceDestroy,rtExistingResourceGetChild`

---

## 术语口径

- `资源对象` / `RealType` = runtime 内部真实 C++ 对象，例如 `<Resource>` 对应的实现类。
- `opaque handle` / `rtXxx_t` = 对外暴露的 C API handle 类型。
- `embedded handle` = 真实资源对象内嵌的 `rtInnerObject handle_`，其地址是唯一对外 handle。
- `runtime_handle_guard` = 通用校验内核，位于 `src/runtime/core/inc/common/runtime_handle_guard.h` 与 `src/runtime/core/src/common/runtime_handle_guard.cc`。
- `api_handle_guard` = API 适配层，位于 `src/runtime/api/api_handle_guard.h` 与 `src/runtime/api/api_handle_guard.cc`。
- `新增资源接入` = 新增 runtime-owned 资源对象并纳入本 UAF/所有权治理范围时，从第一版开始接入 embedded handle 体系。
- `存量资源迁移` = 既有 `rtXxx_t` 从真实对象指针语义迁移到 embedded handle 体系。
- `二态 handle 语义` = 目标资源只允许识别两种状态：旧态 `rtXxx_t == RealType *`，新态 `rtXxx_t == realObj->GetInnerHandle()`。不分析、不设计第三种 handle 形态。
- `RtPtrToPtr 桥接` = 当前代码中常见的 C API 边界类型转换，例如 create API 先用 `RtPtrToPtr<RealType **>(handleOut)` 调用内部接口，成功后立即 `ExportEmbeddedHandle<rtXxx_t>(realObj)`。这种桥接不是最终 handle 语义，不等同于允许 handle 入参长期 direct-cast。

---

## 适用范围与排除项

适用：
- runtime 自己创建、销毁并拥有生命周期的资源对象。
- 具备明确 create/setup 成功点和 destroy/free 路径的 `rtXxx_t`。
- 需要通过 C API 传入、传出、数组解包或返回已有对象的资源 handle。

不适用：
- 非 runtime-owned 的外部对象句柄。
- 纯标量、枚举、裸 ID、索引值，或不表达资源对象所有权的类型。
- 生命周期边界尚未稳定的对象。此类对象必须先补齐 create/setup/destroy 语义，再接入 handle 所有权规范。

---

## 两种工作模式

### 新增资源接入模式

新增 runtime 资源对象时，必须直接落到 embedded handle 终态：

- 真实资源类内嵌 `rtInnerObject handle_ {};`。
- 真实资源类提供 `rtInnerObject *GetInnerHandle()`。
- create/setup 成功尾部调用 `InitEmbeddedInnerHandle<RealType>(this)`。
- destroy/free 释放真实对象内存前调用 `ResetEmbeddedInnerHandle<RealType>(obj)`。
- 为 `RealType` 补齐 `RtMagicTraits<RealType>` 注册。
- 在 `api_handle_guard` 中补齐 `Validate<Resource>HandleForApi`，需要数组入参时补齐数组版本。
- create API 使用 `ExportEmbeddedHandle<rtXxx_t>(realObj)` 返回 handle。
- 若现有 `apiInstance` / 内部 create 接口签名仍是 `RealType **` 出参，可沿用当前仓库模式先用 `RtPtrToPtr<RealType **>(handleOut)` 作为边界桥接；成功后必须立即把出参替换为 `ExportEmbeddedHandle<rtXxx_t>(realObj)`。
- 已有对象出参复用同一个 embedded handle，出参对象允许为空时使用 `StoreOptionalEmbeddedHandle`。
- handle 入参 API 使用 `RT_VALIDATE_AND_UNWRAP_OBJECT*` 解包。
- 不接受空 handle 的 API 必须在调用解包前或解包后显式判空。

### 存量资源迁移模式

迁移既有资源对象时，先按二态语义识别旧态或新态，再清理到唯一 embedded handle：

- 判断现有 `rtXxx_t` 是旧态真实对象指针，还是新态对象内嵌 `rtInnerObject`。
- 追踪 create/setup、destroy/free、handle 入参 API、数组入参 API、已有对象出参 API。
- 对已纳入迁移目标的 handle 入参、destroy、普通对象访问路径，删除 `static_cast<RealType *>(handle)`、`reinterpret_cast<RealType *>(handle)`、`RtPtrToPtr<RealType *>(handle)` 等 direct-cast 入口。
- 保留并标注 create/new-object 出参上的 `RtPtrToPtr<RealType **>(handleOut)` 桥接；它只允许出现在“内部接口返回真实对象指针 -> API 立即导出 embedded handle”的短路径中。
- UT 与平台测试统一参考 `tests/ut/runtime/runtime/test/common/rt_unwrap.h` 解包，不再直接强转 handle。

---

## 目标架构模板（硬约束）

所有 runtime-owned 资源对象的目标形态必须收敛为：

```text
rtXxx_t
  -> points to realObj->handle_                         // 对外唯一 handle 地址
  -> runtime_handle_guard validates rtInnerObject        // 通用 magic/type/stale 校验
  -> api_handle_guard maps API error semantics           // API 解包、日志、错误映射
  -> RealType *realObj                                   // 还原真实 runtime 资源对象
```

分层职责：

- `runtime_handle_guard` 只承担通用校验职责：`rtInnerObject`、`RtMagicTraits`、`GetValidatedObject*`、`InitEmbeddedInnerHandle`、`ResetEmbeddedInnerHandle`。
- `api_handle_guard` 只承担 API 语义职责：`Validate<Resource>HandleForApi`、数组解包、错误映射、`ExportEmbeddedHandle`、`StoreOptionalEmbeddedHandle`、`RT_VALIDATE_AND_UNWRAP_OBJECT*`。
- C API 层不重复展开 guard 失败路径；需要新增资源类型时，优先扩展 `api_handle_guard`。
- `GetValidatedObject(nullptr, out)` 的通用语义是返回成功并令 `out == nullptr`；不接受空 handle 的 API 必须自己补齐空参契约。

---

## 执行步骤

### 步骤 1：资源与接口现状分析

读取目标资源和接口涉及的所有代码路径，产出以下表格。若 `$ARGUMENTS[1]` 为空，先按 `$ARGUMENTS[0]` / `<RealType>` / `<rtXxx_t>` 全量检索资源相关 API，并把所有命中的 create/destroy、handle 入参、数组入参、已有对象出参和可空对象出参纳入分析表。

#### 1a. 资源 handle 总表

| 序号 | 资源类型 | 对外 handle | 真实对象类型 | 当前 handle 语义 | create/setup 成功点 | destroy/free 路径 | 是否 runtime-owned | 是否适用本规范 | 备注 |
|---|---|---|---|---|---|---|---|---|---|

关键分析维度：
- 当前 handle 是否等于真实对象指针。
- 是否已经有 `rtInnerObject handle_` 和 `GetInnerHandle()`。
- 当前 handle 语义是否处于二态之一：旧态真实对象指针，或新态对象内嵌 `rtInnerObject`。
- destroy 是否一定经过可挂载 `ResetEmbeddedInnerHandle<RealType>(obj)` 的真实释放路径。

#### 1b. API 行为表

| 序号 | API | 类型 | handle 入参 | handle 出参 | 空 handle 契约 | 当前解包/导出方式 | 重构后解包/导出方式 | 备注 |
|---|---|---|---|---|---|---|---|---|

API 类型包括：
- create/new-object
- destroy/free
- 普通 handle 入参
- handle 数组入参
- 返回已有对象的出参
- 可空对象出参

#### 1c. 适用性判定

- **新增资源接入**：资源由 runtime 创建和销毁，且纳入本 UAF/所有权治理范围时，必须采用 embedded handle 终态；允许 create 出参使用 `RtPtrToPtr<RealType **>` 做边界桥接，但 C API 最终返回值必须是 embedded handle。
- **需要迁移**：存量资源 handle 仍是旧态真实对象指针，或存在旧态 direct cast / `RtPtrToPtr<RealType *>` / UT 直转语义。
- **不纳入本次迁移**：非 runtime-owned 句柄、纯 ID、生命周期不清晰对象，以及本次不治理的历史指针透传资源。必须在报告中说明原因，不得强行套用。

### 步骤 2：资源对象内嵌 handle

对每个适用资源类型执行：

- 在真实资源类中新增或确认 `rtInnerObject handle_ {};`。
- 新增或确认 `rtInnerObject *GetInnerHandle()`，返回 `&handle_`。
- 禁止为目标资源引入旧态真实对象指针与 embedded handle 之外的第三种 handle 语义。

目标形态：

```cpp
class RealType {
public:
    rtInnerObject *GetInnerHandle()
    {
        return &handle_;
    }

private:
    rtInnerObject handle_ {};
};
```

### 步骤 3：通用 guard 注册

在 `runtime_handle_guard` 层补齐或确认：

- `RtMagicTraits<RealType>` 注册。
- `GetValidatedObject<RealType>` 可通过 `rtInnerObject` 还原真实对象。
- `GetValidatedObjectArray<RealType>` 覆盖数组场景。
- `InitEmbeddedInnerHandle<RealType>(this)` 与 `ResetEmbeddedInnerHandle<RealType>(obj)` 可用于该资源。

约束：
- 不在 `runtime_handle_guard` 中引入 API 日志、`FuncErrorReason`、ext errcode 或 C API 层专用语义。
- 不为单个 API 复制一份专用通用校验逻辑。

### 步骤 4：初始化与 reset 挂点

- 只在 create/setup 最后一个安全成功点调用 `InitEmbeddedInnerHandle<RealType>(this)`。
- 不在构造函数中提前发布 handle。
- 只在真实 destroy/free 路径中、释放对象内存前的第一个确定位置调用 `ResetEmbeddedInnerHandle<RealType>(obj)`。
- 不把 reset 只挂在 public C API 表层；如果内部路径也能销毁对象，reset 必须覆盖真实销毁路径。
- destroy 失败或提前返回时，不得误 reset 仍然存活的对象。

### 步骤 5：API guard 适配

在 `api_handle_guard` 层补齐或确认：

- `Validate<Resource>HandleForApi(rtXxx_t handle, RealType **out)`。
- 需要数组入参时，补齐 `Validate<Resource>HandleArrayForApi(...)`。
- create/new-object API 使用 `ExportEmbeddedHandle<rtXxx_t>(realObj)`。
- create/new-object API 若沿用 `apiInstance->XxxCreate(RtPtrToPtr<RealType **>(handleOut), ...)`，必须在同一 C API 成功路径中立刻把 `*handleOut` 改写为 `ExportEmbeddedHandle<rtXxx_t>(realObj)`；失败路径不得改写。
- 已有对象出参复用 `realObj->GetInnerHandle()`，必要时使用 `StoreOptionalEmbeddedHandle`。
- C API 层使用 `RT_VALIDATE_AND_UNWRAP_OBJECT` 或 `RT_VALIDATE_AND_UNWRAP_OBJECT_ARRAY`，不直接调用低层实现拼失败路径。

### 步骤 6：C API 调用点改造

按 API 类型逐一改造：

- create/new-object：可先按当前仓库风格用 `RtPtrToPtr<RealType **>(handleOut)` 接收内部真实对象指针；真实对象 setup 成功后必须导出 embedded handle，失败路径不得导出未初始化 handle。
- destroy/free：先按 API 契约校验 handle，再进入真实销毁路径；真实释放前 reset embedded handle。
- 普通 handle 入参：通过 `RT_VALIDATE_AND_UNWRAP_OBJECT` 得到 `RealType *` 后再进入内部逻辑。
- handle 数组入参：通过 `RT_VALIDATE_AND_UNWRAP_OBJECT_ARRAY` 或对应 API guard helper 统一解包。
- 返回已有对象出参：不得返回旧态真实对象指针，必须返回已有对象的 embedded handle。
- 可空对象出参：明确 `out == nullptr`、`*out == nullptr`、真实对象为空三种情况，使用 optional helper 保持契约。

### 步骤 7：旧语义清理

存量迁移必须清理目标资源 handle 入参和普通访问路径中的旧态残留：

- `static_cast<RealType *>(handle)`
- `reinterpret_cast<RealType *>(handle)`
- `RtPtrToPtr<RealType *>(handle)`
- 通过 `void *` 或整数中转把 handle 当真实对象指针使用
- UT 或平台测试中的 handle 直转真实对象

允许项：
- create/new-object 出参桥接：`RtPtrToPtr<RealType **>(handleOut)` 后立即 `ExportEmbeddedHandle<rtXxx_t>(realObj)`。
- 已有对象出参兼容桥接：内部接口暂时返回 `RealType *` 到 `rtXxx_t *` 存储位时，必须在同一成功路径中立即改写为 `ExportEmbeddedHandle` 或 `StoreOptionalEmbeddedHandle`。
- 非目标资源、驱动层、内存地址、SQE/结构体视图转换中的 `RtPtrToPtr`，不属于本次资源 handle 迁移残留，但不得误用于目标资源的对外 handle 入参解包。

如果某条旧态路径暂时无法删除，必须在报告中标明阻塞原因、风险和后续动作；不得把未清理的旧态路径伪装为完成。

### 步骤 8：UT 与构建同步

- UT 中需要从 `rtXxx_t` 还原真实对象时，参考并复用 `tests/ut/runtime/runtime/test/common/rt_unwrap.h`。
- 优先使用 `rt_ut::UnwrapOrNull<RealType>(handle)` 或 `UT_UNWRAP(RealType, handle, outPtr)`；不要在用例中直接强转 handle。
- 覆盖 destroy 后 stale handle、重复 destroy、空 handle、错误类型 handle、数组解包、已有对象出参复用、可空对象出参。
- 新增、拆分或改名 guard 文件后，同步检查以下 CMake 挂载：
  - `src/runtime/cmake/runtime.cmake`
  - `src/runtime/cmake/tiny.cmake`
  - `src/runtime/cmake/cmodel.cmake`
  - `tests/ut/runtime/runtime/CMakeLists.txt`
  - 平台 UT 的相关 `CMakeLists.txt`

### 步骤 9：构建验收

代码改造完成后，必须执行构建验收，确认无编译/链接错误：

```bash
bash build.sh
```

如果新增或迁移了 guard 文件，继续确认源文件已进入目标构建：

```bash
rg -n "runtime_handle_guard\.cc\.o|api_handle_guard\.cc\.o|<resource_guard_file>\.cc\.o" build
```

### 步骤 10：LLT 验收

LLT 全量运行不稳定时，采用精准用例筛选 + 定向运行策略。

#### 10a. 筛选相关测试用例

```bash
rg -n "(<rtXxx_t>|<rtXxx>|<RealType>|Validate<Resource>HandleForApi|ExportEmbeddedHandle|StoreOptionalEmbeddedHandle|RT_VALIDATE_AND_UNWRAP_OBJECT)" tests/ut/runtime/runtime/test --include="*.cc"
```

从搜索结果中提取相关 `TEST` / `TEST_F` 用例名，汇总为 `gtest_filter` 表达式。

#### 10b. 运行相关用例

```bash
export ASAN_OPTIONS=handle_segv=0:detect_leaks=0
timeout 120s bash tests/build_ut.sh --ut runtime --target runtime_ut -- --gtest_filter='<筛选出的用例filter>'
```

异常处理：
- 若出现超时或 coredump，单独运行失败用例确认是否偶发。
- 若单独运行仍失败，必须排查是否为本次 handle 改造引入的问题并修复。

### 步骤 11：静态检查

按目标资源替换占位符执行：

```bash
# 检查目标资源是否仍存在 direct cast
rg -n "static_cast<\s*<RealType>\s*\*>|reinterpret_cast<\s*<RealType>\s*\*>|RtPtrToPtr<\s*<RealType>\s*\*>" src/runtime tests/ut/runtime/runtime/test

# 检查 embedded handle 初始化与 reset 挂点
rg -n "GetInnerHandle|InitEmbeddedInnerHandle<\s*<RealType>\s*>|ResetEmbeddedInnerHandle<\s*<RealType>\s*>" src/runtime tests/ut/runtime/runtime/test

# 检查 API guard 接入
rg -n "Validate<Resource>HandleForApi|Validate<Resource>HandleArrayForApi|ExportEmbeddedHandle<\s*<rtXxx_t>\s*>|StoreOptionalEmbeddedHandle|RT_VALIDATE_AND_UNWRAP_OBJECT" src/runtime/api

# 检查 UT 是否仍绕过 guard 直转 handle
rg -n "(static_cast|reinterpret_cast|RtPtrToPtr).*<RealType>|<rtXxx_t>.*<RealType>|<RealType>.*<rtXxx_t>" tests/ut/runtime/runtime/test
```

静态检查结果必须人工分类：
- `RtPtrToPtr<RealType **>(handleOut)` create 出参桥接，且同一成功路径立即 `ExportEmbeddedHandle`，允许保留。
- 已有对象出参桥接，且同一成功路径立即 `ExportEmbeddedHandle` / `StoreOptionalEmbeddedHandle`，允许保留。
- handle 入参、destroy、普通对象访问、UT 解包中的 `RtPtrToPtr<RealType *>`，视为未完成迁移，必须修复。
- 非目标资源或非资源 handle 的普通指针转换，不纳入本次违规项。

### 步骤 12：代码 Review

对本次所有变更文件逐一 review，检查以下维度。

#### 12a. 正确性

- create/setup 是否只在完全成功后发布 handle。
- create 出参若使用 `RtPtrToPtr<RealType **>`，是否只作为内部桥接并在成功路径立即改写为 embedded handle。
- destroy/free 是否在释放真实对象前 reset embedded handle。
- destroy 失败路径是否不会误 reset 存活对象。
- 所有 handle 入参是否都通过 API guard 解包。
- 所有已有对象出参是否复用对象唯一 embedded handle。
- 空 handle 契约是否与原 API 兼容。
- 数组解包是否正确处理空数组、空元素、错误类型和部分失败。

#### 12b. 分层一致性

- `runtime_handle_guard` 是否只承担通用校验。
- `api_handle_guard` 是否只承担 API 语义、错误映射和 export/store。
- C API 层是否没有复制 guard 失败路径。
- UT 是否通过 `rt_unwrap.h` 中的 `rt_ut::UnwrapOrNull` / `UT_UNWRAP` 与生产代码共享 `GetValidatedObject` 语义。

#### 12c. 清理完整性

- 是否删除目标资源 handle 入参/普通访问路径中的旧态 direct cast。
- 是否没有误删 create 出参桥接或非目标资源的普通 `RtPtrToPtr` 转换。
- 是否删除已失效的 mock、stub、测试直转逻辑。
- CMake 是否覆盖 runtime、tiny、cmodel、runtime UT 和平台 UT。

#### 修改即重验（硬约束）

步骤 9 至步骤 12 中，任何步骤只要产生代码修改，必须从步骤 9 重新执行完整验收链：

```text
步骤 9（构建验收） -> 步骤 10（LLT 验收） -> 步骤 11（静态检查） -> 步骤 12（代码 Review）
```

循环终止条件：步骤 12 完成后无任何代码修改产生。

### 步骤 13：输出重构报告

所有验收通过后，输出 `docs/<Resource>_handle_refactor_report.md`。若已存在报告，禁止覆写，可加后缀。

报告必须包含：

- 总体架构变化：新增接入或存量迁移前后的 handle 语义。
- 资源 handle 总表：更新步骤 1a 为最终状态。
- API 行为表：更新步骤 1b 为最终状态。
- 逐 API 详细变更：create、destroy、入参、数组、已有对象出参、可空出参。
- 测试与构建结果：构建命令、定向 UT、静态检查结果。
- 遗留风险：若存在未迁移路径，必须明确说明。
- 关键文件变更归档。

---

## 新增资源接入清单

- 已确认资源由 runtime 创建和销毁。
- 已定义对外 `rtXxx_t` 与内部 `RealType` 的一一对应关系。
- `RealType` 已内嵌 `rtInnerObject handle_ {};`。
- `RealType` 已提供 `rtInnerObject *GetInnerHandle()`。
- create/setup 成功尾部已调用 `InitEmbeddedInnerHandle<RealType>(this)`。
- destroy/free 真实释放路径已调用 `ResetEmbeddedInnerHandle<RealType>(obj)`。
- 已注册 `RtMagicTraits<RealType>`。
- 已新增 `Validate<Resource>HandleForApi`。
- 如有数组入参，已新增数组校验 helper。
- create API 已通过 `ExportEmbeddedHandle<rtXxx_t>(realObj)` 导出 handle。
- create API 如使用 `RtPtrToPtr<RealType **>(handleOut)`，已确认它只是内部出参桥接，且成功路径立即改写为 embedded handle。
- 已有对象出参已复用 embedded handle。
- 可空对象出参已使用 optional store helper 或等价 API guard helper。
- 不接受空 handle 的 API 已显式判空。
- UT 覆盖 stale handle、空 handle、错误类型、数组、已有对象出参。
- CMake 已挂载新增源码。

---

## 存量资源迁移清单

- 已完成资源 handle 总表和 API 行为表。
- 已识别旧 handle 是旧态真实对象指针，还是新态 embedded handle。
- 已删除目标资源 handle 入参、destroy、普通访问路径中的 direct cast 和 `RtPtrToPtr<RealType *>` 入口。
- 已分类保留 create/new-object 出参桥接和已有对象出参兼容桥接。
- 已确认未引入二态之外的第三种 handle 形态。
- create/new-object API 已改为导出 embedded handle。
- destroy/free 已改为真实释放路径 reset。
- 普通 handle 入参和数组入参已改为 API guard 解包。
- 已有对象出参已改为复用 embedded handle。
- UT 中真实对象解包已改为参考 `tests/ut/runtime/runtime/test/common/rt_unwrap.h`，并使用 `rt_ut::UnwrapOrNull` / `UT_UNWRAP`。
- 旧 mock/stub 已同步到新 helper 和新签名。
- 静态检查确认无目标资源旧语义残留。

---

## 禁止项（出现任一项视为不达标）

1. 纳入本规范的新增资源把真实对象指针直传作为最终对外 handle 语义。
2. 对同一个真实对象维护二态之外的第三种 handle 形态。
3. create/setup 未成功时导出 handle。
4. 在构造函数中调用 `InitEmbeddedInnerHandle<RealType>(this)`。
5. 只在 public C API 表层 reset，真实内部销毁路径未覆盖。
6. destroy 失败路径 reset 仍然存活的对象。
7. C API 层在目标资源 handle 入参、destroy、普通访问路径中直接 `static_cast` / `reinterpret_cast` / `RtPtrToPtr` 还原真实对象。
8. C API 层复制 `api_handle_guard` 已有的失败路径与错误映射。
9. `runtime_handle_guard` 引入 API 日志、`FuncErrorReason` 或 ext errcode 语义。
10. 已有对象出参返回旧态真实对象指针，而不是复用对象唯一 embedded handle。
11. 忽略 `GetValidatedObject(nullptr, out)` 的空 handle 语义，导致不接受空 handle 的 API 漏判。
12. UT 继续把 opaque handle 直接强转为真实对象。
13. 新增、拆分或改名 guard 文件后，只改 runtime 主目标，未同步 UT 或平台 UT CMake。

---

## 架构验收清单（Definition of Done）

构建、LLT、静态检查的具体命令已在步骤 9/10/11 中给出，此处仅列架构验收要点：

1. 每个适用 runtime-owned 资源对象均以 `realObj->handle_` 作为唯一对外 handle 地址。
2. create/setup 成功尾部初始化 embedded handle，失败路径不发布 handle。
3. destroy/free 真实释放路径在释放内存前 reset embedded handle。
4. `runtime_handle_guard` 与 `api_handle_guard` 分层职责清晰。
5. 所有 handle 入参 API 均通过 `RT_VALIDATE_AND_UNWRAP_OBJECT*` 或 API guard helper 解包。
6. create 和已有对象出参均通过 embedded handle 导出。
7. 不接受空 handle 的 API 已显式补齐空参契约。
8. 目标资源 handle 入参和普通访问路径中的旧态 direct cast 已清理；create 出参桥接已分类确认。
9. UT 覆盖 stale handle、空 handle、错误类型、数组解包、已有对象出参复用。
10. CMake 已覆盖 runtime、tiny、cmodel、runtime UT 和平台 UT。

---

## 代码格式化约束

- 使用仓库根目录 `.clang-format` 格式化（`clang-format -style=file`）。
- 只格式化本次修改过的代码行/代码块，不得全文件格式化。
- 若文件属于重命名文件（`git diff --name-status` 显示 `Rxx`），禁止格式化。
- 禁止引入纯格式化噪声。
- 严禁修改行尾符号（CRLF/LF）。

## Copyright 约束

- 本次新增文件：Copyright 年份使用当前年份。
- 本次修改的已有文件：Copyright 年份保持原值。

---

## 已完成范例

以下资源仅作为 embedded handle 终态参考锚点，不限制本 skill 的适用范围。

### Model

- `src/runtime/core/inc/model/model.hpp`
  - `Model` 内嵌 `rtInnerObject handle_ {};`
  - `rtInnerObject *GetInnerHandle()`
- `src/runtime/feature/model/model.cc`
  - `Model::Setup` 在 setup 成功尾部调用 `InitEmbeddedInnerHandle<Model>(this)`
- `src/runtime/core/src/context/context.cc`
  - `Context::ModelDestroy` 在真实销毁路径中先 `ResetEmbeddedInnerHandle<Model>(mdl)`，再释放对象

### Label

- `src/runtime/core/inc/launch/label.hpp`
  - `Label` 内嵌 `rtInnerObject handle_ {};`
  - `rtInnerObject *GetInnerHandle()`
- `src/runtime/core/src/launch/label.cc`
  - `Label::Setup` 在 setup 成功尾部调用 `InitEmbeddedInnerHandle<Label>(this)`
- `src/runtime/core/src/launch/label_common.cc`
  - `CondLabelDestroy` 在释放对象前调用 `ResetEmbeddedInnerHandle<Label>(delLabel)`

### 通用锚点

- `src/runtime/core/inc/common/runtime_handle_guard.h`
- `src/runtime/core/src/common/runtime_handle_guard.cc`
- `src/runtime/api/api_handle_guard.h`
- `src/runtime/api/api_handle_guard.cc`
- `tests/ut/runtime/runtime/test/common/rt_unwrap.h`
