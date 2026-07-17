# xmole2::cfb 实现分析报告

对照 MS-CFB v20240423 规范、四份参考实现（compoundfilereader、office_parser、ole-compound-pp、docwire）以及项目架构文档。

## 复核处置（2026-07-17）

本节记录按项目 spec、`[MS-CFB]` v20240423 Markdown 与 OCR 页复核后的处置；下文保留原始报告，不能用其未修订结论覆盖本节。

| 原报告结论 | 处置 | 依据 |
|---|---|---|
| v4 Minor Version 必须为 `0x0004` | **不采纳；原结论错误** | §2.2 明确写明 major 3 或 4 的 Minor Version 都 SHOULD 为 `0x003E`；`0x0004` 是 Major Version 4。当前实现仍需修复“把 SHOULD 当 MUST”的过严行为，接受并保留其他 minor value。 |
| Header CLSID 可为兼容性降级 | **不采纳** | Header CLSID 是 MUST 全零；没有 fixture/interop 证据时不得降级。 |
| 其余 directory 验证均正确 | **不采纳；发现项均已加固** | 首轮复核发现 v3 Stream Size 高 DWORD、2 GiB 上限、root creation time、unused entry、red-black color 与 Unicode 名称排序/唯一性缺口；现均有规范和 contract 门禁。 |
| 目录遍历/名称查找 | **继续暂缓** | canonical comparison 已在本轮落实，但路径 API 仍需 container/source ownership 与真实 codec consumer 证据；不能以数组线性 exact-match 代替标准路径语义。 |
| OLE property set | **不属于当前 CFB 物理容器切片** | 需要 `[MS-OLEPS]` 基线与独立模块/契约。 |
| DOC/XLS/PPT 格式检测 | **不采纳到 CFB** | 格式检测属于 `xmole2::office-runtime` detector registry 与具体 binary codec 证据。 |
| SubStreamReader、push/pop、typed read/write | **暂缓** | 属于 binary codec 消费模式或通用 bounded reader 设计；在真实 DOC/XLS/PPT 切片给出需求前不扩张 CFB public API。 |
| 预计算 sector/mini offset | **不采纳** | 没有 benchmark；会增加与 chain 长度线性相关的常驻内存，违反 `testing.md` 的性能证据原则。 |
| 合并逐层预算计算 | **不采纳** | 各层计算的是自身 peak live-set 与新增暂存，属于模块化安全检查，不是可测量热点。 |
| 提取 CFB 重复 primitive | **采纳一部分** | 只提取无 domain 语义的 checked arithmetic、little-endian read、zero check 与 memory accumulation；保留 contextual error/cancel message、不同 map_io_error 和不同 SectorRole。 |

规范性契约位于 `docs/spec/cfb.md`。此前暂缓的 Unicode sibling 排序/大小写唯一性现已具备证据并成为 parser 门禁：实现固定使用 Unicode 17.0.0 `UnicodeData.txt` 第 12 字段的 BMP simple-uppercase 映射（SHA-256 `2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C`），按 `[MS-CFB]` §2.6.4 先比较 UTF-16 长度、再比较大写后的 code unit，代理 code unit 保持恒等。synthetic contract 覆盖非 ASCII 排序、非法 BST、大小写重复名与 supplementary surrogate 行为；Microsoft CompoundFileReader `unicode.dat`（commit `d0f3914ac1b1134387a077d3c76ee1d8eb756be1`，SHA-256 `67BE3CF47AAAE38755A3882A72FE6253D34868B748D08C689CD36072DF5A2633`）提供本地人工互操作证据。未因该证据扩张路径查找、property set、格式检测或 codec convenience API。

---

## 一、正确性问题

### 1.1 严重：Minor Version 检查过于严格

**位置**：`libs/cfb/src/compound_file_header.cpp:183`

```cpp
if (read_u16(view, 24) != kMinorVersion)  // kMinorVersion = 0x003E
```

**问题**：MS-CFB §2.2 规定 v3 minor version 为 0x003E，v4 为 **0x0004**。当前无条件检查 `!= 0x003E`，会拒绝合法 v4 文件。

**修复**：根据 major version 分别检查：

