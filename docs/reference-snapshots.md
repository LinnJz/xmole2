# 本地参考快照目录

`references/` 整体不提交 Git，也不参与配置、构建、测试或安装。需要研究时按本表恢复到本地；参考项目用于理解格式、算法选择和互操作经验，不能覆盖 `docs/spec/`、已接受 ADR、格式标准或 xmole2 contract。

本地快照若没有 `.git` 元数据或不能确认 commit，只允许阅读。复制或改写任何实现前，必须先补齐精确上游、commit、文件级许可证和 provenance 记录；参考输出不是规范 oracle。

## OOXML 与产品 codec 参考

| 本地目录 | 版本/来源 | 许可证 | 研究范围与状态 |
|---|---|---|---|
| `references/rdocx/` | rdocx 0.1.2，<https://github.com/tensorbee/rdocx> | MIT OR Apache-2.0 | 研究 DOCX/WordprocessingML 算法和性能经验；可按 release/tag 恢复，复用前记录 provenance |
| `references/aspose-cells-foss/` | 26.4.0 目录快照，<https://github.com/aspose-cells-foss/Aspose.Cells-FOSS-for-Cpp> | MIT | 研究 XLSX/SpreadsheetML 能力覆盖；具体 commit 待补，当前只读 |
| `references/aspose-slides-foss/` | main 目录快照，<https://github.com/aspose-slides-foss/Aspose.Slides-FOSS-for-Cpp> | MIT | 研究 PPTX/PresentationML 能力覆盖；具体 commit 待补，当前只读 |

## OLE2/CFB 与旧版 Office 参考

| 本地目录 | 版本/来源 | 许可证 | 适合研究的内容 | 限制与状态 |
|---|---|---|---|---|
| `references/ole-compound-pp/` | 本地源码目录快照；上游 URL、tag/commit 待补 | MIT；本地 LICENSE 标注 Copyright (c) 2018 Nacle | CFB header、sector addressing、MSAT/DIFAT 与 SAT/FAT 的基础术语和拆分方式 | 实现明显未完成，storage/stream 与扩展 MSAT 路径不能作为正确性依据；仅只读研究 |
| `references/office_parser/` | 本地四文件源码快照：`OfficeParser.*`、`OfficeCrypto.*`；上游与 commit 未知 | 未提供 LICENSE，许可证未知 | CFB header、DIFAT/FAT、MiniFAT、directory、普通/mini stream、SummaryInformation、加密 OOXML 外壳识别以及 DOC/XLS/PPT 元数据调用路径 | 整文件物化、简化 directory/stream size 和平台 OLE API 不符合 xmole2 契约；在来源和许可证补齐前禁止复制或改写代码 |
| `references/docwire/` | DocWire 2026.07.07 目录快照，<https://github.com/docwire/docwire>；具体 commit 待补 | AGPL-3.0-only OR commercial；内含 wv2 等组件，须逐文件复核 | OLE storage/stream adapter、旧 DOC 消费 CFB stream 的集成边界、并发包装和故障处理经验 | xmole2 当前许可证策略下不得复制 AGPL 实现；只可做概念和行为研究，任何复用须先完成法律/许可证评审 |
| `references/compoundfilereader/` | Microsoft CompoundFileReader 目录快照，<https://github.com/microsoft/compoundfilereader>；具体 commit 待补 | MIT | buffer-based CFB header、DIFAT/FAT、MiniFAT、directory tree、stream 分块读取及 OLE property set | public pointer/异常/整缓冲区模型不能沿用；循环、预算和恶意输入防御必须按 xmole2 spec 独立设计 |

## `xmole2::cfb` 使用边界

后续 `xmole2::cfb` 可以交叉研究上述四个 OLE2/CFB 快照，但必须遵守以下顺序：

1. 以 MS-CFB、MS-OLEPS、`docs/spec/` 和 CFB contract 为行为基线。
2. 对照多个参考实现理解 header、DIFAT/FAT、MiniFAT、directory 和 stream chain，不以单一实现输出作为 oracle。
3. 独立设计 `ByteSource`/`SourceLease` 接入、惰性 stream reader、稳定错误码、cause/native_code、预算、取消、溢出与 chain 循环检测。
4. 禁止沿用参考项目的公共 API、裸指针、全文件物化、平台专用 OLE handle 或异常错误模型。
5. 借鉴具体算法前，在变更记录中标明参考文件、上游 commit、许可证、改写范围和验证证据；来源或许可证未知时不得借鉴实现。

恢复任何快照后必须保留上游 LICENSE、NOTICE 和文件级版权声明。参考项目自带 fixture 仍属于未提交的本地 payload；若要形成 xmole2 contract，必须按 `docs/fixtures/catalog.md` 单独登记来源、许可证、哈希和预期行为。
