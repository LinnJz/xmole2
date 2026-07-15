# xmole2 Agent Guide

本文件只负责规范路由。`docs/spec/` 是实现基线；匹配任务时完整读取对应文档，不要一次加载无关规范。

## 始终遵循

- [开发流程](docs/spec/development-process.md)：需求、ADR、实现、验证与完成定义。
- 重大架构或公共契约变更先提交 ADR；实现与 spec 冲突时默认修正实现。

## 按任务加载

| 任务 | 必读规范 |
|---|---|
| C++ 新增、修改、评审 | [代码规范](docs/spec/code-style.md) |
| 构建或第三方库 | [依赖规范](docs/spec/dependencies.md)、[总体架构](docs/spec/architecture.md) |
| 模块边界或 target | [总体架构](docs/spec/architecture.md) |
| 公共 API、句柄、错误、事务 | [公共 API](docs/spec/public-api.md) |
| I/O、SourceLease、临时文件、原子保存 | [I/O 规范](docs/spec/io.md)、[安全规范](docs/spec/security.md) |
| ZIP 索引、entry 流与压缩路径 | [ZIP 规范](docs/spec/zip.md)、[I/O 规范](docs/spec/io.md)、[安全规范](docs/spec/security.md) |
| OfficeRuntime、格式检测、codec registry | [Office Runtime 规范](docs/spec/office-runtime.md)、[公共 API](docs/spec/public-api.md)、[安全规范](docs/spec/security.md) |
| 保存、转换、未知内容 | [保真规范](docs/spec/fidelity.md) |
| 不可信输入、资源或外部访问 | [安全规范](docs/spec/security.md) |
| 测试、fixture、benchmark | [测试规范](docs/spec/testing.md) |
| 旧代码或参考项目 | [迁移规范](docs/spec/migration.md)、[依赖规范](docs/spec/dependencies.md) |
| 模块代码审查 | [审查规范](docs/spec/review.md)、[总体架构](docs/spec/architecture.md)、对应模块 spec |

## 渐进式披露

1. 先读取与当前任务直接相关的规范。
2. 规范引用其他文档且影响当前决策时，再读取该文档。
3. 不以 README、旧实现或参考项目覆盖 spec。
4. 完整会议事实源仅用于追溯；日常实现优先使用已对齐的 spec 与 ADR。
