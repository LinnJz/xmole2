# `xmole2::io` [DONE、git未提交]

M1 的 `xmole2::io` 切片已完成，包括公共接口、平台后端、测试、安装导出和独立 consumer 验证。

关键实现：

- `ByteSource`、`ByteSink`、`RandomAccessReader`
- move-only `SourceLease`、owning buffer、稳定文件 identity、`detach()`
- 受预算控制的 `TemporaryFile`
- 同目录 `AtomicFileWriter`
- Windows `ReplaceFileW`/`MoveFileExW` 与 POSIX rename 路径
- 取消、资源限制、native error 和 cause 链贯通
- 新增规范：[io.md](D:/xmole2/docs/spec/io.md)

验证结果：MSVC 14.51 Debug 构建成功，CTest 4/4 通过，安装后的 `xmole2::io` consumer 编译运行成功。尚未暂存或提交；已有未跟踪 `.agents/` 未被修改。

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::io`
- 文件变更：
  - [IO public API](D:/xmole2/libs/io/include/xmole2/io)
  - [IO implementation](D:/xmole2/libs/io/src)
  - [IO contract tests](D:/xmole2/tests/io)
  - [IO specification](D:/xmole2/docs/spec/io.md)
  - [IO CMake target](D:/xmole2/libs/io/CMakeLists.txt)
- 实现内容：完成随机字节读取、源租约、临时存储与原子文件替换的首个跨平台实现。

### 状态

- 本轮 contract test：`1/1`；完整 CTest：`4/4`
- 编译：通过，MSVC 14.51 Debug
- 已知问题或待完善项：
  - POSIX 分支已实现，但尚未在 Linux/macOS CI 实际编译。
  - M1 的 ZIP entry 索引/流式读取及 CFB contract 尚未开始。
  - `.agents/` 是此前已有的未跟踪目录，本轮未修改。

### 与 spec 一致性

- 已对照 `architecture.md`、`public-api.md`、`security.md`、`testing.md`、`migration.md`、`dependencies.md`、`code-style.md`：全部对齐。
- 新增 `io.md` 固化本轮公共契约。
- 未决决策：
  - OfficeDocument 表示方式，office-runtime 前提交 ADR。
  - EditSession 句柄可见性，公开事务 API 前提交 ADR。

### 下轮任务建议

- 按 migration.md 阶段：M1，实现 `xmole2::zip` 的 entry 索引和流式读取。
- 优先级：高
- 开始前应先读取：`docs/spec/development-process.md`、`architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`。

M1 的 `xmole2::zip` 切片已完成，当前未暂存、未提交。

实现包括：

- `ZipArchive` 不解压 payload 的不可变 entry 索引。
- move-only `ZipEntryReader`，支持 Store/Deflate 分块读取及 CRC/尺寸验证。
- 稳定 `SourceLease` 生命周期，archive 销毁后 reader 仍可读取。
- entry 数量、路径、内存、单资源及总解压预算。
- 取消、progress、ZIP native error 与底层 I/O cause 链。
- 重复名称、路径穿越、损坏目录、加密及不支持压缩方法检测。
- minizip-ng 4.0.10 PRIVATE 后端与按组件安装依赖发现。

验证结果：

- MSVC 14.51 Debug：通过
- ZIP contract：1/1
- 全量 CTest：5/5
- public header 第三方泄漏检查：16 个全部通过
- clang-format dry-run：通过
- 安装后的 `xmole2::zip` 独立 consumer：干净构建并运行成功

# `xmole2::zip` Cycle Handoff[DONE、git已提交]

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::zip`
- 文件变更：
  - [ZIP public API](D:/xmole2/libs/zip/include/xmole2/zip)
  - [ZIP implementation](D:/xmole2/libs/zip/src)
  - [ZIP contract test](D:/xmole2/tests/zip/zip_contract_test.cpp)
  - [ZIP specification](D:/xmole2/docs/spec/zip.md)
  - [ZIP CMake target](D:/xmole2/libs/zip/CMakeLists.txt)
  - [Package consumer](D:/xmole2/tests/package-consumer)
