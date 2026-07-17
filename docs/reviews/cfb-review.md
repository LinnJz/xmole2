# CFB 模块结构化代码审查报告

审查日期：2026-07-17  
审查范围：`xmole::cfb` 
规范基线：相关.md文档  
对照测试：`tests/cfb/`

---

## 1. 架构对齐度

### 1.1 Target 定义与依赖方向

`libs/cfb/CMakeLists.txt` 定义 `xmole2_cfb`，ALIAS 为 `xmole2::cfb`。PUBLIC 依赖 `xmole2::base` 与 `xmole2::io`，与 `architecture.md` §3 规范 target 表中的 `base、io` 声明一致。

`libs/CMakeLists.txt` 中 `xmole2::cfb` 被以下 target 依赖：`office-encryption`、`words-doc`、`cells-xls`、`slides-ppt`。这些是二进制 codec 和加密模块，符合 architecture.md 中 `Binary codecs → cfb → io → base` 的依赖链。

### 1.2 Public Header 第三方类型泄漏

全部 7 个 public header 只使用了 xmole2 自有类型（`Error`、`Result`、`OperationContext`、`SourceLease`）和标准库类型（`cstdint`、`vector`、`string`、`span`、`memory`）。无第三方类型泄漏。✓

### 1.3 模块边界

- `cfb_stream_reader.hpp` 不暴露 `Impl` 类型，使用 `unique_ptr<Impl>` 隐藏实现。✓
- `MiniStreamAllocationTable` 包含 `DirectoryIndex`，`DirectoryIndex` 包含 `SectorAllocationTable`，`SectorAllocationTable` 包含 `CompoundFileHeader`。**C1**：嵌套深度达 4 层（`table.directory_index.allocation_table.fat_entries`），但保持了所有权清晰。符合 spec §4 中 header → DIFAT/FAT → directory → MiniFAT → stream 的分层验证顺序。
- 不依赖 platform OLE handle、参考项目 API、异常错误模型或整文件物化。✓

---

## 2. 正确性

### **A1** `map_io_error` 缺失 UnexpectedEndOfFile 映射

- **位置**：`src/sector_reader_internal.cpp:21-34`、`src/cfb_stream_reader.cpp:314-327`
- **问题**：`sector_reader_internal` 和 `cfb_stream_reader` 中的 `map_io_error` 函数只映射 `Cancelled` 和 `ResourceLimitExceeded`，缺少 `UnexpectedEndOfFile → InvalidHeader/InvalidStream` 的映射（对比 `compound_file_header.cpp:63-66`）。当 DIFAT/FAT/directory 或 stream payload 的 `read_exact` 因截断失败时，错误被报告为 `ReadFailed` 而非更具体的 CFB domain 码。
- **影响**：降低了错误诊断精度；cause 链仍保留原始 `UnexpectedEndOfFile`。建议对齐映射逻辑。

### A2（无）— 输入验证

- 溢出防御：`checked_add`/`checked_multiply` 覆盖所有 sector/offset 运算。✓
- 来源大小对齐到 sector 检查（`compound_file_header.cpp:191-192`）。✓
- 路径遍历：无路径/文件名用于 I/O，不适用。✓
- 空字节：目录名字段检测内嵌 null（`directory_index.cpp:172-176`）。✓
- UTF-16 合法性：`is_valid_utf16()` 在 `directory_index.cpp:84-108` 检查代理对完整性。✓

### A3（无）— 资源清理与异常安全

- 所有函数返回 `Result<T>` 或 `Status`，无异常抛出。✓
- 所有 allocate-on-heap 的对象使用 RAII（`unique_ptr`、`vector`）。✓
- 析构函数不执行可能失败的 I/O（`~CfbStreamReader()` = `default`，无 flush/close）。✓

### A4（无）— 线程安全

- `CfbStreamReader` 是 move-only 且文档标注单线程顺序 reader。✓
- `OperationContext` 中的 `cancellation` token 允许线程安全取消，但不保证 reader 本身线程安全。符合 spec §5 明确声明。

### A5（无）— 目录树验证

