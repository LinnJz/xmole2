# Office 官方标准资料规范

状态：Normative Baseline
适用范围：Office 文件格式、容器、加密、属性集及相关协议的实现、测试与评审

## 1. 资料职责与优先级

`docs/office-standard/` 保存实现所依据的官方格式标准及其本地可检索派生产物。官方标准约束文件格式事实；已接受 ADR 与 `docs/spec/` 约束 xmole2 的架构、公共 API、安全、资源和工程策略。两者职责不同，不得用参考实现、旧代码或当前实现覆盖任一基线。

发生冲突时按以下规则处理：

1. 字段布局、取值、版本、链结构、互操作行为以及标准中的 MUST/SHOULD 等格式事实，以对应版本的官方 PDF 为最终依据。
2. target 边界、所有权、错误、预算、取消、懒加载、保真和公开契约，以已接受 ADR 与最新 `docs/spec/` 为依据。
3. xmole2 spec 或测试与官方标准冲突时必须暂停实现，先修正规范、ADR 或测试；禁止以兼容当前代码为由放宽标准要求。
4. `references/`、`deprecated/`、第三方项目输出和经验性 fixture 只能辅助理解，不能成为格式行为 oracle。

## 2. 目录与派生产物

每份标准采用以下布局：

```text
docs/office-standard/<standard>.pdf                 # 官方原始文档
docs/office-standard/<standard>/<standard>_md_full.md
docs/office-standard/<standard>/<standard>_ocr_page<N>.json
```

- PDF 是版本、版权、页码、表格、图形和规范措辞的原始凭据。
- Markdown 是日常搜索和顺序阅读的首选入口，但属于从 PDF 转换的派生产物。
- OCR JSON 保留逐页文字、表格、图形文字和版面坐标，用于 Markdown 丢失结构或图像信息时复核。
- Markdown 与 OCR JSON 不得独立改变标准含义；内容不一致、缺失或识别可疑时必须回查 PDF。

派生产物应与 PDF 来自同一版本。更新标准时必须同时更新 PDF、Markdown、OCR JSON 和本规范记录的版本信息，不能混用不同发布日期的内容。

## 3. 开发与评审流程

涉及某一 Office 格式或底层容器时：

1. 先读取对应模块 spec 与本规范，再定位 `docs/office-standard/` 中匹配的标准。
2. 优先在完整 Markdown 中检索术语、章节和规范关键词；涉及表格、图、字节布局或转换结果可疑时读取对应页 OCR JSON，并以 PDF 页面终审。
3. contract/negative test 的行为必须能够追溯到具体标准版本与章节，或明确标注为 xmole2 的安全收紧策略。
4. 标准未规定的 API、资源策略和恢复行为不得反向写成格式事实；由 xmole2 spec 或 ADR 单独决策。
5. 标准中的示例用于理解和构造独立 synthetic fixture，不能把示例输出或 OCR 结果直接当作未经验证的测试 oracle。

## 4. 当前 CFB 基线

`xmole2::cfb` 当前使用 Microsoft `[MS-CFB]` v20240423（Revision 12.0，2024-04-23）作为格式标准：

- 原始文档：`docs/office-standard/ms-cfb.pdf`
- 原始文档 SHA-256：`9D0D61E34495347EE32F3DE5B06F2D59953CC60607EA72605D4162D21A34863F`
- 完整检索文本：`docs/office-standard/ms-cfb/ms-cfb_md_full.md`
- 图表与逐页 OCR：`docs/office-standard/ms-cfb/ms-cfb_ocr_page<N>.json`

后续 CFB header、DIFAT/FAT、MiniFAT、directory、stream chain、sector role、size limit 和 corruption validation 的实现与评审必须以该版本为准。`docs/reference-snapshots.md` 中的 OLE2/CFB 项目只用于交叉研究实现经验。

CFB directory 名称比较另使用 Unicode Character Database 17.0.0 作为 `[MS-CFB]` §2.6.4 允许的更新 Unicode supporting baseline：

- 官方输入：<https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt>
- 输入 SHA-256：`2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C`
- 字段语义：Unicode Standard Annex #44 Revision 36 所定义的 `Simple_Uppercase_Mapping`（`UnicodeData.txt` 第 12 字段），结果为单一 code point
- 使用边界：只生成 BMP source 的 simple-uppercase 映射；代理 UTF-16 code unit 按 `[MS-CFB]` 保持恒等。生成表保留 Unicode License V3，不把完整 case folding、locale 规则或平台字符 API 当作 CFB 比较语义

更新 Unicode supporting baseline 时，必须同时更新输入哈希、生成表、许可证、排序/重复名 contract 和至少一个可追溯互操作 fixture；不能静默采用宿主平台的 Unicode 版本。

## 5. OOXML 基线（预备）

`xmole2` 后续 M2/M3 阶段（OPC、DOCX/XLSX/PPTX codec）使用 **ECMA-376 4th Edition（December 2012）** 作为 Office Open XML 格式标准，涵盖以下 4 个部分。当前 M1 CFB 工作不依赖此基线；以下资料仅作预备，待进入对应模块时以此版本为准。

### 5.1 Part 1：Fundamentals and Markup Language Reference

