# Changelog

本项目遵循语义化版本思想记录面向开发者和使用者的可观察变化。1.0 前公共源码 API 仍可能调整，breaking change 必须提供迁移说明。

## Unreleased

- 完成首次提交后的开发基线审计。
- 增加可复现 vcpkg baseline、公开 foundation presets 和本地 preset 模板。
- 增加 target 依赖、public-header 泄漏、安装消费和 Windows/Linux CI 门禁。
- 补齐 ResourceBudget 的安全资源类别。

## 0.1.0 - 2026-07-15

- 建立 Words、Cells、Slides 多 target 架构规范。
- 交付 `xmole2::base` 的 Error、Result、ResourceBudget、取消与 OperationContext 基础设施。

