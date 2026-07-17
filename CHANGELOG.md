# Changelog

本项目遵循语义化版本思想记录面向开发者和使用者的可观察变化。1.0 前公共源码 API 仍可能调整，breaking change 必须提供迁移说明。

## Unreleased

- 修复 CFB metadata/payload 提前 EOF 的阶段错误映射与 stream 验证进度计数，并补齐计算阶段取消、64-bit sector 边界和跨 FAT sector 读取 contract。
- 扩展 CMakePresets.json 编译器 base：新增 gcc-base、clang-base、clang-cl-base，支持 Ninja 下 GCC、Clang、Clang-Cl 构建。
- 增加 CMakeUserPresets.json.example 参考文件（JSON5 注释格式），说明编译器路径、包管理器（vcpkg/Conan/FetchContent）和环境变量的可选配置方式。

- 完成首次提交后的开发基线审计。
- 增加可复现 vcpkg baseline、公开 foundation presets 和本地 preset 模板。
- 增加 target 依赖、public-header 泄漏、安装消费和 Windows/Linux CI 门禁。
- 补齐 ResourceBudget 的安全资源类别。

## 0.1.0 - 2026-07-15

- 建立 Words、Cells、Slides 多 target 架构规范。
- 交付 `xmole2::base` 的 Error、Result、ResourceBudget、取消与 OperationContext 基础设施。