- 实现内容：完成受预算控制的 ZIP entry 索引、Store/Deflate 流式读取及完整性验证。

### 状态

- 本轮 contract test：1/1
- 完整 CTest：5/5
- 编译：通过，MSVC 14.51 Debug
- 已知问题或待完善项：
  - 尚未在 Linux/macOS CI 实际编译。
  - 当前读取契约只承诺 Store 与 Deflate。
  - ZIP 写入不属于本轮范围。
  - 工作树未暂存、未提交。

### 与 spec 一致性

- 已对照 `development-process.md`、`architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`code-style.md`、`public-api.md`：全部对齐。
- 新增 `zip.md` 固化公共契约。
- minizip-ng 未泄漏到 public header，target 方向保持 `zip → base/io`。
- 未决决策：无新增；OfficeDocument 与 EditSession 决策仍留待对应阶段 ADR。

### 下轮任务建议

- 按 `migration.md` 阶段：M1，实现 `xmole2::cfb` 独立 contract 与首个读取切片。
- 优先级：高
- 开始前应先读取：`development-process.md`、`architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`code-style.md`、`public-api.md`、`docs/adr/0001-architecture-baseline.md`。

# `xmole2::cfb`[1] [DONE、git未提交]

已完成 M1 的首个 `xmole2::cfb` 读取切片：

- 将占位 interface target 替换为可安装的实体 `xmole2::cfb` target。
- 新增 [`read_header()` (line 41)](D:/xmole2/libs/cfb/include/xmole2/cfb/compound_file_header.hpp:41)，按需读取固定 512-byte header，不物化后续 sectors。
- 支持 CFB v3/v4，验证 signature、版本、字节序、sector 参数、文件对齐和声明计数。
- 贯通资源预算、取消、进度，以及带 `cause/native_code` 的 I/O 错误链。
- 新增 6 组 synthetic contract 场景；未复制任何参考项目实现。
- 更新了 [migration.md (line 27)](D:/xmole2/docs/spec/migration.md:27)，未将尚未实现的 FAT/目录/stream 标为完成。

验证结果：

- MSVC 14.51 Debug 全量构建通过。
- CTest：6/6 通过。
- `xmole2.cfb.contract`：1/1 target、6/6 场景通过。
- public-header、依赖 smoke、clang-format dry-run、安装导出均通过。

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::cfb`
- 文件变更：[CFB 模块](D:/xmole2/libs/cfb)、[CFB contract](D:/xmole2/tests/cfb/cfb_contract_test.cpp)、[模块注册 (line 11)](D:/xmole2/libs/CMakeLists.txt:11)、[测试注册 (line 33)](D:/xmole2/tests/CMakeLists.txt:33)、[迁移状态 (line 27)](D:/xmole2/docs/spec/migration.md:27)
- 实现内容：建立独立 CFB target，并完成受预算、可取消、保留错误链的 v3/v4 header 读取契约。

### 状态

- 本轮 contract test：1/1 target，6/6 场景
- 编译：通过
- 已知问题或待完善项：DIFAT/FAT、MiniFAT、directory 和惰性 stream chain 尚未实现。

### 与 spec 一致性

- 已对照 `architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`reference-snapshots.md`、`public-api.md`、`code-style.md`：全部对齐
- 未决决策：本切片无；未引入句柄、事务或保真公共契约。

### 下轮任务建议

