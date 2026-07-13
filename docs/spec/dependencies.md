# 第三方依赖与源码研究规范

状态：Normative Baseline  
适用范围：所有 xmole2 target、测试、工具、代码生成器和集成模块

## 1. 信息来源与基本原则

[`deps.json`](../../deps.json) 是依赖名称、固定版本、上游仓库、主页和许可证的机器可读清单。本文定义每个依赖在 xmole2 中的职责、允许依赖的模块，以及开发者和大模型查找真实接口与源码的流程。

必须遵守以下规则：

- 第三方依赖只能作为实现 target 的 `PRIVATE` 依赖，除非独立 ADR 明确批准其他范围。
- public header、公开成员和函数签名不得出现第三方类型。
- 使用标准库能够清晰、正确且满足性能要求时，优先使用标准库；替换为第三方设施必须有明确收益或统一工程要求。
- CMake 必须链接包导出的 imported target，禁止在源码或公共构建脚本中硬编码本机 include/lib 文件路径。
- 版本、许可证或上游仓库变更必须先更新 `deps.json`，再更新本文、构建集成和测试。
- 不得根据记忆猜测接口、宏、target 名称或所有权契约；必须检查当前固定版本的实际头文件、CMake config、官方文档或源码。

## 2. 依赖清单与职责边界

| 依赖 | 固定版本 | xmole2 中的职责 | 允许的主要模块 | 推荐 CMake target | 当前 static-md 状态 |
|---|---:|---|---|---|---|
| Abseil | 20250814.1 | 标准容器、字符串和算法的高性能补充 | 内部实现；按需使用 | 具体的 `absl::*` target | 已安装 |
| Boost.UUID | 1.90.0#1 | UUID 生成与表示 | base/internal identity | `Boost::uuid` | 已安装 |
| BqLog | 2.3.1 | 可选高性能日志后端 | diagnostics/logging integration | `BqLog::BqLog` | 安装产物存在；当前 vcpkg registry 未列出 |
| fmt | 12.1.0 | 字符串格式化、错误和诊断消息构造 | base 及各内部模块 | `fmt::fmt` | 已安装 |
| frozen | 1.2.0 | 编译期 constexpr 不可变容器 | 内部实现 | `frozen::frozen` | 已安装 |
| GoogleTest | 1.17.0#2 | 单元、contract 与集成测试 | tests only | `GTest::gtest_main` 等 | 已安装 |
| magic_enum | 0.9.7#1 | 编译期枚举辅助与内部反射 | base、生成代码和内部 codec | `magic_enum::magic_enum` | 已安装 |
| minizip-ng | 4.0.10#1 | ZIP 容器读取、写入和 entry 流 | zip | `MINIZIP::minizip-ng` | 已安装 |
| OpenSSL | 3.6.0#3 | 加密、解密、Hash、KDF 等密码学 primitive | crypto | `OpenSSL::Crypto`；确需 TLS 时才使用 `OpenSSL::SSL` | 已安装 |
| pugixml | 1.15#1 | XML DOM 解析与序列化后端 | xml | `pugixml::pugixml` | 已安装 |
| RE2 | 2025-11-05 | 线性时间正则匹配 | 明确需要正则的格式/工具模块 | `re2::re2` | 已安装 |
| simdutf | 8.0.0 | SIMD 加速 UTF 校验、检测与转码，为 XML 前端提供快速编码路径 | io、xml | `simdutf::simdutf` | 已安装 |

“已安装”只描述当前开发机的 `x64-windows-static-md` 状态，不替代 `deps.json`，也不构成其他开发环境必须具有相同绝对路径的要求。

## 3. 当前开发机的查找位置

当前 Windows 开发环境使用：

```text
VCPKG_ROOT=E:/Development/vcpkg
triplet=x64-windows-static-md
installed root=E:/Development/vcpkg/installed/x64-windows-static-md
```

开发者和大模型应按以下顺序检查：

1. `deps.json`：确认项目固定的名称、版本、上游和许可证。
2. `installed/<triplet>/share/<port>/usage`：查看 vcpkg 提供的标准 CMake 用法。
3. `installed/<triplet>/share/<port>/*Config.cmake` 与 `*Targets.cmake`：确认 imported target 的准确名称、编译定义和传递依赖。
4. `installed/<triplet>/include/`：阅读实际安装版本的公开头文件，确认命名空间、函数签名、所有权和错误契约。
5. `vcpkg list --triplet x64-windows-static-md`：核对注册的安装版本。
6. `vcpkg owns <path-or-name>`：不确定某个文件属于哪个 port 时进行归属检查。

不得直接修改 `installed/` 中的文件。它们是构建产物和接口证据，不是 xmole2 源码。

## 4. 查找上游源码

仅凭安装头文件无法理解行为、复杂度或错误契约时，可以阅读源码。顺序如下：

1. 检查 `E:/Development/vcpkg/ports/<port>/vcpkg.json` 与 `portfile.cmake`，确定 vcpkg 使用的上游版本、补丁和构建选项。
2. 检查 `E:/Development/vcpkg/buildtrees/<port>/` 中当前版本的已解包源码；该目录可能被清理，不能作为长期链接目标。
3. 若本地没有源码，使用 `deps.json` 中的 `repository` 访问官方 GitHub 仓库，并切换到与固定版本对应的 tag/commit。
4. 同时阅读 `homepage` 或官方 API 文档；博客、问答和非官方镜像只能作为辅助材料。
5. vcpkg 版本中的 `#N` 是 port revision，不是上游 tag 的一部分。必须通过 port 元数据确定真实上游版本。