- 原始文档：`docs/office-standard/ecma-ooxml-p1-Fundamentals And Markup Language Reference.pdf`
- 原始文档 SHA-256：`4A9D481C74DAEAB4068408DAE354E8B6F808771A1B9810EA69482344CE6CCD65`
- 完整检索文本：`docs/office-standard/ecma-ooxml-p1/ecma-ooxml-p1_md_full.md`
- 图表与逐页 OCR：`docs/office-standard/ecma-ooxml-p1/ecma-ooxml-p1_ocr_page<N>.json`
- 补充材料：`OfficeOpenXML-DrawingMLGeometries/`、`OfficeOpenXML-RELAXNG-Strict/`、`OfficeOpenXML-SpreadsheetMLStyles/`、`OfficeOpenXML-WordprocessingMLArtBorders/`、`OfficeOpenXML-XMLSchema-Strict/`
- OCR 覆盖范围：前 100 页（page 0–99），包含 Foreword、Scope、Conformance、Terms、Notational Conventions 以及 WordprocessingML（§11）、SpreadsheetML（§12 至 §12.3.13）等核心章节。后续章节（PresentationML、DrawingML、Shared Parts 等）尚未 OCR，待实际需要时补充。

Part 1 定义了 OOXML 的总体框架、WordprocessingML、SpreadsheetML、PresentationML、DrawingML、VML、Shared ML 以及文档模板、主控文档、邮件合并等特性。所有 FIXML（公式）和 OOXML 严格/过渡命名空间也在 Part 1 中规定。

### 5.2 Part 2：Open Packaging Conventions

- 原始文档：`docs/office-standard/ecma-ooxml-p2-Open Packaging Conventions.pdf`
- 原始文档 SHA-256：`1256D9D704AF65B8DABFCF9E67770C0294D256387E1945DDBA69FB453D174F55`
- 完整检索文本：`docs/office-standard/ecma-ooxml-p2/ecma-ooxml-p2_md_full.md`
- 图表与逐页 OCR：`docs/office-standard/ecma-ooxml-p2/ecma-ooxml-p2_ocr_page<N>.json`
- 补充材料：`OpenPackagingConventions-RELAXNG/`、`OpenPackagingConventions-XMLSchema/`
- OCR 覆盖范围：前 100 页（page 0–99），包含 Scope、Package Model、Parts、Content Types、Relationships、Digital Signatures、Encryption、Physical Package（ZIP）等完整 OPC 核心规范。

Part 2 定义了 OPC 包模型：ZIP 物理包、部件（Part）、内容类型（Content Type）、关系（Relationship）、包级别数字签名和数据加密。OPC 是 DOCX/XLSX/PPTX 的基础容器层。

### 5.3 Part 3：Markup Compatibility and Extensibility

- 原始文档：`docs/office-standard/ecma-ooxml-p3-Markup Compatibility and Extensibility.pdf`
- 原始文档 SHA-256：`E4E58BE15925162BAEB8FD3AAE51381C8C6995E421C4E460B950C057861A11C6`
- 完整检索文本：`docs/office-standard/ecma-ooxml-p3/ecma-ooxml-p3_md_full.md`
- 图表与逐页 OCR：`docs/office-standard/ecma-ooxml-p3/ecma-ooxml-p3_ocr_page<N>.json`
- OCR 覆盖范围：44 页（page 0–43），基本完整。

Part 3 定义了标记兼容性机制：Ignorable、ProcessContent、MustUnderstand、AlternateContent、Fallback 等属性与元素，用于支持严格/过渡命名空间的向前兼容和扩展。

### 5.4 Part 4：Transitional Migration Features

- 原始文档：`docs/office-standard/ecma-ooxml-p4-Transitional Migration Features.pdf`
- 原始文档 SHA-256：`46A34FD930801E69AD5150996CA7671C8137EFA4FD4BCA3DCD30829594F65E02`
- 完整检索文本：`docs/office-standard/ecma-ooxml-p4/ecma-ooxml-p4_md_full.md`
- 图表与逐页 OCR：`docs/office-standard/ecma-ooxml-p4/ecma-ooxml-p4_ocr_page<N>.json`
- 补充材料：`OfficeOpenXML-RELAXNG-Transitional/`、`OfficeOpenXML-XMLSchema-Transitional/`
- OCR 覆盖范围：前 100 页（page 0–99），包含 Additional Shared Parts、WordprocessingML 过渡特性（§9）、SpreadsheetML 过渡特性（§10）、PresentationML 过渡特性（§11）等。

Part 4 定义了从旧版 Office 二进制格式向 OOXML 过渡所需的特性：VML Drawing、兼容性设置、过渡命名空间元素与属性。实现导入旧版文档时需参考 Part 4。

### 5.5 使用原则

进入 M2/M3 的 OPC 或 DOCX/XLSX/PPTX codec 开发时：

1. 包容器行为（ZIP items、部件名、内容类型、关系图）以 Part 2 为最终依据。
2. 标记兼容性（AlternateContent、Ignorable 等）以 Part 3 为最终依据。
3. 各文档类型的 XML 元素、属性、约束以 Part 1 为最终依据；保存与加载旧版特性时辅以 Part 4。
4. Part 1 未 OCR 的章节（PresentationML §13、DrawingML §14、Shared ML §15 等）在进入对应模块时需补充 OCR 并更新本规范。
5. 标准未规定的行为（错误恢复、资源策略、安全限制）由 xmole2 spec 或 ADR 单独决策，不得反向写入标准事实。