- 按 migration.md 阶段：M1，实现 DIFAT/FAT sector table 读取与循环、越界、预算防御
- 优先级：高
- 开始前应先读取：`docs/spec/architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`docs/reference-snapshots.md`、`docs/adr/0001-architecture-baseline.md`

# `xmole2::cfb`[2] [DONE、git未提交]

已完成 M1 DIFAT/FAT sector-table 切片。

核心入口为 [`read_sector_allocation_table()` (line 25)](D:/xmole2/libs/cfb/include/xmole2/cfb/sector_allocation_table.hpp:25)，实现包括：

- header 内嵌 DIFAT 与扩展 DIFAT chain。
- 按 DIFAT 顺序读取 FAT sectors，不读取 directory/stream 数据。
- FAT/DIFAT 重复、角色冲突、marker 不一致和 padding 校验。
- DIFAT chain 与全 FAT 图循环检测。
- sector 引用、offset、容量及地址范围检查。
- `max_cfb_sector_count`、`max_cfb_stream_chain_length`、`max_memory_bytes`。
- 运行中取消、进度报告和完整 I/O `cause/native_code` 链。
- 未使用或复制参考项目实现。

验证结果：

- MSVC 14.51 Debug 全量构建通过。
- CTest：7/7 通过。
- 新增 contract：1/1 target、6/6 场景通过。
- 安装导出、public-header 检查及 clang-format dry-run 通过。

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::cfb`
- 文件变更：[sector_allocation_table.hpp (line 16)](D:/xmole2/libs/cfb/include/xmole2/cfb/sector_allocation_table.hpp:16)、[sector_allocation_table.cpp (line 299)](D:/xmole2/libs/cfb/src/sector_allocation_table.cpp:299)、[header 内部解析 (line 129)](D:/xmole2/libs/cfb/src/compound_file_header.cpp:129)、[CFB 错误码 (line 23)](D:/xmole2/libs/cfb/include/xmole2/cfb/error.hpp:23)、[contract (line 316)](D:/xmole2/tests/cfb/cfb_sector_table_contract_test.cpp:316)、[migration 状态 (line 28)](D:/xmole2/docs/spec/migration.md:28)
- 实现内容：完成有界、可取消的 DIFAT/FAT sector-table 读取及循环、越界、角色与预算防御。

### 状态

- 本轮 contract test：1/1 target，6/6 场景
- 编译：通过
- 已知问题或待完善项：MiniFAT、directory index 与惰性 stream reader 尚未实现；CFB 容器整体仍未完成。

### 与 spec 一致性

- 已对照 `architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`reference-snapshots.md`、`development-process.md`、`code-style.md`、ADR-0001：全部对齐
- 未决决策：本切片无；所有发现的结构错误均为致命错误，因此不产生可恢复 diagnostics。

### 下轮任务建议

- 按 migration.md 阶段：M1，实现 FAT 驱动的 directory sector chain 与 128-byte directory entry 索引
- 优先级：高
- 开始前应先读取：`docs/spec/architecture.md`、`docs/spec/io.md`、`docs/spec/security.md`、`docs/spec/testing.md`、`docs/spec/migration.md`、`docs/spec/public-api.md`、`docs/reference-snapshots.md`、`docs/adr/0001-architecture-baseline.md`

# `xmole2::cfb`[3] [DONE、git未提交]

已完成 M1 的 FAT 驱动 directory sector chain 与 128-byte directory entry 索引。

实现覆盖多 sector chain、v3/v4、UTF-16 名称、目录引用图、循环/越界检测、预算与取消，以及 I/O 错误链保留。全量 CTest `8/8` 通过，安装导出和 clang-format 检查通过。

主要文件：[directory_index.hpp](D:/xmole2/libs/cfb/include/xmole2/cfb/directory_index.hpp)、[directory_index.cpp](D:/xmole2/libs/cfb/src/directory_index.cpp)、[cfb_directory_contract_test.cpp](D:/xmole2/tests/cfb/cfb_directory_contract_test.cpp)、[migration.md](D:/xmole2/docs/spec/migration.md)。

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::cfb`
- 文件变更：`directory_index.hpp/.cpp`、共享 sector reader、CFB error/CMake、directory contract、`migration.md`
- 实现内容：读取 FAT directory chain，并按物理 directory ID 建立经过完整验证的 128-byte entry 索引。