```
v3 → minor == 0x003E
v4 → minor == 0x0004
```

### 1.2 信息：CLSID 检查严格度

**位置**：`libs/cfb/src/compound_file_header.cpp:179-182`

检查 bytes 8-23 必须全零。MS-CFB §2.2 要求 "MUST be set to all zeroes"，符合规范。部分第三方工具可能写入非零值，若遇兼容性问题可降级为 warning。

### 1.3 其余正确性验证结论

逐层对照规范后，其余实现均正确：

| 模块 | 验证项 | 结论 |
|---|---|---|
| Header | 签名、字节序、sector shift、mini sector shift、DIFAT 起始 | ✅ |
| DIFAT/FAT | 链读取、容量验证、扇区角色标记与冲突检测、环检测 | ✅ |
| Directory | 128 字节条目解析、UTF-16 名称、红黑树结构、入度计数、DFS 环检测、可达性 | ✅ |
| MiniFAT | 链读取、角色冲突、环检测、越界引用 | ✅ |
| Stream Reader | 链长度验证、扇区共享检测、metadata 冲突检测、lazy open | ✅ |

---

## 二、架构建议

### 2.1 缺失：目录树遍历 API

**参考**：`compoundfilereader` 的 `EnumFiles()` 回调式递归遍历。

**现状**：`DirectoryIndex.entries` 仅为平铺数组，外部需自行实现树遍历。

**建议**：在 `DirectoryIndex` 或独立 API 中提供树遍历 + 按路径查找能力。

### 2.2 缺失：流名称查找

**参考**：`office_parser` 的 `findStream(name)`。

**现状**：`CfbStreamReader::open()` 只接受 `directory_entry_id`，调用方需自行遍历 entries 匹配名称。

**建议**：提供 `find_stream(name)` 或 `open_stream(name)` 接口。

### 2.3 缺失：OLE 属性集支持

**参考**：`compoundfilereader` 的 `PropertySet` / `PropertySetStream`。

**现状**：`\005SummaryInformation` 和 `\005DocumentSummaryInformation` 流解析未实现。

**建议**：新增属性集模块，为格式检测和元数据提取提供基础。

### 2.4 缺失：格式检测

**参考**：`office_parser` 的 `DetectCompoundMediaType()`。

**现状**：无容器级格式检测。

**建议**：与 `docs/spec/office-runtime.md` Stage 3 检测对齐，扫描特征流识别 DOC/XLS/PPT。

### 2.5 缺失：子范围读取器（借鉴 docwire OLEImageReader）

**参考**：`docwire/src/wv2/src/olestream.h:116-173` 的 `OLEImageReader`。

**设计**：包装一个 `OLEStreamReader`，限制只读取 `[start, limit)` 区间，提供独立的 `seek`/`tell`/`read`/`size`，通过 `push()`/`pop()` 不修改底层 reader 状态。多个子范围读取器可安全共享同一底层流。

**适用场景**：Word 文档中嵌入的图像、Excel 中的子记录、PPT 中的幻灯片数据——这些场景需要从一个流中安全地读取子范围，且常有多个解析器交替读取同一流的不同区间。

**建议**：在 `CfbStreamReader` 之上提供 `SubStreamReader`，接受 `[start, limit)` 范围约束，读取时自动边界检查。

### 2.6 缺失：push/pop 位置栈（借鉴 docwire OLEStream）

**参考**：`docwire/src/wv2/src/olestream.h:64-68`。

**设计**：`OLEStream` 基类提供 `push()`/`pop()` 保存/恢复读取位置。嵌套解析时非常有用——先读头部信息确定结构，再回到原位读取具体数据。

**建议**：在 `CfbStreamReader` 中增加 `push()`/`pop()` 方法，实现简单（内部维护 `std::stack<uint64_t>`）。

---

## 三、性能优化

### 3.1 预计算流扇区链

**位置**：`libs/cfb/src/cfb_stream_reader.cpp:561`

**问题**：`read()` 每次跨扇区边界时通过 FAT 条目查下一扇区，大流时开销累积。

**优化**：在 `open()` 时预计算并缓存完整扇区链，避免运行时逐跳 FAT 查找。

