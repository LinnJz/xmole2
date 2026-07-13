# 测试与质量规范

## 1. 测试层次

- unit：纯值类型、路径、解析 primitive、预算与错误映射。
- contract：每个 port/codec 的公开契约；同一 contract 可运行在不同实现。
- round-trip：未修改直拷、局部修改、未知内容保留、确定性输出。
- interoperability：Microsoft Office 与 LibreOffice 打开/保存验证。
- negative/security：损坏 ZIP/CFB/XML、重复身份、路径攻击和预算边界。
- fuzz：ZIP/CFB/XML、关系、公式及格式入口。
- benchmark：small/medium/large 数据集的时间、峰值内存、分配与吞吐。

## 2. 三格式最小垂直切片验收

DOCX、XLSX、PPTX 必须同时完成：

1. OfficeRuntime 显式注册和统一检测；
2. OPC Part 索引懒加载；
3. 创建正确顶层领域对象；
4. 打开真实样本并读取核心属性；
5. 完成一个最小领域编辑；
6. 未修改 Part 字节直拷，dirty Part 重写；
7. 输出 CapabilityReport、SaveReport 和结构化诊断；
8. 同格式保存后由本库、Office/LibreOffice 重新打开。

OfficeRuntime contract 必须覆盖仅注册 Words、仅注册 Cells、仅注册 Slides、无匹配 codec、mock codec、故障 Source、受限能力 codec 和“包含当前构建全部内置 codec”的便捷 runtime。

该切片通过前，不继续大规模迁移 rdocx 的深层 Words 能力。

## 3. Fixture 管理

所有 fixture 位于 `testdata/`，具有 manifest：来源、许可证、哈希、格式/dialect、预期能力、是否含敏感信息。参考实现输出不得作为规范 oracle。

## 4. CI 门禁

CI 必须检查：

- target 依赖方向与 forbidden include；
- public header 无第三方类型泄漏；
- 格式化、编译警告、unit/contract/round-trip；
- Windows x64，随后 Linux x64 与 macOS；
- CFB、ZIP、XML 与六个格式 codec 的跨平台 contract；
- sanitizer/fuzz 定期任务；
- benchmark 基线的显著回退；
- 规范或公共契约变更是否包含 ADR 与测试更新。

## 5. 性能原则

不凭 Big-O 或猜测进行微优化。性能 finding 必须有代表性 fixture 和可复现数据。大型 XLSX 测试必须验证打开阶段不会物化所有 worksheet，行级流式 reader 的内存随窗口而非文件总量增长；完整随机编辑只能通过显式高内存模式启用。DOCX/PPTX 测试必须验证默认按 Part 懒加载。