### 状态

- 本轮 contract test：6/6 场景通过；全量 CTest 8/8 通过
- 编译：MSVC 14.51 Debug 通过；安装导出通过
- 已知问题或待完善项：MiniFAT、root mini-stream 映射与惰性 stream reader 尚未实现

### 与 spec 一致性

- 已对照 `architecture.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`public-api.md`、`reference-snapshots.md`、ADR 0001：全部对齐
- 未决决策：无

### 下轮任务建议

- 按 migration.md 阶段：M1，实现 MiniFAT table 与 root mini-stream sector mapping
- 优先级：高
- 开始前应先读取：`docs/spec/architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`public-api.md`、`docs/reference-snapshots.md`、`docs/adr/0001-architecture-baseline.md`

# `xmole2::cfb`[4-MiniFAT table] [DONE、git未提交]

已完成 M1 的 MiniFAT table 与 root mini-stream sector mapping。

新增 [mini_stream_allocation_table.hpp (line 16)](D:/xmole2/libs/cfb/include/xmole2/cfb/mini_stream_allocation_table.hpp:16) 和对应实现，支持 v3/v4、MiniFAT/FAT chain 映射、角色冲突、越界、循环、共享 mini-sector、链长、内存/资源预算、取消及 I/O cause/native_code 传播。root payload 保持惰性，映射阶段不会读取其内容。

验证结果：MSVC 14.51 Debug 构建通过，全量 CTest `9/9`，新 contract `6/6` 场景组通过，格式检查、public-header 检查及安装后独立 consumer 均通过。工作树未暂存、未提交。

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::cfb`
- 文件变更：[公共 API](D:/xmole2/libs/cfb/include/xmole2/cfb/mini_stream_allocation_table.hpp)、[实现 (line 451)](D:/xmole2/libs/cfb/src/mini_stream_allocation_table.cpp:451)、[contract](D:/xmole2/tests/cfb/cfb_mini_stream_contract_test.cpp)、[迁移状态 (line 30)](D:/xmole2/docs/spec/migration.md:30)
- 实现内容：完成受预算、可取消的 MiniFAT table 读取与 root mini-stream regular-sector 映射，不物化 payload。

### 状态

- 本轮 contract test：`1/1` target，`6/6` 场景组
- 完整 CTest：`9/9`
- 编译：通过，MSVC 14.51 Debug
- 已知问题或待完善项：惰性 regular/mini stream reader 尚未实现；CFB 容器整体仍未完成。

### 与 spec 一致性

- 已对照 `architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`public-api.md`、`reference-snapshots.md`、ADR-0001 及 MS-CFB v20240423：全部对齐
- 未引入第三方依赖或第三方公共类型
- 未决决策：无

### 下轮任务建议

- 按 migration.md 阶段：M1，实现惰性 regular/mini stream chain reader
- 优先级：高
- 开始前应先读取：`docs/task-records.md`、`docs/spec/architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`public-api.md`、`docs/reference-snapshots.md`、`docs/adr/0001-architecture-baseline.md`、`cfb处理需要官方文档描述时读ms-cfb\`

# `xmole2::cfb`[5-stream reader] [DONE、git未提交]

已完成 M1 的惰性 regular/mini stream chain reader。新增 move-only `CfbStreamReader`，打开阶段只读取并验证 CFB allocation/directory 元数据，不读取所选 stream payload；`read()` 按 FAT 或 MiniFAT/root mapping 分块读取声明范围，并在原 `SourceLease` 销毁后继续保持稳定源。

验证覆盖 v3/v4、regular/mini 跨 sector 读取、EOF 截断、零长度 stream、chain 短链/共享、预算、取消、progress、payload 故障的 cause/native_code 传播及打开阶段零 payload 读取。

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::cfb`
- 文件变更：`cfb_stream_reader.hpp/.cpp`、CFB error/CMake、stream reader contract、`migration.md`
- 实现内容：完成受预算、可取消、保持源生命周期的惰性 regular/mini stream reader，并验证 stream chain 独占性和声明长度。