**参考**：`docwire/src/thread_safe_ole_storage.cpp:124-180` 的 `getStreamPositions()` 实现了类似策略——在 `createStreamReader()` 时一次性遍历 FAT/MiniFAT 链，将每个扇区的**文件偏移**预计算到 `m_file_positions` 数组。读取时直接按索引取偏移。

**注意**：docwire 的预计算是 eager 的（构造时遍历整个链），且该实现有 bug——mini 流跨物理扇区时 `mini_sector_location` 的链式追踪未正确重置（`thread_safe_ole_storage.cpp:138-152`）。xmole2 可在保留 lazy 语义的前提下折中为：仅在 `open()` 时预计算扇区 ID 链（而非文件偏移），运行时仍按需读取。

### 3.2 Mini 流偏移预计算

**位置**：`libs/cfb/src/cfb_stream_reader.cpp:512-526`

**问题**：每次读取都重新计算 mini-sector → 物理扇区的映射（除法 + 取模）。

**优化**：`open()` 时预计算所有 mini-sector 对应的物理扇区偏移。

### 3.3 消除重复预算计算

**位置**：各层 `validate_memory_budget()` 均重新累加所有下层的内存占用。

**优化**：在 `CfbStreamReader::open()` 中一次性计算总预算，避免逐层冗余累加。

---

## 四、代码重复

> 扫描范围: `libs/` 下所有 `.cpp` 和 `.hpp`，共 19 个源文件 + 29 个头文件
> 扫描结果: 12 组重复函数，分布在 13 个源文件的匿名命名空间中

---

### 可提取的通用函数（签名 + 实现完全一致）

| 函数                   | 重复数 | 所在文件                                                     | 建议位置                              |
| ---------------------- | ------ | ------------------------------------------------------------ | ------------------------------------- |
| `checked_multiply`     | 5      | `cfb_stream_reader.cpp`, `directory_index.cpp`, `mini_stream_allocation_table.cpp`, `sector_allocation_table.cpp`, `sector_reader_internal.cpp` | `cfb/src/cfb_internal_utils.hpp`      |
| `read_u32`             | 4      | `compound_file_header.cpp`, `directory_index.cpp`, `mini_stream_allocation_table.cpp`, `sector_allocation_table.cpp` | `cfb/src/cfb_internal_utils.hpp`      |
| `cancelled_error`      | 7      | CFB 5 文件 + IO 2 文件（`file_io.cpp`, `source_lease.cpp`）  | 各 module 私有 utility（domain 不同） |
| `resource_error`       | 9      | CFB 5 文件 + IO 2 文件 + ZIP 2 文件                          | 各 module 私有 utility（domain 不同） |
| `checked_add`          | 2      | `cfb_stream_reader.cpp`, `mini_stream_allocation_table.cpp`  | `cfb/src/cfb_internal_utils.hpp`      |
| `ceiling_divide`       | 2      | `cfb_stream_reader.cpp`, `mini_stream_allocation_table.cpp`  | `cfb/src/cfb_internal_utils.hpp`      |
| `read_u16`             | 2      | `compound_file_header.cpp`, `directory_index.cpp`            | `cfb/src/cfb_internal_utils.hpp`      |
| `bytes_are_zero`       | 2      | `compound_file_header.cpp`, `directory_index.cpp`            | `cfb/src/cfb_internal_utils.hpp`      |
| `out_of_range`         | 2      | `mini_stream_allocation_table.cpp`, `sector_allocation_table.cpp` | `cfb/src/cfb_internal_utils.hpp`      |
| `validate_source_size` | 2      | `file_io.cpp`, `source_lease.cpp`（均为 IO module）          | `io/src/io_internal_utils.hpp`        |

---

### 语义等价但有细微差异的函数

| 函数                       | 变体数 | 差异                                                         |
| -------------------------- | ------ | ------------------------------------------------------------ |
| `add_memory`               | 2      | 变体 A（`cfb_stream_reader` / `mini_stream_allocation_table`）调用 `checked_add`；变体 B（`directory_index` / `sector_allocation_table`）内联溢出检查。应统一。 |
| `map_io_error`             | 2      | `compound_file_header` 版多映射 `UnexpectedEndOfFile→InvalidHeader`。可用组合形式统一。 |
| `cancelled_error` 消息文本 | 7      | 每个文件消息不同（如 `"CFB stream operation cancelled"` vs `"CFB directory read cancelled"`）。可加 message 参数统一工厂。 |