大模型在提出代码前必须说明它检查的是哪个版本和哪个接口来源。若本地安装、vcpkg port 与上游 main 分支不一致，以 `deps.json` 固定版本和 vcpkg 实际构建内容为准。禁止根据上游 main 的新 API 编写当前版本无法编译的代码。

## 5. 各库使用准则

### 5.1 Abseil

- 用于标准库不足或可测量收益明确的容器、字符串与算法能力。
- 常用候选包括 `flat_hash_map`、`flat_hash_set`、`btree_map`、`btree_set`、`InlinedVector` 和 strings utilities。
- 长期存储必须保持清晰所有权，禁止为减少一次拷贝而把 owning key 变成悬垂 `string_view`。
- 不链接笼统的"全部 Abseil"；按使用接口选择最小 `absl::*` targets。

### 5.2 Boost.UUID

- 用于确需 UUID 的内部身份，不自动替代稳定 NodeId/Generation 句柄模型。
- 通过 vcpkg port `boost-uuid` 安装，CMake 导入目标为 `Boost::uuid`。
- `boost/uuid.hpp` 为仅头文件依赖，链接时传递 `Boost::assert`、`Boost::config`、`Boost::throw_exception`、`Boost::type_traits`。

### 5.3 BqLog

- BqLog 是可选日志后端，不能替代 `Result<...>`、Error、DiagnosticSink 或 SaveReport。
- 日志不得参与控制流，也不得记录密码、密钥、文档正文或其他敏感内容。
- 当前安装文件提供 `BqLog::BqLog` target，但未出现在本机 `vcpkg list` 注册结果中；升级和重新安装前必须查明其来源并补齐 provenance。

### 5.4 fmt

- 用于项目内部格式化、错误消息和诊断文本。
- 字符串拼接仍遵循项目代码风格；不得把 fmt formatter 或 format context 暴露到公共 API。
- 格式化不得出现在未经测量的热点循环中。

### 5.5 frozen

- frozen 提供编译期确定的 `constexpr`/`consteval` 映射与集合（`frozen::map`、`frozen::unordered_map`、`frozen::set`、`frozen::unordered_set`）。
- 所有容器在编译期初始化，无运行时构造开销。适用于固定字符串枚举、格式/类型到处理的查找表等场景。
- 保持映射小型且集中；不要为了复用 frozen 而将运行时数据硬塞进 `constexpr` 访问模式。
- 按需包含最小头文件：`<frozen/unordered_map.h>`、`<frozen/map.h>`、`<frozen/set.h>`、`<frozen/unordered_set.h>`。
- 公共 API 不得暴露 frozen 类型。

### 5.6 GoogleTest

- 仅用于测试 target，不能成为任何安装组件的运行时依赖。
- 优先围绕公开契约建立 tests；实现细节测试不能替代 contract、round-trip 和互操作测试。

### 5.7 magic_enum

- 用于内部枚举辅助，不能把编译器生成的枚举名称当作持久文件格式或稳定公共协议。
- OOXML/二进制格式的枚举字符串映射仍由固定 schema/codegen 表定义。

### 5.8 minizip-ng

- 只能位于 `xmole2::zip` 后端，OPC 和产品 codec 通过 xmole2 port 访问。
- 所有 entry 读取、解压和写入受共享 ResourceBudget、取消和错误链约束。
- 不得把 `mz_*` handle、错误码或结构体暴露到公共 API。

### 5.9 OpenSSL

- 只能通过 `xmole2::crypto` adapter 使用；产品 codec 和 OPC 不直接调用 OpenSSL。
- OpenSSL error stack 必须转换为统一 Error cause/native_code，不得只保留一条文本。
- 密码、密钥和明文不得写入 BqLog、fmt 诊断或测试快照。

### 5.10 pugixml

- 作为 `xmole2::xml` 的私有 DOM 后端，不得成为领域模型或公开句柄。
- 高保真 source span、token 保留和 patch writer 是 xmole2 的上层能力，不能错误宣称由 pugixml 自动提供。
- 必须结合 ResourceBudget 限制 XML 深度、节点、属性和文本规模。

### 5.11 RE2

- 用于需要可预测线性时间行为的正则匹配。
- 不得依赖 RE2 不支持的回溯特性；模式应在循环外编译并复用。
- 简单前缀、字符查找或固定模式优先使用标准字符串算法。

### 5.12 simdutf

- simdutf 负责 UTF 编码检测、校验与转码，不负责 XML 语法解析。
- XML 输入路径可以先使用 simdutf 验证/转成内部约定编码，再交给 pugixml 或流式 XML parser。
- SIMD 快速路径必须保留正确的标量 fallback，并由真实 Office fixture 和 benchmark 验证收益。

## 6. 依赖变更流程

增加或升级依赖必须同时完成：

1. 明确现有标准库或已声明依赖为何不能满足需求。
2. 记录预期模块、性能/正确性收益、许可证与供应链风险。
3. 更新 `deps.json` 和本文。
4. 使用 imported target 完成 PRIVATE 构建集成。
5. 增加最小 contract、错误路径和必要 benchmark。
6. 检查安装导出的 public headers 与 CMake targets，确认没有第三方泄漏。
7. 对新增的长期架构依赖提交 ADR。

