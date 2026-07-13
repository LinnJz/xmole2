# 保真、保存与转换规范

## 1. 保真等级

| 场景 | 最低承诺 |
|---|---|
| 完全未修改的 Part/CFB stream | 字节级原样复制 |
| 已修改 XML Part 中未触碰的子树 | 尽可能通过 source span/patch writer 保持原始字节 |
| 必须重写的已理解子树 | 保持语义，确定性序列化 |
| 必须重写的含未知内容子树 | 保留未知属性、元素与相对顺序；不承诺字节一致 |
| 新建文档 | 相同模型、版本和选项产生确定性字节 |
| 跨格式转换 | 只承诺 ConversionReport 明确列出的能力 |

普通 DOM 不足以实现上述要求。XML 层必须保留 token/source span，codec 必须维护语义节点与原始表示之间的映射，并支持局部 patch。

## 2. Dirty tracking

修改通过受控 API 进入 mutation journal。dirty tracking 至少到 Part/stream，并可细化到 XML subtree。未修改内容不得因“打开后保存”而被整体重新序列化。

同格式保存应直接复制未修改 ZIP entry/CFB stream；不得无故解压、解析或重新压缩大型媒体。

## 3. 保存策略

- `Preserve`：同格式默认。保留未知内容；无法确认安全时返回 FidelityConflict。
- `Strict`：存在未验证降级、未知冲突或签名破坏时拒绝保存。
- `BestEffort`：允许明确降级/删除，但必须逐项写入 SaveReport。

禁止静默丢失。未受修改影响的未知合法内容应原样保留且不产生噪声诊断。

## 4. 报告

`CapabilityReport` 描述当前 runtime/codec 的静态能力与本文档实例的实际可处理范围。`SaveReport` 描述本次保存的保留、重写、降级、签名影响与诊断。`ConversionReport` 描述目标 codec 无法表达或只能近似表达的能力。

报告条目必须具有稳定 code、严重级别、DocumentLocation、受影响 feature 和建议动作。

ConversionReport 存在损失、降级或未验证能力时，`convert_to()` 只能返回 preflight，不得自动继续；调用者必须显式接受 Strict/Preserve/BestEffort 中适用的策略。

## 5. Dialect 与扩展

每个文档和 Part 记录 dialect/namespace profile。解析通过 profile 映射 Transitional 与 Strict；领域模型不得写死 namespace URI。

同格式保存保持原 dialect；新建文件默认 Transitional，调用者可显式选择 Strict。Microsoft 扩展中已理解部分进入模型，未知部分进入保真 sidecar。“可读取”不得被描述为“完全符合 ISO”。

格式验证器必须独立于容错解析器。解析器可以在策略允许时读取带诊断的非规范输入；验证器必须针对选定标准版本/profile 给出独立结论，禁止以“成功构造领域对象”推导格式合规。

## 6. 签名、宏和嵌入对象

任何签名覆盖字节的变化都必须报告签名失效。初期 `office-signatures` 负责检测、覆盖范围和验证，重新签名以后实现。

VBA、ActiveX 和嵌入 OLE 初期作为 opaque object 保留并允许提取元数据；不得执行。破坏其关系的修改在 Preserve 下失败，在 BestEffort 下逐项报告。

## 7. 公式与缓存

cells-model 保存公式文本、AST、依赖信息和缓存值；XLSX/XLS codec 负责普通公式、共享公式、数组公式和缓存结果的读取与序列化；cells-calc 是可选 target，负责函数实现、依赖图、增量重算和循环引用策略。公式变化但未重算时，codec 必须设置正确 calculation flags，禁止把旧缓存伪装为最新结果。外部链接、宏函数和未实现函数必须产生结构化诊断。