---

### 按模块统计重复密度

```
CFB module:  15 处重复分布在 7 个源文件 ← 最严重
IO module:    3 处重复分布在 2 个源文件
ZIP module:   1 处重复分布在 2 个源文件
BASE module:  0 处重复
```

---

### 建议行动

#### P0（高收益，低风险）

为 **CFB** 创建 `libs/cfb/src/cfb_internal_utils.hpp`，提取：

- `checked_add` / `checked_multiply` / `ceiling_divide`
- `read_u16` / `read_u32`
- `bytes_are_zero`
- `add_memory`（统一变体 A/B 为一种实现）
- `out_of_range`

预期消去 **~30%** 的 CFB 匿名命名空间重复代码。

#### P1

为 **IO** 创建 `libs/io/src/io_internal_utils.hpp`，提取：

- `validate_source_size`

#### P2

统一 CFB 中 `map_io_error` / `wrap_io_error`（差异仅在 `UnexpectedEndOfFile` 映射，可用参数化处理）。

以下工具函数在 4 个 .cpp 文件中重复定义：

| 函数 | 出现文件 |
|---|---|
| `read_u16` / `read_u32` / `read_u64` | compound_file_header.cpp, sector_allocation_table.cpp, directory_index.cpp, mini_stream_allocation_table.cpp |
| `checked_multiply` / `checked_add` | sector_allocation_table.cpp, directory_index.cpp, mini_stream_allocation_table.cpp, cfb_stream_reader.cpp |
| `add_memory` | sector_allocation_table.cpp, directory_index.cpp, mini_stream_allocation_table.cpp, cfb_stream_reader.cpp |
| `cancelled_error` | sector_allocation_table.cpp, directory_index.cpp, mini_stream_allocation_table.cpp, cfb_stream_reader.cpp |
| `map_io_error` / `wrap_io_error` | compound_file_header.cpp, sector_reader_internal.cpp, cfb_stream_reader.cpp |
| `SectorRole` enum（值不同） | sector_allocation_table.cpp, mini_stream_allocation_table.cpp |

**建议**：提取到 `libs/cfb/src/internal/` 共享头文件，避免重复代码和定义不一致。

---

## 五、功能差距矩阵

| 功能 | xmole2::cfb | compoundfilereader | office_parser | ole-compound-pp | docwire |
|---|---|---|---|---|---|---|
| 分层架构 | ✅ 清晰分层 | ❌ 单体类 | ❌ 单体类 | ✅ 模块化 | ✅ 基于 GSF |
| Result 错误处理 | ✅ | ❌ 异常 | ❌ bool | ❌ 异常 | ❌ 异常 |
| 资源预算 | ✅ | ❌ | ❌ | ❌ | ❌ |
| 取消支持 | ✅ | ❌ | ❌ | ❌ | ❌ |
| Lazy 读取 | ✅ | ❌ 全量 | ❌ 全量 | ❌ | ❌ |
| UTF-16 验证 | ✅ | ❌ | ❌ | ❌ | ❌ |
| 子范围读取器 | ❌ | ❌ | ❌ | ❌ | ✅ OLEImageReader |
| push/pop 位置栈 | ❌ | ❌ | ❌ | ❌ | ✅ OLEStream |
| 预计算扇区位置 | ❌ lazy 逐跳 | ❌ | ❌ | ❌ | ✅ getStreamPositions |
| 类型化读取方法 | ❌ 仅 raw read | ❌ | ❌ | ❌ | ✅ readU8/16/32 |
| 写入接口骨架 | ❌ | ❌ | ❌ | ❌ | ✅ OLEStreamWriter |
| 目录树遍历 | ❌ | ✅ EnumFiles | ❌ | ❌ | ✅ listDirectory |
| 流名称查找 | ❌ | ❌ | ✅ findStream | ❌ | ❌ |
| 属性集解析 | ❌ | ✅ PropertySet | ❌ | ❌ | ✅ oshared |
| 格式检测 | ❌ | ❌ | ✅ DetectMediaType | ❌ | ❌ |
| 线程安全 | ❌ | ❌ | ❌ | ❌ | ✅ |

