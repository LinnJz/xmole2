# Contributing to xmole2

`docs/spec/` 是实现基线，完整工程流程见 `docs/spec/development-process.md`。提交变更前：

1. 按 `AGENTS.md` 路由读取与任务相关的规范；
2. 跨模块依赖或公共契约变化先提交 ADR；
3. 建立可验证的 acceptance criteria 和回归/contract test；
4. C++ 遵循 `docs/spec/code-style.md` 并运行 clang-format；
5. 运行 foundation Debug/Release 测试；涉及依赖时再运行 vcpkg dependency smoke；
6. 更新 spec、能力状态和 `CHANGELOG.md`。

Preset 验证——参考 `CMakeUserPresets.json.example` 配置本地 `CMakeUserPresets.json`，然后：

```powershell
# 先列出可用 preset，选择对应名称后替换 <name>
cmake --list-presets all -S .
cmake --preset <name>
cmake --build --preset build-<name>
ctest --test-dir out/build/<name> --output-on-failure
```

`testdata/`、`references/`、`deprecated/` 是不提交 Git 的本地隔离目录，变更不得依赖它们在其他开发机存在。

