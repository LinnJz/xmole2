---
name: xmole2-driver-development
description: Develop xmole2 Office library modules per spec baseline. Implements one vertical slice per cycle, self-verifies, and hands off structured context for the next task.
---

<task-instruction>

实现指定模块的当前任务。

执行前：

1. 先读取 docs/spec/ 中与本模块相关的规范文档。
2. 检查该模块已在 architecture.md 中定义 target 及依赖方向。
3. 检查 docs/adr/ 下影响本模块的已有决策。
4. 检查 libs/ 下同级模块已完成状态与代码风格，确认公共 API 惯例。
5. 前一轮 handoff 记录了未完成任务 / 待决策点的，优先处理。

执行：

- 遵循 target 依赖方向，不跨层依赖。
- public header 只出现 xmole2 自有类型 + 标准库类型。
- 第三方库仅 PRIVATE；出现前查 dependencies.md 确认版本与 target。
- 句柄契约、事务可见性、保真策略若无 ADR 不得写入公开 API。
- 不可信文件是默认威胁模型；所有长路径统一走 OperationContext（含 budget、cancel、diagnostics）。

</task-instruction>

<self-check>

执行完成后必须逐项验证：

- [ ] 编译通过（当前 MSVC Debug preset）
- [ ] 新增 unit/contract test 通过
- [ ] 改动符合 target 依赖方向，未产生循环或越层依赖
- [ ] public header 无第三方库类型泄漏（absL/pugixml/minizip-ng/fmt/frozen/OpenSSL...）
- [ ] 新增 API 符合 code-style.md（后置返回、命名空间、m_ 私有成员）
- [ ] 与对应 spec 逐条对照：不一致时暂停，先更新 spec 或 ADR
- [ ] Error 传递保留 cause/native_code 链
- [ ] OperationContext（budget/cancel/diagnostics）在本模块路径中贯通
- [ ] clang-format 已格式化本次修改文件
- [ ] 如果涉及第三方库用法变动，与 dependencies.md 固定版本的实际接口一致

任何一条未通过则不能标记完成。

</self-check>

<handoff-output>

任务结束时在回答末尾输出以下结构（供下一轮直接使用）：

```markdown
## Cycle Handoff

### 本轮完成
- 模块：<target 名>
- 文件变更：<路径列表>
- 实现内容：<一句话描述>

### 状态
- 本轮 contract test：<通过数>/<总数>
- 编译：<通过/失败>
- 已知问题或待完善项：<列表>

### 与 spec 一致性
- 已对照 <文档名>：<全部对齐 / 发现差异并已修复 / 需 ADR>
- 未决决策：<列表，如无可省略>

### 下轮任务建议
- 按 migration.md 阶段：<M0~M5 + 具体步骤>
- 优先级：<高/中/低>
- 开始前应先读取：<docs/spec/xxx.md, docs/adr/xxx>
</handoff-output>
<feedback-loop>
本 skill 采用单任务 → 自检 → handoff 循环：
1. 每次只处理一个切片（一个 target 或一组紧密关联 target）。
2. 不提前实现未要求的模块。
3. 自检未通过则不生成 handoff，返回修复。
4. handoff 直接描述下轮做什么、优先级和必读文档。
5. 下轮以此 handoff 为起点，不需要重新阅读全部会议记录。
</feedback-loop>
```