### 状态

- 本轮 contract test：`1/1` target，`5/5` 场景组
- 完整 CTest：`10/10`
- 编译：MSVC 14.51 Debug 通过；安装后的 `xmole2::cfb` 独立 consumer 编译运行通过
- 已知问题或待完善项：Linux/macOS 尚未实际编译；CFB 写入不属于 M1 范围

### 与 spec 一致性

- 已对照 `architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`public-api.md`、`reference-snapshots.md`、ADR-0001 及 `[MS-CFB]` v20240423 §2.1、§2.2、§2.6.3、§2.7、§4.1：全部对齐
- 未引入第三方依赖或第三方公共类型
- 未决决策：无

### 下轮任务建议

- 按 migration.md 阶段：M2，实现 `xmole2::opc` 的 Part 索引与 ZIP-backed lazy PartStore 首个切片
- 优先级：高
- 开始前应先读取：`docs/task-records.md`、`docs/spec/development-process.md`、`architecture.md`、`dependencies.md`、`io.md`、`zip.md`、`security.md`、`testing.md`、`migration.md`、`public-api.md`、`fidelity.md`、`office-standard.md`、`docs/adr/0001-architecture-baseline.md`、ECMA-376 Part 2 对应章节

# `xmole2::cfb`[6-M1 audit hardening] [DONE、git未提交]

M1 CFB 审计修复已完成，M2 未开始。报告中的有效问题已接纳修复，错误或缺少证据的建议已明确驳回/暂缓。

### 架构对齐度

- `cfb → base/io` 依赖方向保持不变，没有新增第三方依赖或公共 API。
- OLE property set、DOC/XLS/PPT 检测、typed reader 和路径查找没有错误塞入物理 CFB 层。
- 新增规范基线：[cfb.md (line 1)](/D:/xmole2/docs/spec/cfb.md:1)。

### 正确性

- 纠正报告错误：major 3/4 的 Minor Version 都 SHOULD 为 `0x003E`；现在接受并保留任意 minor 值：[compound_file_header.cpp (line 156)](/D:/xmole2/libs/cfb/src/compound_file_header.cpp:156)。
- 修复 v3 Stream Size 高 DWORD 兼容和 2 GiB 上限。
- 严格验证 unallocated entry。
- 修复 root creation time、root color、stream State Bits 规则。
- 增加 sibling-tree root black 和 red-red 约束：[directory_index.cpp (line 195)](/D:/xmole2/libs/cfb/src/directory_index.cpp:195)。

### 性能

- 保留惰性 regular/mini stream 读取。
- 未采纳无 benchmark 的 sector-offset 预计算，避免新增线性常驻内存。
- 各层预算继续按 peak live-set 独立计算。

### 设计

- 仅抽取无领域语义的整数运算、LE 读取和 zero-check：[cfb_internal_utils.hpp (line 13)](/D:/xmole2/libs/cfb/src/cfb_internal_utils.hpp:13)。
- 错误映射、取消信息和不同 `SectorRole` 仍由各层负责。
- Unicode simple-uppercase 排序/唯一性仍是明确记录的待决边界，没有用 ASCII/locale 近似冒充完成。

### 测试覆盖

新增回归覆盖：

- advisory minor 接受和原值保留；
- v3 高 DWORD、2 GiB 上限；
- unallocated entry；
- root/stream metadata；
- root/color 与 red-red 约束。

对应测试见 [header contract (line 175)](/D:/xmole2/tests/cfb/cfb_contract_test.cpp:175) 和 [directory contract (line 488)](/D:/xmole2/tests/cfb/cfb_directory_contract_test.cpp:488)。

### 代码质量

- MSVC 14.51 Debug 构建通过。
- 完整 CTest：`10/10`。
- CFB contract targets：`5/5`。
- 24 个 public headers 无第三方泄漏。
- clang-format、安装导出、安装后独立 consumer、`git diff --check` 均通过。
- 复核处置和周期记录已同步至 [vertical-analysis-report/cfb-report.md (line 5)](/D:/xmole2/docs/vertical-analysis-report/cfb-report.md:5) 与 [task-records.md (line 305)](/D:/xmole2/docs/task-records.md:305)。

## Review Handoff

### 本次审查

- 模块：`xmole2::cfb`
- 文件覆盖：`libs/cfb` 全部 public/private 源码与 CMake、`tests/cfb` 全部 contract、依赖模块 public headers、CFB/spec/ADR/官方标准

### 关键发现

- 6 个正确性问题，均已修复；当前未遗留阻断问题
- 0 个有证据的性能问题
- 1 个已记录的设计边界：Unicode canonical comparison
- 1 个重复 primitive 微问题，已收敛

### 待修复优先级建议

- 有可追溯 Unicode simple-uppercase 数据方案后，再实现完整 sibling 排序和名称唯一性校验。
- 有真实 codec consumer 后，再设计目录路径查找和 bounded typed reader。
- 有互操作 fixture 或 benchmark 后，再评估兼容性与缓存优化。

### 对应 spec 状态

- 已修复发现的偏差并记录当前验证边界，代码、contract、`cfb.md`、`migration.md` 和报告处置一致。

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::cfb`（M1 审计加固）
- 文件变更：CFB header/directory 实现、私有工具、CFB contract fixtures/tests、`docs/spec/cfb.md`、spec 索引、migration、report、task record
- 实现内容：修复 advisory minor 与目录兼容/字段/color 约束，并同步规范和审计处置。

