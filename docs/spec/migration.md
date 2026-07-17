# 迁移与实施规范

## 1. 旧实现处置

旧源码和旧 API 测试在需要查阅的开发机上冻结于 `deprecated/legacy-v0/`，退出默认 CMake 和 CI。整个 `deprecated/` 是不提交 Git 的本地隔离目录，干净 clone 可以不存在。旧内容只可作为算法、第三方库用法和性能经验参考，禁止沿用其模块边界或公开 API。

旧 fixture 移入不提交 Git 的本地 `testdata/`。其受版本控制的目录、来源、哈希与隐私状态记录位于 `docs/fixtures/catalog.md`。经规范/互操作确认的行为转写成新 contract test；旧实现不是行为 oracle。

rdocx、Aspose.Cells FOSS、Aspose.Slides FOSS 以及 OLE2/CFB 研究项目可按 `docs/reference-snapshots.md` 恢复到本地 `references/`，只读且不参与构建。整个目录不提交 Git。复用算法前必须确认精确上游、commit、文件级许可证并记录 provenance；来源或许可证未知、或许可证与 xmole2 不兼容时禁止复制或改写实现。

M1 的 `xmole2::cfb` 必须以 `docs/office-standard/ms-cfb.pdf` 所存 `[MS-CFB]` v20240423 为格式基线，日常检索使用 `docs/office-standard/ms-cfb/ms-cfb_md_full.md`，表格与图形复核使用对应逐页 OCR JSON，具体规则见 `office-standard.md`。实现可以交叉研究本地 `ole-compound-pp`、`office_parser`、DocWire 和 Microsoft CompoundFileReader 快照，重点比较 CFB header、DIFAT/FAT、MiniFAT、directory、stream chain 与 OLE property set 的处理，但不得沿用参考项目的模块边界、公共 API、整文件物化、裸指针、异常或平台 OLE handle 模型。

## 2. 实施阶段

状态只描述受版本控制实现与默认 contract 的当前证据，不以空 target 或本地 fixture 代替完成。最后核对：2026-07-17。

### M0：工程基线（已完成）

- [x] 建立规范 target、public/private header 和导出规则。
- [x] 实现 base 的 Error/Result、OperationContext、ResourceBudget、诊断收集、取消和默认拒绝的外部资源解析 port。
- [x] 加入依赖方向与 public-header 检查；默认 CTest 注册 `xmole2.architecture.public_headers`。

### M1：I/O 与容器（已完成）

- [x] SourceLease、ByteSource/ByteSink、原子保存和临时存储，具有 base/io contract。
- [x] ZIP entry 索引与流式读取，具有 `xmole2.zip.contract`。
- [x] `xmole2::cfb` 已建立独立实现 target 与首个读取切片：受预算、可取消地读取并校验 CFB v3/v4 header，具有 `xmole2.cfb.contract`。
- [x] CFB DIFAT/FAT sector table 已实现有界读取、角色校验、循环与越界检测，并具有 `xmole2.cfb.sector_table.contract`。
- [x] CFB directory sector chain 与 128-byte entry index 已实现 UTF-16、字段兼容、引用图、循环、color 约束、Unicode 17.0.0 simple-uppercase sibling 排序/唯一性、预算与故障防御，并具有 `xmole2.cfb.directory.contract`。
- [x] CFB MiniFAT table 与 root mini-stream sector mapping 已实现声明长度、角色冲突、循环、越界、预算与故障防御，并具有 `xmole2.cfb.mini_stream.contract`。
- [x] CFB 惰性 regular/mini stream chain reader 已实现按声明长度的分块读取、源生命周期保持、chain 共享/边界验证与 payload 故障传播，具有 `xmole2.cfb.stream_reader.contract`。
- [x] 已交付的 base/io/zip 路径贯通资源预算、取消与故障注入。
- [x] CFB 路径已提供贯通 header、FAT/DIFAT、directory、MiniFAT/root mapping 与 regular/mini payload reader 的资源预算、取消和故障注入证据。

### M2：OPC 与 lossless XML（未开始）

- PartStore、Content Types、Relationships、URI。
- token/source span、patch writer、dirty tracking。
- Transitional/Strict profile 与未知扩展保留。

### M3：三格式垂直切片（未开始）

- DOCX/XLSX/PPTX codec 与三个最小领域模型并行落地。
- OfficeRuntime、格式检测、报告、真实 fixture 往返。
- 通过 `testing.md` 的全部垂直切片标准。

### M4：领域深化（未开始）

- 在边界验证后继续 Words 迁移。
- Cells 与 Slides 以相同 contract 扩展。
- 逐步加入 DOC/XLS/PPT、加密、签名检测。

### M5：可选能力（未开始）

- cells-calc、三个独立 layout/render、graphics，以及 PDF/HTML/SVG/raster exporters。
- 独立异步 integration target、动态 C ABI plugin、语言绑定和 1.0 ABI 决策。

## 3. 每项功能的完成定义

代码、公共契约测试、错误/诊断、资源预算、保真行为、文档和必要 ADR 必须同时完成。只有解析无保存策略、只有 happy-path 无 negative test、或依赖旧实现输出的功能均不算完成。
