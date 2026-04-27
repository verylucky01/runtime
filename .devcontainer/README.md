# Dev Container 使用说明

本目录包含 VS Code Dev Container 配置，用于在容器内编译 runtime 仓及执行 UT。

## 环境规格

| 工具 | 版本 |
|----|------|
| 基础镜像 | Ubuntu 22.04 LTS |
| GCC / G++ | 9.x |
| CMake | 3.22+ |
| Python | 3.10+ |
| ccache | 系统包 |
| lcov | 系统包（用于覆盖率） |
| libasan5 / libtsan0 / libubsan1 | gcc-9 消毒器运行时（`--asan` 时使用） |

## 快速开始

### 1. 启动容器

使用 VS Code 打开本仓目录，按 `F1` → **Dev Containers: Reopen in Container**，等待镜像构建完成。

容器启动时会自动尝试通过网络下载第三方依赖（保存在 `./third_party/`，并软链到 `output/third_party` 以匹配 `build.sh`/`build_ut.sh` 默认查找路径）。

### 2. 编译 runtime 整包

```bash
# 容器启动时已自动下载第三方包到 third_party/ 并软链到 output/third_party。
# 因此可直接执行（无需传 --cann_3rd_lib_path）：
export CMAKE_TLS_VERIFY=0
bash build.sh

# 如果第三方依赖在其他路径，必须使用绝对路径
# （CMake 的 INTERFACE_INCLUDE_DIRECTORIES 不接受相对路径）：
bash build.sh --cann_3rd_lib_path=$(pwd)/third_party
```

> **注意**：完整的 runtime 包编译及运行需要 CANN toolkit 和 NPU 驱动。  
> 请按照 README.md 中"环境准备"章节，在容器内手动下载并安装对应版本的 `Ascend-cann-toolkit` 包：
> ```bash
> chmod +x Ascend-cann-toolkit_${cann_version}_linux-x86_64.run
> ./Ascend-cann-toolkit_${cann_version}_linux-x86_64.run --full --force --quiet
> source /usr/local/Ascend/cann/set_env.sh
> ```

### 3. 执行 UT

UT 使用 mock/stub 实现，**无需 NPU 硬件**即可执行。

> **内存提示**：`build_ut.sh` 默认 `-j8`，部分 ascendcl 源文件 cc1plus 单实例内存占用接近 1GB，
> 若 Docker Desktop / 容器内存 ≤ 8GB，建议显式降并发以避免 cc1plus 被 OOM-Kill：
> ```bash
> bash tests/build_ut.sh --ut=acl --target=ascendcl_utest -j2
> ```

```bash
# 编译并运行 acl 模块所有用例（默认会从 output/third_party 找第三方包，已被 postCreateCommand 软链准备好）
bash tests/build_ut.sh --ut=acl --target=ascendcl_utest -j2

# 带覆盖率
bash tests/build_ut.sh --ut=acl --target=ascendcl_utest -c -j2

# 指定第三方依赖路径（必须使用绝对路径，否则 CMake 会拒绝相对路径）
bash tests/build_ut.sh --ut=acl --target=ascendcl_utest -j2 --cann_3rd_lib_path=$(pwd)/third_party

# 清理编译产物（注意 output/third_party 是软链，删 output/ 会顺带删掉，重启容器或重建软链即可恢复）
rm -rf build/
rm -rf output/ && ln -sfn $(pwd)/third_party output/third_party
```

所有可用的 UT 模块（通过 `--ut` 指定）：

| 模块名 | 路径 |
|--------|------|
| full | tests/ut（全量） |
| acl | tests/ut/acl |
| runtime | tests/ut/runtime/runtime |
| runtime_c | tests/ut/runtime/runtime_c/testcase |
| platform | tests/ut/platform/ut |
| qs / queue_schedule | tests/ut/queue_schedule |
| aicpusd / aicpu_sched | tests/ut/aicpu_sched |
| slog | tests/ut/slog |
| atrace | tests/ut/atrace |
| msprof | tests/ut/msprof |
| adump | tests/ut/adump |
| tsd | tests/ut/tsd |
| error_manager | tests/ut/error_manager |
| mmpa | tests/ut/mmpa |

更多参数请运行：
```bash
bash tests/build_ut.sh -h
bash build.sh -h
```