- Red-black 约束、单一入边、cycle、root 可达性、名称排序均正确实现。
- `compare_directory_names` 使用生成的 Unicode simple-uppercase 表，正确处理 surrogate（不 decode、不变换）。✓

---

## 3. 性能

### B1（无）— 无显著性能问题

- 所有 I/O 按需读取：header 仅 512 字节、sector-by-sector DIFAT/FAT/directory/MiniFAT、stream payload 不预读。符合 spec §6 "不得在无 benchmark 时预计算完整 chain"。
- `read_u32`/`read_u16` 在内联头文件中展开，无函数调用开销。
- `detect_fat_cycles` 使用 `states[]` + `path[]` 一次遍历 O(n) 检测。✓
- 无不必要的堆分配：`sector_bytes` 向量在 DIFAT/FAT 读取中复用。✓
- 无虚函数分派在 hot path 上（`read_sector` 是普通 `free function`）。

---

## 4. 设计合理性

### **C1** 深层对象嵌套

- **位置**：`include/xmole2/cfb/mini_stream_allocation_table.hpp:17-23` → `directory_index.hpp:51-54` → `sector_allocation_table.hpp:17-22`
- **说明**：`MiniStreamAllocationTable` 持有 `DirectoryIndex`，后者持有 `SectorAllocationTable`，后者持有 `CompoundFileHeader` 及 `fat_entries` 向量。`CfbStreamReader::open()` 中访问路径为 `table->directory_index.allocation_table.header`，深 4 层。嵌套使所有权清晰、避免外部生命周期管理，但增大了编译期耦合和类型体积。当前设计可接受，建议在 1.0 前评估是否可以用 span/ref 替代某些层次的 owned copy。

### **C2** Reader open 消费 MiniStreamAllocationTable

- **位置**：`src/cfb_stream_reader.cpp:411-421`
- **说明**：`CfbStreamReader::open()` 从 `MiniStreamAllocationTable` move 出 `allocation_entries` 和 `root_mini_stream_sector_ids`，使 table 结构在 open 后不完整。消费语义与 `CfbStreamReader` 单次使用的意图一致，但若未来需要同时打开多个 stream reader 或查询 table 元数据（如通过 entry ID 查 stream_size）则会失效。

### C3（无）— Move 语义

- `CfbStreamReader` 正确禁用 copy、启用 move。测试使用 `static_assert` 验证。✓
- `SourceLease` 和 `RandomAccessReader` 也是 move-only。✓

### C4（无）— Budget 与取消

- 所有公开路径接受 `OperationContext`，在循环内检查 `is_cancelled()`。✓
- 内存预算计算细致覆盖各中间向量（roles、states、incoming、path、stack、sector_bytes）。✓
- `ResourceLimitExceeded` 返回 CFB domain 错误，不伪装为损坏。✓

---

## 5. 测试覆盖

### 5.1 Public API 覆盖

| API | 正常路径 | 错误路径 |
|---|---|---|
| `read_header` | v3/v4 header, minor version advisory, bounded read | truncated, invalid signature, unsupported version, wrong byte order, sector count overflow, budget, cancellation, empty source, I/O error chain |
| `read_sector_allocation_table` | embedded DIFAT (v3/v4), extended DIFAT chain | DIFAT cycle, out-of-range, FAT cycle, role mismatch, duplicate DIFAT, budget, running cancellation, I/O error chain |
| `read_directory_index` | v3/v4 entries, multi-sector chain, state bits, red root, surrogate ordering, unicode ordering | chain cycle, table sector ref, v4 count mismatch, malformed entries (odd name, invalid type, invalid UTF-16, unused metadata), v3 stream size >2GiB, null terminated, red-black constraints, duplicate name, bad reference, stream child, tree cycle, budget, cancellation, error chain |
| `read_mini_stream_allocation_table` | v3/v4 lazy mapping, absent MiniFAT/empty root | MiniFAT chain short/long, role overlap, MiniFAT entry range, bad padding, shared sector, cycle, root stream short/long, overlap with MiniFAT, root out-of-range, budget, progress, cancellation, error chain |
| `CfbStreamReader::open` | regular/mini lazy, source lifetime escape, move-only | invalid entry, root attempt, empty stream (EOF immediate) |
| `CfbStreamReader::read` | chunked read (mini/regular), EOF detection, position/size/finished/storage | chain short (regular/mini), sharing, budget (open + read), cancellation, empty destination, I/O error chain, progress |

