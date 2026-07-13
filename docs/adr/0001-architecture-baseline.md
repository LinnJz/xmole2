# ADR-0001：建立多产品分层架构基线

状态：Accepted  
日期：2026-07-15

## Context

旧实现从 rdocx 迁移而来，虽然具有 common/document/presentation/spreadsheet 目录，但仍构建为单一 target，公共 OOXML 层包含 Word 专属类型，且 Facade 直接暴露 OPC/原始 XML。继续扩展 Cells 与 Slides 会固化错误边界。

## Decision

采用 `docs/spec/` 定义的 monorepo、多 target 架构：统一基础设施与生命周期，不统一三类内容模型；领域模型与 codec 分离；OPC 与 CFB 并行；高保真、懒加载、受控句柄、显式 registry、统一错误信封与资源预算成为强制契约。

旧实现移入 `deprecated/legacy-v0`，三个外部项目移入 `references/`。先完成 DOCX/XLSX/PPTX 最小垂直切片，再深化 Words。

本决策的完整事实源记录于 `docs/meetings/2026-07-15-office-architecture-all-sessions.md`，逐项导航摘要位于 `docs/meetings/2026-07-15-office-architecture-grill.md`。`OfficeDocument` 的具体 C++ 表示仍属未决事项，必须在 office-runtime 实现前另立 ADR；EditSession 的事务内外句柄可见性模型也必须在事务实现前另立 ADR。

## Consequences

初期会产生工程重建成本，但 CMake 能验证依赖边界，大型 XLSX 与未知内容保留不再受旧模型限制。1.0 前允许源码 API 演进，不承诺 C++ ABI。