### 状态

- 本轮 contract test：CFB `5/5` targets；完整 CTest `10/10`
- 编译：MSVC 14.51 Debug 通过
- 已知问题或待完善项：Unicode sibling 排序/唯一性尚非 parser 门禁；Linux/macOS 尚未实际编译；路径和 codec convenience API 等待真实消费证据

### 与 spec 一致性

- 已对照 `development-process.md`、`review.md`、`architecture.md`、`dependencies.md`、`io.md`、`security.md`、`testing.md`、`migration.md`、`public-api.md`、`code-style.md`、`office-standard.md`、ADR-0001 与 MS-CFB v20240423：发现差异并已修复
- 未决决策：完整 CFB 名称 canonical comparison 的跨平台 Unicode 数据/策略

### 下轮任务建议

- 按 migration.md 阶段：继续停留在 M1；只在获得互操作 fixture 或 Unicode 方案后追加 CFB hardening，不自动开始 M2
- 优先级：按证据触发
- 开始前应先读取：`docs/task-records.md`、`docs/spec/cfb.md`、`docs/spec/office-standard.md`、`docs/spec/testing.md`、`docs/spec/security.md`、`docs/vertical-analysis-report/cfb-report.md` 与 MS-CFB 对应章节

# `xmole2::cfb`[7-Unicode directory ordering] [DONE、git未提交]

M1 的 Unicode directory ordering 加固已完成，M2 未开始。此前明确要求的证据门槛已经满足：采用可追溯的 Unicode Character Database 17.0.0 simple-uppercase 数据，并以 Microsoft CompoundFileReader 的 `unicode.dat` 做安装后独立互操作验证。

### 证据与决策