### 5.2 测试覆盖缺口

| 编号 | 缺口 |
|---|---|
| **T1** | 无 cancellation 测试在 FAT cycle detection / 目录树遍历 / MiniFAT cycle detection 的计算阶段（所有现有取消测试在 I/O 读取阶段注入） |
| **T2** | 无 >4GB 文件边界测试（如 `sector_count` 接近 `kMaxRegularSector`），可以撰写代码但不实际做真实>4GB文件，因为测试的文件数据没有>4GB的，可以代码模拟 |
| **T3** | 无 stream `read()` 的跨 FAT sector 边界（FAT entry 来自不同 FAT sector）的测试 |
| **T4** | 无精确大小 read 测试（`destination.size() == remaining`，应正好填满并到达 EOF 或链末尾） |
| **T5** | 无 `first_mini_fat_sector == kEndOfChain` 但 `mini_fat_sector_count > 0` 的测试（header 验证应拒绝） |
| **T6** | 无 `read_mini_stream_allocation_table` 中 progress phase `cfb.minifat` 和 `cfb.root_mini_stream` 计数验证 |

---

## 6. 代码质量细节

### **D1** Progress 计数不准确

- **位置**：`src/cfb_stream_reader.cpp:306-308`
- **问题**：`cfb.stream.validate` 的 progress 使用 `entry.index + 1 / entries.size()`，但循环跳过了非 stream 和空 stream 的 entry，导致进度比例偏移。

### **D2** 测试辅助函数大量重复

- **位置**：5 个测试文件中 `write_u16`、`write_u32`、`write_u64`、`sector_size`、`sector_offset`、`fill_sector`、`write_fat_entry` 等辅助函数完全重复（每文件约 50 行）。
- **建议**：提取到 `tests/cfb/cfb_test_utils.hpp`。

### **D3** `std::min` 临时对象引用风险

- **位置**：`tests/cfb/cfb_sector_table_contract_test.cpp:268`
- **问题**：`std::min(destination.size(), m_bytes.size())` 返回对临时 `size_t` 的引用。虽然在完整表达式结束前 `auto count` 复制了值（不 UB），但代码脆弱，应使用 `std::min<std::size_t>` 或本地变量。

### **D4** 测试 fixture 依赖隐式零初始化

- **位置**：`tests/cfb/cfb_directory_contract_test.cpp:171-172`
- **问题**：`std::vector<std::byte>(count)` 进行 value-initialization（全零），CLSID 字节 81-95 依赖此隐式行为。如果将来改用 `resize` 或其他初始化方式，可能引入不确定值。

### D5（无）— 命名与格式

- 命名遵循 `kPascalCase` 常量、`snake_case` 变量/函数、`m_` 成员。✓
- 全部使用 trailing return type。✓
- `#pragma once` 保护。✓
- `clang-format` 一致性：无格式化问题。✓

### D6（无）— 跨平台

- 唯一平台分支在 `export.hpp`，`_WIN32` 和 `__GNUC__` 两端都有实现。✓
- 所有整数布局假设使用显式 little-endian 读取宏，不依赖平台字节序。✓

---

## Review Handoff

### 本次审查
- **模块**：`xmole2::cfb`
- **文件覆盖**：
  - 7 public headers: `export.hpp`, `error.hpp`, `compound_file_header.hpp`, `sector_allocation_table.hpp`, `mini_stream_allocation_table.hpp`, `directory_index.hpp`, `cfb_stream_reader.hpp`
  - 13 implementation files: `CMakeLists.txt`, `error.cpp`, `compound_file_header_internal.hpp`, `compound_file_header.cpp`, `cfb_internal_utils.hpp`, `sector_reader_internal.hpp`, `sector_reader_internal.cpp`, `sector_allocation_table.cpp`, `directory_index.cpp`, `cfb_name_internal.hpp`, `cfb_name_internal.cpp`, `unicode_simple_uppercase_generated.hpp`, `mini_stream_allocation_table.cpp`, `cfb_stream_reader.cpp`
  - 5 contract test files: `cfb_contract_test.cpp`, `cfb_sector_table_contract_test.cpp`, `cfb_directory_contract_test.cpp`, `cfb_mini_stream_contract_test.cpp`, `cfb_stream_reader_contract_test.cpp`
  - 2 spec documents: `docs/spec/cfb.md`, `docs/spec/architecture.md`
  - 2 cross-reference specs: `docs/spec/io.md`, `docs/spec/security.md`

