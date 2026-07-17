# xmole2：现代 C++ Office 文档处理库

[English](README.md) | [简体中文](README.zh-CN.md)

[![Language](https://img.shields.io/badge/language-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Build System](https://img.shields.io/badge/build-CMake-064F8C.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

xmole2 是一个使用 C++23 开发的开源 Microsoft Office 文档处理库，目标是提供文档读取、编辑、写入和转换能力。项目由相互独立的 Words、Cells、Slides 组件组成，并共享封装格式、XML、安全、诊断和 I/O 基础设施。

> [!IMPORTANT]
> xmole2 当前处于早期开发阶段，正在建立总体架构和基础 API。Office 文档 codec 与公开文档对象模型尚未达到生产可用状态。

## 设计目标

- 同时支持现代 OOXML 容器和旧版复合文档格式，并保持 OPC 与 CFB/OLE2 的语义边界。
- 分别提供 `WordDocument`、`Workbook`、`Presentation` 领域模型，同时保持一致的文档生命周期体验。
- 在安全可行的前提下，同格式往返时保留尚未支持的内容和厂商扩展。
- 通过包索引、Part 懒加载和工作表流式读取支持大型工作簿。
- 默认将 Office 文件视为不可信输入，在解析、编辑、计算、渲染和保存过程中统一执行资源限制。
- 公共 API 不暴露第三方实现类型。
- 保持 Windows、Linux 和 macOS 的跨平台可移植性。

## 项目状态

仓库目前已提供：

- 已接受的架构与软件工程规范；
- 对应规划模块边界的独立 CMake target；
- 初始 `xmole2::base` 错误、结果、资源预算、取消和操作上下文设施；
- 基础 contract test 与 CMake package export。

文档加载、领域模型、格式转换、公式计算和渲染能力仍在开发中。相关 API 实现并达到可准确描述的稳定程度后，再补充使用示例和详细功能矩阵。

## 环境要求

- 支持 C++23 且标准库提供 `std::expected` 的编译器
- CMake 3.28 或更高版本
- Ninja 或其他 CMake 支持的生成器

当前开发使用 Windows x64 与 MSVC 19.44 完成验证。Linux x64 和 macOS 支持属于架构要求，将随对应平台契约的实现逐步接入。仅支持 C++23 语言模式、但尚未提供所需 C++23 标准库设施的旧工具链不受支持。

## 依赖库

依赖元数据、固定版本、上游仓库和许可证标识统一维护在 [`deps.json`](deps.json) 中。清单描述 xmole2 各模块可以使用的依赖；早期基础 target 不一定已经链接其中所有库。所有第三方依赖均属于私有实现细节，不得出现在 xmole2 公共 API 中。

[`vcpkg.json`](vcpkg.json) 固定用于复现这些版本的 registry baseline。BqLog 当前不在所选 builtin registry baseline 中，因此只作为可选的本地集成。

| 依赖库 | 版本 | 用途 | 许可证 |
|---|---:|---|---|
| [Abseil](https://abseil.io/) | 20250814.1 | 优化容器与内部工具 | Apache-2.0 |
| [Boost.UUID](https://www.boost.org/doc/libs/1_90_0/libs/uuid/) | 1.90.0#1 | UUID 生成与表示 | BSL-1.0 |
| [BqLog](https://github.com/Tencent/BqLog) | 2.3.1 | 日志后端 | Apache-2.0 |
| [fmt](https://fmt.dev/) | 12.1.0 | 内部字符串格式化与诊断 | MIT |
| [frozen](https://github.com/serge-sans-paille/frozen) | 1.2.0 | 编译期不可变查找表 | Apache-2.0 |
| [GoogleTest](https://github.com/google/googletest) | 1.17.0#2 | 测试框架 | BSD-3-Clause |
| [magic_enum](https://github.com/Neargye/magic_enum) | 0.9.7#1 | 编译期枚举工具 | MIT |
| [minizip-ng](https://github.com/zlib-ng/minizip-ng) | 4.0.10#1 | ZIP 容器后端 | BSD-3-Clause |
| [OpenSSL](https://www.openssl.org/) | 3.6.0#3 | 密码学基础能力 | Apache-2.0 |
| [pugixml](https://pugixml.org/) | 1.15#1 | XML 解析后端 | MIT |
| [RE2](https://github.com/google/re2) | 2025-11-05 | 正则表达式处理 | BSD-3-Clause |
| [simdutf](https://simdutf.github.io/simdutf/) | 8.0.0 | 面向 XML 输入的 SIMD Unicode 校验与转码 | Apache-2.0 |

部分版本中的 `#N` 是 vcpkg port-version 后缀。增加、删除或升级依赖时，应先更新 `deps.json`，并在修改构建集成前复核上游许可证。

## 构建

将 `CMakeUserPresets.json.example` 复制为已被 Git 忽略的 `CMakeUserPresets.json`，按本机环境修改路径，然后列出可用 preset：

```console
cmake --list-presets all -S .
```

选择任一 configure preset 后运行：

```console
cmake --preset <preset_name>
cmake --build --preset build-<preset_name>
ctest --test-dir "out/build/<preset_name>" --output-on-failure
```

> **注意：** CMakeUserPresets.json 中可使用 `//` 或 `/* */` 注释（JSON5 风格）作为指引。若工具链或编辑器不支持注释，请在使用前全部删除。

手动配置时，必须确保 CMake 选择的编译器及其标准库提供 `std::expected`：

```console
cmake -S . -B out/build/default -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXMOLE2_BUILD_TESTS=ON
cmake --build out/build/default
ctest --test-dir out/build/default --output-on-failure
cmake --install out/build/default --prefix out/install/default
```

## 文档

- [规范索引](docs/spec/README.md)
- [总体架构](docs/spec/architecture.md)
- [公共 API 与生命周期](docs/spec/public-api.md)
- [保真与转换](docs/spec/fidelity.md)
- [安全与资源限制](docs/spec/security.md)
- [依赖库与源码查阅规范](docs/spec/dependencies.md)
- [C++ 代码规范](docs/spec/code-style.md)
- [测试与质量要求](docs/spec/testing.md)
- [本地 fixture 清单](docs/fixtures/catalog.md)
- [本地参考快照](docs/reference-snapshots.md)
- [架构决策记录](docs/adr/)

完整架构讨论保存在[设计会议记录](docs/meetings/2026-07-15-office-architecture-all-sessions.md)中。修改架构边界或公共契约必须先提交 ADR，并同步更新规范和测试。

## 参与贡献

欢迎在项目成型阶段参与贡献，具体流程见 [CONTRIBUTING.md](CONTRIBUTING.md)。修改模块边界或公共契约前，请先阅读上述规范并通过 ADR 记录决策。C++ 修改必须遵循仓库代码风格，并使用仓库中的 `.clang-format` 配置完成格式化。

## 许可证

xmole2 使用 [Apache License 2.0](LICENSE) 开源许可证。