---

## 六、总结

- **正确性**：除 minor version 检查的 bug 外，其余实现正确且比所有参考实现更严格，覆盖 MS-CFB 全部关键约束。
- **架构**：分层设计优于所有参考实现。docwire 的 `OLEImageReader`（子范围读取器）和 `push/pop` 位置栈值得引入。目录树遍历和流名称查找仍为缺失项。
- **性能**：当前 lazy 设计已足够。docwire 的预计算扇区位置思路可参考，但其实现有 bug，不应直接复制。主要优化点仍是预计算扇区链和消除重复预算计算。
- **代码质量**：工具函数重复是最大问题，建议提取到 `internal/` 共享头文件。
- **不采纳项**：docwire 的异常处理、`pimpl` 模式、`data_stream` 抽象均不如 xmole2 当前设计。

> docwire 值得借鉴的点
> 1. OLEImageReader：带边界检查的子范围读取器（olestream.h:116-173）
> 这是 docwire 最有价值的设计。它包装一个 OLEStreamReader，限制只读取 [start, limit) 区间，提供独立的 seek/tell/read/size，且不修改底层 reader 的状态（通过 push/pop 保存恢复）。
> 适用场景：Word 文档中嵌入的图像数据、Excel 中的子记录、PPT 中的幻灯片数据——这些场景需要从一个流中安全地读取子范围。当前 xmole2 的 CfbStreamReader 只能从流起始顺序读取，没有子范围限制能力。
> 建议：在 CfbStreamReader 之上或内部提供 SubStreamReader，接受 [start, limit) 范围约束，读取时自动边界检查。
> 2. 预计算流扇区位置（getStreamPositions）
> docwire 在 createStreamReader() 时遍历 FAT/MiniFAT 链，将每个扇区的文件偏移预计算到 m_file_positions 数组。读取时直接按索引取偏移。
> 但该实现有 bug（thread_safe_ole_storage.cpp:138-152）：跨物理扇区时 mini_sector_location 的链式追踪逻辑在 sector_index 变化时未正确重置，可能导致错误偏移。
> 对 xmole2 的参考价值有限：xmole2 的 lazy 方式在只读部分流时更优。若需优化，可折中为在 open() 时预计算扇区 ID 链（而非文件偏移），避免运行时逐跳。
> 3. OLEImageReader：带边界检查的子范围读取器（olestream.h:116-173）
> 这是 docwire 最有价值的设计。它包装一个 OLEStreamReader，限制只读 [start, limit) 区间，通过 push()/pop() 不修改底层 reader 状态。
> 适用场景：Word 文档中嵌入的图像数据、Excel 中的子记录、PPT 中的幻灯片数据——这些场景需要从一个流中安全地读取子范围。当前 xmole2 的 CfbStreamReader 只能从流起始顺序读取，没有子范围限制能力。
> 建议：在 CfbStreamReader 之上或内部提供 SubStreamReader，接受 [start, limit) 范围约束，读取时自动边界检查。
> 4. push()/pop() 位置栈（olestream.h:64-68）
> OLEStream 基类提供 push()/pop() 保存/恢复读取位置。嵌套解析时有用（如先读头部信息再回到原位继续解析）。实现简单，xmole2 可考虑在 CfbStreamReader 中增加。
> 5. OLEStreamWriter 接口骨架（olestream.h:176-239）
> wv2 定义了类型化写入接口（writeU8/writeU16/writeU32），为未来写入支持预留了接口设计。当前 xmole2 只读，但接口骨架设计值得参考。
> 6. OLEImageReader 的 push/pop 模式
> OLEImageReader::read() 在每次读取时 push() 保存底层 reader 位置，读取完成后 pop() 恢复。这使得多个子范围读取器可以安全共享同一个底层流。xmole2 的 CfbStreamReader 没有类似机制。
