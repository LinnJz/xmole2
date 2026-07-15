# 迁移与实施规范

## 1. 旧实现处置

旧源码和旧 API 测试在需要查阅的开发机上冻结于 `deprecated/legacy-v0/`，退出默认 CMake 和 CI。整个 `deprecated/` 是不提交 Git 的本地隔离目录，干净 clone 可以不存在。旧内容只可作为算法、第三方库用法和性能经验参考，禁止沿用其模块边界或公开 API。

旧 fixture 移入不提交 Git 的本地 `testdata/`。其受版本控制的目录、来源、哈希与隐私状态记录位于 `docs/fixtures/catalog.md`。经规范/互操作确认的行为转写成新 contract test；旧实现不是行为 oracle。

rdocx、Aspose.Cells FOSS、Aspose.Slides FOSS 可按 `docs/reference-snapshots.md` 恢复到本地 `references/`，只读且不参与构建。整个目录不提交 Git。复用算法前必须确认许可证并记录 provenance。

## 2. 实施阶段

### M0：工程基线

- 建立规范 target、public/private header 和导出规则。
- 实现 base 的 Error/Result、OperationContext、ResourceBudget。
- 加入依赖方向与 public-header 检查。

### M1：I/O 与容器

- SourceLease、ByteSource/ByteSink、原子保存和临时存储。
- ZIP entry 索引/流式读取；CFB 独立 contract。
- 统一资源预算、取消与故障注入接缝。

### M2：OPC 与 lossless XML

- PartStore、Content Types、Relationships、URI。
- token/source span、patch writer、dirty tracking。
- Transitional/Strict profile 与未知扩展保留。

### M3：三格式垂直切片

- DOCX/XLSX/PPTX codec 与三个最小领域模型并行落地。
- OfficeRuntime、格式检测、报告、真实 fixture 往返。
- 通过 `testing.md` 的全部垂直切片标准。

### M4：领域深化

- 在边界验证后继续 Words 迁移。
- Cells 与 Slides 以相同 contract 扩展。
- 逐步加入 DOC/XLS/PPT、加密、签名检测。

### M5：可选能力

- cells-calc、三个独立 layout/render、graphics，以及 PDF/HTML/SVG/raster exporters。
- 独立异步 integration target、动态 C ABI plugin、语言绑定和 1.0 ABI 决策。

## 3. 每项功能的完成定义

代码、公共契约测试、错误/诊断、资源预算、保真行为、文档和必要 ADR 必须同时完成。只有解析无保存策略、只有 happy-path 无 negative test、或依赖旧实现输出的功能均不算完成。
