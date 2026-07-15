# Office Runtime 与格式检测规范

状态：Normative Baseline  
适用 target：`xmole2::office-runtime`

## 1. 边界与依赖

`office-runtime` 只依赖 `xmole2::base` 和 `xmole2::io`，负责显式 codec registry、Office 格式检测以及打开前的 codec 选择。它不解析产品内容模型，不直接依赖 ZIP、CFB、OPC、XML、产品 codec 或第三方 MIME 检测对象。

检测器和 codec 必须显式注册，禁止依赖全局静态初始化。聚合组件可以构造带内置 codec 的 runtime，但裸 `office-runtime` 不得隐式拉入 Words、Cells 或 Slides。

runtime 只调用 xmole2 自有的窄 detector 接口。需要 ZIP、CFB、OPC 或产品知识的 detector 由组合层/codec provider 实现并注入；这些依赖留在 provider 一侧，禁止为了容器 probe 扩大 `office-runtime` 的直接依赖。

## 2. 证据管线

格式检测采用多阶段证据管线。每个检测器独立产生候选格式、置信度与证据；后续阶段可以补充或推翻较弱证据，但不得删除冲突证据。

推荐阶段为：

1. 调用者 hint 与扩展名：只作为低置信度提示；
2. 固定 magic/signature：识别 ZIP、CFB/OLE2 等物理容器；
3. 容器标记：识别 OOXML Content Types/根关系、加密 CFB stream、DOC/XLS/PPT 特征 stream；
4. codec probe：由已注册 codec 执行受预算控制的最小结构确认，不建立完整领域模型。

阶段不是硬编码的唯一实现顺序；registry 可以加入窄职责 detector，但每个 detector 必须声明稳定 ID、可产生的证据种类和最大置信度。检测不得执行宏、ActiveX、外部进程、网络访问或任意路径读取。

## 3. 置信度与裁决

`DetectionConfidence` 是有序值：`None < Low < Medium < High < VeryHigh < Highest`。各等级语义为：

- `Low`：扩展名、调用者 hint 或单一弱启发式；
- `Medium`：通用容器特征或多项一致的弱证据；
- `High`：格式 magic 与结构标记一致；
- `VeryHigh`：存在足以区分具体 Office 格式的容器身份；
- `Highest`：已注册 codec 的最小结构 probe 成功，且未发现等强冲突。

扩展名和调用者 hint 的置信度上限为 `Low`，不得覆盖相反的 magic/container 证据。最终选择最高置信度候选；同一格式的独立证据可以合并，但不得仅以证据数量把弱证据提升为强证据。

若两个不同格式具有相同最高置信度，结果必须为 `AmbiguousFormat` 并保留双方证据，禁止用 detector 注册顺序、文件扩展名或未记录的启发式静默裁决。稳定结果不得依赖 hash 容器迭代顺序。

## 4. DetectionReport

检测返回 `DetectionReport`，至少包含：

- 选中的候选格式（若可唯一裁决）；
- 最终置信度；
- 按确定性顺序排列的检测证据；
- 候选格式对应 codec 是否已注册；
- 是否为加密容器以及继续识别是否需要 credential；
- 检测期间产生的结构化诊断。

证据至少记录 detector ID、证据种类、候选格式和置信度。公开报告使用 xmole2 自有值类型与标准库类型，不暴露第三方 MIME/容器对象。

格式检测与 codec 可用性是两个步骤：能可靠识别但未注册 codec 时返回 `CodecUnavailable`，不能降级为 `UnsupportedFormat`。加密 OOXML 识别到 CFB 中的 `EncryptionInfo` 与 `EncryptedPackage` 后返回 `EncryptedCredentialRequired`；解密前不得猜测 Word/Cells/Slides 产品类型。

损坏输入必须区分：没有任何受支持格式证据时为 `UnsupportedFormat`；已具有强格式证据但关键结构损坏时为 `CorruptInput`。

## 5. OperationContext 与所有权

检测和 probe 是可能长时间运行的操作，必须贯通同一 `OperationContext`：

- 所有读取经 `SourceLease`/`ByteSource`，共享输入、内存、entry/stream、诊断和递归预算；
- 每个 detector 和容器 probe 都检查取消；
- progress phase 使用稳定 detector ID；
- 可恢复的扩展名冲突、未知标记和降级证据进入 `DiagnosticSink`；
- detector 不得为提高置信度而隐式放宽预算。

打开入口可以为 path、owning buffer、`unique_ptr<ByteSource>` 和 `shared_ptr<ByteSource>` 提供便捷适配，但最终必须取得稳定 `SourceLease`。不得采用“左值引用 + 空 deleter shared_ptr”的隐式借用：文档、archive 和 reader 可逃逸调用栈，该模式会产生悬垂 source。

未来若确需 borrowed source，必须使用显式 borrowed 类型并携带可共享 lifetime guard；无 guard 的 `ByteSource&` 只能用于保证不逃逸的同步 probe，不得进入文档或 reader 状态。

## 6. Contract

OfficeRuntime contract 必须覆盖：

- 扩展名与 magic 一致/冲突；
- ZIP、CFB、加密 OOXML 和六种目标 Office 格式的分层证据；
- 多 detector 合并、较强证据覆盖较弱证据、同分冲突返回 `AmbiguousFormat`；
- 结果不依赖注册顺序；
- 已识别但 codec 缺失、无格式证据、强证据后的损坏结构；
- 预算、取消、故障 Source、诊断收集和 deterministic report；
- detector/codec registry 的显式注入与 mock 实现。
