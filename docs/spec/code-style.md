# C++ 代码规范

状态：Normative Baseline  
适用范围：xmole2 的 C++ 源码、测试、示例与代码生成输出

## 1. 命名

| 类别 | 规则 | 示例 |
|---|---|---|
| 类型、结构体、枚举、别名 | `PascalCase` | `DocumentState`, `ErrorCode` |
| 函数 | `snake_case` | `open_document()` |
| 局部变量、参数、公有成员 | `snake_case` | `part_name` |
| 常量 | `k` + `PascalCase` | `kMaxPartCount` |
| 枚举值 | `PascalCase` | `Severity::Warning` |
| 私有/保护成员 | `m_` + `snake_case` | `m_source` |
| 静态成员 | `s_` + `snake_case` | `s_instance_count` |
| 命名空间 | 小写 `snake_case` | `xmole2::office` |
| 宏 | 大写 `SNAKE_CASE` | `XMOLE2_BASE_API` |

命名应表达领域含义，禁止用含糊缩写代替规范术语。

## 2. constexpr 字符串

编译期字符串常量使用 `std::string_view` 的 `sv` 后缀：

```cpp
using std::literals::string_view_literals::operator""sv;

inline constexpr auto kOfficeDocument = "officeDocument"sv;
```

## 3. 初始化与返回值

一般初始化和返回值构造使用 `{}`。容器的大小构造、迭代器区间构造保留 `()`，避免与 initializer-list 语义混淆。

```cpp
auto const error = Error { ErrorDomain::Opc, 1 };
return std::unexpected { error };

return std::vector<std::uint8_t>(text.begin(), text.end());
```

禁止无必要的默认构造后赋值。

## 4. 字符串

- 格式化使用 `fmt::format`。
- 字符串拼接使用 `absl::StrCat`。
- 禁止使用字符串 `operator+` 构造拼接结果。
- 热点循环中的格式化和临时字符串必须经过测量。

```cpp
auto const message = fmt::format("invalid part: {}", part_name);
auto const path = absl::StrCat(base_path, "/", part_name);
```

## 5. 容器与编译期查找

内部运行时容器可按语义和性能选择 Abseil：

- hash：`absl::flat_hash_map/set`；需要地址稳定性时评估 `node_hash_map/set`；
- ordered：`absl::btree_map/set`；
- small/temporary：`absl::InlinedVector`、`absl::FixedArray`。

公共 API 不得暴露 Abseil 类型。容器替换必须先确认所有权、迭代顺序、地址失效和可测量收益，不能只凭“更快”选择。

编译期可确定的固定查找表优先使用 frozen 的 map/set/unordered 容器：

```cpp
constexpr auto kKinds = frozen::make_unordered_map<std::string_view, Kind>({
    { "word"sv, Kind::Word },
    { "sheet"sv, Kind::Spreadsheet },
});
```

`magic_enum` 仅用于合适的内部枚举辅助。持久格式、协议字符串和稳定公共名称必须使用显式映射，不能依赖编译器生成的枚举名称。

第三方库的版本、接口检查和允许依赖层见 [依赖规范](dependencies.md)。

## 6. 后置返回类型

所有非构造/析构函数必须使用后置返回类型（trailing return type）语法：

```cpp
constexpr auto is_cancelled() const noexcept -> bool;
auto make_error(std::string_view message) -> Error;
auto main() -> int;
auto request_cancellation() noexcept -> void;
```

## 7. 注释

使用精简的 Doxygen 风格注释。公共 API、生命周期、失效规则、安全边界和非显然算法必须说明；代码已经清楚表达的事实不重复注释。

```cpp
/// Returns the part bytes without materializing unrelated entries.
/// @return StaleHandle when the owning node has been removed.
Result<ByteView> read_part(PartHandle const &part);
```

注释解释“为什么”和契约，不解释逐行语法。TODO 必须包含可追踪的问题或明确的后续条件。

## 8. 格式化

使用仓库根目录 `.clang-format`。Windows 开发环境的固定工具路径：

```powershell
& 'E:\Development\LLVM\bin\clang-format.exe' -i <files>
```

只格式化本次修改涉及的 C++ 文件；不要用全仓格式化掩盖功能变更。
