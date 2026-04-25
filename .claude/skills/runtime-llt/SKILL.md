---
name: runtime-llt
description: 对本次代码修改执行精准 LLT（Low Level Test）验收。筛选相关测试用例并定向运行，确保覆盖率达到 95% 且全部通过。覆盖率不足时新增用例，运行失败时定位是代码 bug 还是用例问题并修复。当用户请求运行 LLT、运行 /runtime-llt、或需要验证代码修改的测试覆盖时触发。
---

# Runtime LLT 精准验收

LLT 全量运行不稳定（易超时/coredump），采用精准用例筛选 + 定向运行策略，避免全量执行。

## 执行流程

### 1. 筛选相关测试用例

从 UT 代码中搜索与本次修改涉及的接口/函数名相关的测试用例：

```bash
rg -n "(<目标接口名>|<目标函数名>)" tests/ut/runtime/runtime/test/ --include="*.cc"
```

从搜索结果中提取 `TEST` / `TEST_F` 用例名，汇总为 gtest_filter 表达式：

```
TestSuiteA.TestCase1:TestSuiteA.TestCase2:TestSuiteB.*
```

### 2. 运行相关用例

```bash
export ASAN_OPTIONS=handle_segv=0:detect_leaks=0
timeout 600s bash tests/build_ut.sh --ut runtime --target runtime_ut -- --gtest_filter='<筛选出的用例filter>'
```

> **超时说明**：首次编译时间较长（可能需要数分钟），超时设为 600s。若确认非首次编译，可缩短至 120s。

### 3. 异常处理

若出现超时或 coredump，单独运行失败用例确认是否为偶发问题：

```bash
timeout 120s bash tests/build_ut.sh --ut runtime --target runtime_ut -- --gtest_filter='<FailedTestCase>'
```

- **单独运行通过** → 判定为偶发问题，LLT 验收通过
- **单独运行仍失败** → 排查是否为本次修改引入的问题：
  - 若为代码 bug → 修复代码后重新运行
  - 若为用例未适配 → 修改用例以匹配新接口，重新运行

### 4. 覆盖率检查

确认本次修改代码的 LLT 覆盖率达到 95%：

- 检查修改的函数/分支是否都有对应的测试用例覆盖
- 覆盖率不足时，新增测试用例补充覆盖
- 新增用例遵循仓库现有 UT 风格（TEST_F 宏、EXPECT_EQ 断言等）

### 5. 迭代直到通过

重复步骤 2-4，直到所有相关用例运行通过且覆盖率达标。