### 关键发现

| 类别 | 数量 | 编号 |
|---|---|---|
| 正确性问题 | 1 | A1 |
| 性能问题 | 0 | — |
| 设计问题 | 2 | C1, C2 |
| 微问题 | 4 | D1–D4 |
| 测试缺口 | 6 | T1–T6 |

### 待修复优先级建议

1. **A1** — 对齐 `sector_reader_internal.cpp` 和 `cfb_stream_reader.cpp` 中的 `map_io_error` 以包含 `UnexpectedEndOfFile` 映射，与 `compound_file_header.cpp` 保持一致。
2. **D1** — 修正 `cfb.stream.validate` progress 计数，使用实际已验证的 stream 数量。
3. **D3** — 修复 `cfb_sector_table_contract_test.cpp:268` 的 `std::min` 临时引用。
4. **D2** — 提取测试公用辅助函数到共享头文件。
5. **T1–T6** — 在现有测试框架中增加缺口测试。

### 需补充信息

- 无。

### 对应 spec 状态

- 全部对齐。实现与 `docs/spec/cfb.md` 没有发现规范偏差。
- 测试覆盖的深度和广度符合 §7 Contract 与负向测试的清单要求（v3/v4 header、advisory minor value、Header MUST 字段、DIFAT/FAT、directory chain、v3 Stream Size 高 DWORD 兼容与 2 GiB 上限、root/stream/unallocated entry 字段约束、directory graph/color、Unicode simple-uppercase sibling 排序、大小写不敏感重复名、代理 code-unit 恒等、MiniFAT/root mapping、regular/mini lazy payload、reader 逃逸生命周期、EOF、共享/短链、预算、运行中取消、progress、完整 I/O cause/native_code 均已覆盖）。

---

## 7. 修复记录（2026-07-17）

| 编号 | 状态 | 处置 |
|---|---|---|
| A1 | 已修复 | 统一 CFB 私有 I/O error wrapper；缓存长度内提前 EOF 按 header、sector table、directory、MiniFAT、stream payload 的所在阶段映射，保留 I/O cause/native_code。 |
| D1 | 已修复 | `cfb.stream.validate` 改用实际非空 stream 数量作为 completed/total。 |
| D2 | 已修复 | 公共 fixture primitive 提取到 `tests/cfb/cfb_test_utils.hpp`。 |
| D3 | 已修复 | `std::min` 显式固定为 `std::size_t`。 |
| D4 | 已修复 | directory fixture 明确指定 `std::byte {}` 零值初始化。 |
| T1 | 已补齐 | FAT、directory、MiniFAT 在 I/O 完成后的纯计算验证阶段均有取消 contract。 |
| T2 | 已补齐 | 虚拟 source 覆盖最大可寻址 regular sector 边界及越界拒绝，不分配真实超大文件。 |
| T3/T4 | 已补齐 | 单次精确大小读取跨越两个 FAT sector table，验证 payload、finished 与后续 EOF。 |
| T5 | 已补齐 | 非零 MiniFAT sector count 配合 `ENDOFCHAIN` 起点的 header 负向测试。 |
| T6 | 已强化 | `cfb.minifat` 与 `cfb.root_mini_stream` 同时校验 update 次数及最终 completed/total。 |

C1 与 C2 保留为 1.0 前评估项：当前深层 owning 结构没有越层依赖或可观察正确性问题；`CfbStreamReader::open()` 消费的是本次调用内部新建的临时 table，尚无共享 allocation metadata 或多 reader factory 的公共契约。若后续以 span/ref 或共享索引改变所有权、生命周期或公开 API，须先以实际多 reader 场景和内存/吞吐数据重新评估，并按公共契约变更流程更新 spec/ADR。
