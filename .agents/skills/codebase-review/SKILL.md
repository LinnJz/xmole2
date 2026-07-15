---
name: codebase-review
description: Structured, critical code review for xmole2 implementation modules. Covers architecture alignment, correctness, performance, design, test coverage, and code quality. Use when user asks to "review", "audit", "evaluate", "check", or "read the code" of one or more targets.
---

<task-instruction>

执行一次结构化代码审查，覆盖一个或多个模块的全部源文件（包括 public header、private 实现、CMakeLists.txt 和 contract test）。

执行前：

1. 读取 `docs/spec/review.md`，以其中的六节结构作为报告骨架。
2. 读取 `docs/spec/architecture.md` 确认被审查模块的 target 定义、职责和允许依赖方向。
3. 读取 `docs/spec/` 中与被审查模块直接相关的规范（如 io.md、zip.md、security.md 等）。
4. 读取被审查模块所在目录的全部 `.hpp`、`.cpp` 和 `CMakeLists.txt`。
5. 读取该模块的 contract test（位于 `tests/` 下对应目录）。
6. 如果被审查模块依赖其他已实现模块，读取其 public header 但不深入实现。

审查者守则：

- **假定实现可能有错**。不以"显然是正确的"跳过验证。
- **先验证据优先于模型推理**。对性能可测量、对安全性可证明、对正确性可测试。
- **识别重复模式中的差异**。例如两个析构函数一个 flush、另一个不 flush；两个类一个禁用 move、另一个允许。
- **每项发现必须标注文件路径和行号**。
- **不确定时主动索要补充信息**。标记需要什么的证据后再继续。
- **不奉承**。确实优秀的实现用精确描述代替夸奖。

</task-instruction>

<review-categories>

按以下分类组织发现，每类内部按严重性降序排列：

### A — 正确性问题
数据损坏、安全漏洞、未定义行为、违反 spec 明确约束。必须修复。

### B — 性能问题
可测量的实际影响：多余系统调用、冗余 I/O、算法复杂度误配（O(n) 误为 O(1)）、不必要的堆分配。应修复。

### C — 设计问题
抽象泄漏、模块边界违反、public API 暴露实现细节、不必要的扩展点或占位符、可移动对象禁用 move 导致堆间接。应改进。

### D — 微问题
无实际影响但应修复的不一致、风格偏离、magic literal、冗余检查、注释过时或缺失。

</review-categories>

<self-check>

审查完成后逐项验证：

- [ ] 六节骨架完整（架构对齐度、正确性、性能、设计、测试覆盖、代码质量）
- [ ] 每项发现标注了文件路径和行号
- [ ] 阅读了模块对应的全部 spec 文档，与实现逐条对照
- [ ] 阅读了 contract test，确认测试覆盖了哪些路径、遗漏了哪些
- [ ] 确认 public header 无第三方类型泄漏
- [ ] 确认依赖方向符合 architecture.md
- [ ] 跨平台分支（_WIN32 / else）两端都检查过
- [ ] 如发现 spec 与实现不一致，标记为偏差而非直接判定哪方正确
- [ ] 如发现 contract test 未编写但应编写的场景，在报告中指出

</self-check>

<handoff-output>

审查结束时在末尾输出：

```markdown
## Review Handoff

### 本次审查
- 模块：<target 名，多个时列出>
- 文件覆盖：<路径列表>

### 关键发现
- <数量> 个正确性问题
- <数量> 个性能问题
- <数量> 个设计问题
- <数量> 个微问题

### 待修复优先级建议
- <修复列表，按严重性排列>

### 需补充信息
- <如无可省略>

### 对应 spec 状态
- <全部对齐 / 发现偏差并记录 / 需进一步确认>
```

</handoff-output>

<feedback-loop>

1. 审查是完整快照，不分批输出。
2. 发现不完整或矛盾时先标记、询问、再继续。
3. handoff 输出供修复者或后续审查直接使用。
4. 本 skill 不生成修复代码。修复由 `xmole2-driver-development` skill 或其他实现 skill 完成。

</feedback-loop>
