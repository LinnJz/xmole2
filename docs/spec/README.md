# xmole2 规范索引

本目录是 xmole2 的规范性基线：

- `architecture.md`：分层、target、依赖与产品边界；
- `public-api.md`：所有权、句柄、错误、事务和生命周期；
- `fidelity.md`：往返、保存、转换、dialect 和未知内容；
- `security.md`：威胁模型、预算、外部访问和主动内容；
- `dependencies.md`：第三方依赖、版本、职责边界和源码研究流程；
- `code-style.md`：C++ 命名、初始化、include、字符串、容器、注释与格式化；
- `testing.md`：测试层次、fixture、CI 与性能门禁；
- `migration.md`：旧实现归档和分阶段实施；
- `development-process.md`：需求到发布的工程流程。

本地且不提交 Git 的参考源码与 fixture，其可追踪元数据分别位于 `../reference-snapshots.md` 和 `../fixtures/catalog.md`。

完整会议事实源是 `../meetings/2026-07-15-office-architecture-all-sessions.md`，必须保持只读。`../meetings/2026-07-15-office-architecture-grill.md` 只是便于导航的决议摘要，不能覆盖、删减或扩展事实源。每项规范性架构决策必须能够追溯到完整会话或 ADR。

规范冲突时，先停止实现并提交 ADR 解决；不得由代码现状反向覆盖规范。