- `[MS-CFB]` v20240423 §2.6.4 要求名称先按 UTF-16 code-unit 长度、再按 Unicode simple uppercase 后的 code unit 比较；代理字符不得大写，并允许实现采用更新 Unicode 版本。
- 使用官方 Unicode 17.0.0 `UnicodeData.txt` 第 12 字段 `Simple_Uppercase_Mapping`；输入 SHA-256 为 `2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C`。1,198 个 BMP 映射已逐项复核，零个 BMP source 映射到 BMP 外。
- `cmake/GenerateCfbUnicodeSimpleUppercase.cmake` 校验固定输入哈希，将映射可复现压缩为 192 段并生成私有表；生成表保留完整 Unicode License V3。重复生成的 header SHA-256 均为 `F0E0DBC5465B65C122B9E10D19AC8399B79EF8073078565162F57AD2A6B39ABA`。
- Microsoft fixture 来源为 <https://github.com/microsoft/compoundfilereader> 的 `test/data/unicode.dat`，文件引入 commit `d0f3914ac1b1134387a077d3c76ee1d8eb756be1`，核对 HEAD `4ddb0602600833bc925d76c0f0382ba88c1c9f60`，fixture SHA-256 为 `67BE3CF47AAAE38755A3882A72FE6253D34868B748D08C689CD36072DF5A2633`，MIT。payload 只保存在 ignored `out/evidence/`，不提交 Git。

### 实现与 contract

- 增加私有 `compare_directory_names()`，严格执行 length-first 与逐 UTF-16 code-unit simple-uppercase 比较；代理 code unit 恒等，不使用 locale、平台 API、scalar case conversion 或完整 case folding。
- 对每个 storage sibling BST 做迭代中序验证，严格递增；逆序为损坏，canonical comparison 相等为大小写不敏感重复名。复用现有遍历栈，不新增另一份线性 peak-live allocation。
- contract 新增非 ASCII simple-uppercase 排序、非法 BST、case-insensitive duplicate 与 supplementary surrogate 回归；原 synthetic directory 的 sibling 位置按标准排序修正。
- 没有增加路径查找、OLE property set、格式检测、typed/substream/push-pop API，也没有预计算 sector chain 或合并逐层预算。

## Cycle Handoff

### 本轮完成

- 模块：`xmole2::cfb`（M1 Unicode directory ordering hardening）
- 文件变更：Unicode 表生成器与私有映射/比较实现、directory 验证、CFB directory/stream contract fixtures、`cfb.md`、`office-standard.md`、`migration.md`、report、reference/fixture provenance 与本记录
- 实现内容：把 `[MS-CFB]` Unicode sibling 排序和名称唯一性升级为 parser 门禁，并完成可复现数据与微软 fixture 互操作证据闭环

### 状态

- 本轮 contract test：CFB `5/5` targets；完整 CTest `10/10`
- 编译与安装：MSVC 14.51 Debug 通过；安装导出和独立 package consumer 编译运行通过
- 互操作：安装后的 hardened parser 成功打开 Microsoft `unicode.dat`，得到 5 个 allocated directory entries
- 已知问题或待完善项：Linux/macOS 尚未实际编译；真实 codec consumer 出现前不扩张路径/typed convenience API；CFB 写入不属于 M1 范围

### 与 spec 一致性

- 已对照 `cfb.md`、`office-standard.md`、`testing.md`、`security.md`、`migration.md`、报告处置、`[MS-CFB]` v20240423 §2.6.4/§4.1、UAX #44 Revision 36 与 Unicode 17.0.0 UCD：全部对齐
- 未引入第三方 runtime 依赖或第三方公共类型；生成数据按 Unicode License V3 保留许可文本
- 未决决策：无

### 下轮任务建议

- 继续停留在 M1；只有新的标准差异、可追溯互操作 fixture 或可复现 benchmark 形成具体证据时，才追加 CFB hardening
- 不自动开始 M2
- 开始前应先读取：`docs/task-records.md`、`docs/spec/cfb.md`、`docs/spec/office-standard.md`、`docs/spec/testing.md`、`docs/spec/security.md`、`docs/vertical-analysis-report/cfb-report.md` 与 MS-CFB 对应章节
