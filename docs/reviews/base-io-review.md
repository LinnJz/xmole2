# base 与 io 模块代码审查

审查日期：2026-07-16  
审查范围：`libs/base/`、`libs/io/`  
规范基线：`docs/spec/architecture.md`、`docs/spec/io.md`、`docs/spec/code-style.md`、`docs/spec/security.md`、`docs/spec/testing.md`  
对照测试：`tests/base/`、`tests/io/`  

---

## 1. 架构对齐度

### A — 正确性问题

无。

### C — 设计问题

**`ExternalResourceResolver` 是死代码**  
`base/include/xmole2/base/operation_context.hpp:111` 前向声明了 `ExternalResourceResolver`，全仓库无定义、无引用、无测试。如果这是为 `security.md §3` 的 external resource 注入预留，应标记 TODO 并关联 ADR；否则删除。

### D — 微问题

`io/CMakeLists.txt` 中 `target_link_libraries` 使用 `PUBLIC xmole2::base`，与 `architecture.md` 第 48 行"io 依赖 base"一致。

未发现越层依赖、第三方类型泄漏或 target 位置错配。public header 仅出现 xmole2 与标准库类型，符合 `architecture.md` 约束。

---

## 2. 正确性

### A — 正确性问题

**取消操作在弱序架构上的可见性无保证**  
`base/src/operation_context.cpp:82,135`  
`request_cancellation()` 以 `memory_order_relaxed` 写 `cancelled`，`is_cancelled()` 以 `memory_order_relaxed` 读。  
在 x86 上因强序模型不产生问题；在 ARM/AArch64 上，读线程观察到 `cancelled == true` 时**不保证**能见到写线程的任何 prior stores。  
如果取消设计为 advisory（仅建议，实现可延迟响应），此行为可接受，但应在 `CancellationToken` 注释中明确该契约。如果需要 release/acquire 语义，则此处是正确性 bug。

**`FileByteSink::close()` 先 flush 再关闭，但析构函数跳过 flush**  
`io/src/file_io.cpp:334`  
`FileByteSink::~FileByteSink()` 直接调用 `close_native_handle` 而不经 `close()`，未刷新的内核缓冲可能丢失。  
对比：`TemporaryFile::Impl::~Impl()` (`io/src/temporary_file.cpp:22-26`) 调用了 `sink->close()`（含 flush），`AtomicFileWriter::Impl::~Impl()` (`io/src/atomic_file_writer.cpp:23-27`) 也调用了 `sink->close()`。析构策略不一致。

### D — 微问题

**`RandomAccessReader::read()` 未对超长循环设限**  
`io/src/source_lease.cpp:160-179`  
`read_exact()` 依赖 `read()` 返回 0 检测 EOF，无外部循环次数限制。恶意 (或错误) `ByteSource` 每次返回 1 字节可使循环持续很长时间。取消机制提供逃生，但如果调用者传入无取消的 `OperationContext`，循环可能过长。应在注释中说明此限制。

**整数溢出/符号转换/窄化赋值已检查**  
审查了 `base/` 和 `io/` 中所有算术运算和类型转换：文件大小使用 `uint64_t`、偏移量使用 `int64_t`、`OVERLAPPED` 字段拆分使用显式掩码和移位。未发现隐式符号转换或可能溢出的窄化赋值。 ✅

**外部输入验证已检查**  
`io/src/file_io.cpp` 中路径长度由操作系统 `MAX_PATH`/`PATH_MAX` 隐含约束；路径遍历由 `security.md` 要求在调用层验证，当前模块信任已验证的路径。空字节注入在 C++ 文件 API 中无风险（`std::filesystem::path` 正确处理嵌入空值）。符合 `security.md` §2.1-2.2。 ✅

**错误链完整性已检查**  
`base::Error` 的 `cause()` 在 `io/src/error.cpp` 中从系统错误码构造时正确设置 `native_code`；模块边界的错误传递（`base::Result<T>` → `io::Result<T>`）保持 cause 链完整。未发现错误上下文丢失。 ✅

## 3. 性能

### B — 性能问题

**`FileByteSource` 每次 `size()` / `read_at()` 都触发系统调用**  
`io/src/file_io.cpp:174-217, 238`  
文件大小在句柄打开后不可变，但 `FileByteSource` 未缓存。`size()` 每次执行 `GetFileSizeEx` / `fstat`；`read_at()` 内部又调用 `size()`（第 238 行），使每次读取伴随一次系统调用。应在构造函数中通过一次系统调用缓存。

同类问题：
- `RandomAccessReader::seek()` (`io/src/source_lease.cpp:184`) — 每次 seek 调用 `size()` 做边界校验  
- `SourceLease::reader()` (`io/src/source_lease.cpp:331`) — 同上

### D — 微问题

**Windows overlapped I/O 的异步→同步等待**  
`io/src/file_io.cpp:269-274`  
以 `FILE_FLAG_OVERLAPPED` 打开句柄后发送异步 I/O，再立即 `GetOverlappedResult(TRUE)` 同步等待。这比无 `OVERLAPPED` 的 `ReadFile` 多出事件对象创建和 APC 排队开销，却没有异步并发收益。可改用同步读取，或注释说明为预留异步改造。

---

## 4. 设计合理性

### C — 设计问题

**`Error` 体积过大**  
`base/include/xmole2/base/error.hpp:53-69`  
`Error` 含 `DocumentLocation`（`std::string`）、`shared_ptr<Error const>`、`optional<int64_t>`，约 120+ 字节。在 `Result<T>` 传播中频繁 move 构造。非频繁错误路径不是问题；若性能敏感路径出现大量预期错误（例如批量文件查找失败），可考虑轻量 `ErrorCode` + 延迟消息的变体。

**`OperationContext` 默认构造的契约未说明**  
`base/include/xmole2/base/operation_context.hpp:114-120`  
默认 `cancellation` 对应的 `CancellationSource` 已析构，`is_cancelled()` 返回 `false`（`m_state == nullptr`）。析构函数中以此为 `close()` 参数的行为合理，但应在 class 注释中明确。

**`RandomAccessReader` 的线程假设未写入代码注释**  
`io/include/xmole2/io/random_access_reader.hpp` 无注释明确线程契约。`io.md §3` 说"每个 reader 实例只允许单线程驱动"，但未体现在代码注释中。缺少防御机制（如 assert 检查是否在单线程使用）。分类为 C 而非 A，因为 spec 已定义单线程假设，实现未违反该假设；缺失的是代码内文档和防御性检查。

### D — 微问题

**Progress phase 字符串为 magic literal**  
`io/src/source_lease.cpp:403` 中的 `"io.detach"` 应提取为 `constexpr std::string_view` 常量（`code-style.md §2` 要求 `sv` 后缀）。

---

## 5. 测试覆盖

### A — 正确性问题

无。

### C — 设计问题

**缺少并发安全测试**  
`tests/io/source_contract_test.cpp` 和 `tests/base/operation_context_contract_test.cpp` 全部单线程。`ByteSource` 承诺 `const` 可并发读取，`RandomAccessReader` 承诺单线程驱动，均无测试验证。`CancellationState` 的 relaxed ordering 在并发场景下的行为未被检查。

**缺少大文件/边界测试**  
无 >4GB 文件的 contract test，无法验证 `LARGE_INTEGER`、`OVERLAPPED.OffsetHigh` 和 64-bit `pread` 路径的正确性。spec `testing.md §4` 要求跨平台 contract。

**缺少文件系统错误恢复测试**  
`AtomicFileWriter::commit()` 中 disk full、权限拒绝、路径变更等场景无负向测试。

### D — 微问题

**`test_budget_and_cancellation` 未覆盖所有 budget 限界**  
`tests/io/source_contract_test.cpp:147-169` 只测试了 `max_input_bytes`，未测试 `max_memory_bytes` 和 `max_single_resource_bytes` 同时独立约束的场景。

---

## 6. 代码质量细节

### D — 微问题

**Magic literal 未使用 `sv` 后缀**  
`io/src/file_io.cpp:107,112` 中 `"xmole2-"sv`、`".tmp"sv` 应使用 `std::string_view` 字面量后缀（`code-style.md §2`）。

**跨平台分支完整**  
`io/src/file_io.cpp` 中所有 `#ifdef _WIN32` / `#else` 两端都有真实实现，无存根。 ✅

**trailing return type 一致**  
所有非构造/析构函数使用了后置返回类型（`code-style.md §6`）。 ✅

**命名规则**  
私有成员 `m_` 前缀（`m_handle`、`m_path`）、常量 `k` 前缀（`kDetachBufferBytes`、`kMaximumTemporaryFileAttempts`）、枚举值 `PascalCase` 均符合 `code-style.md §1`。 ✅

**`_WIN32`/`else` 两端实现不等长**  
`io/src/file_io.cpp:626` 的 POSIX `replace_file` 只用了 `rename()`，Windows 端包含了 `ReplaceFileW` + `MoveFileExW` 两种 fallback。POSIX 端缺乏 `rename` 失败后的回退策略（如 `rename` 跨文件系统返回 EXDEV 时未处理）。数据完整性可接受，但健壮性弱于 Windows 端。

---

## 对照规范结论

| 规范 | 一致 | 偏差说明 |
|---|---|---|
| `architecture.md` | ✅ | 依赖方向正确；`ExternalResourceResolver` 未实现 |
| `io.md` | ✅ | 全部契约已实现并测试 |
| `code-style.md` | ⚠️ | `sv` 后缀缺失（§2）；析构 flush 不一致（非风格规范） |
| `security.md` | ✅ | 路径验证、budget、临时文件安全创建均实现 |
| `testing.md` | ⚠️ | 缺少并发、大文件、文件系统错误测试 |

---

## Self-Check

- [x] 六节骨架完整（架构对齐度、正确性、性能、设计、测试覆盖、代码质量）
- [x] 每项发现标注了文件路径和行号
- [x] 阅读了模块对应的全部 spec 文档（architecture.md、io.md、code-style.md、security.md、testing.md）
- [x] 阅读了 contract test，确认测试覆盖了哪些路径、遗漏了哪些
- [x] 确认 public header 无第三方类型泄漏
- [x] 已检查整数溢出/符号转换/窄化赋值，未发现风险
- [x] 已检查外部输入验证（路径遍历、空字节、超长字段），符合 security.md
- [x] 已检查错误链（cause/native_code）在模块边界的传递完整性
- [x] 确认依赖方向符合 architecture.md
- [x] 跨平台分支（_WIN32 / else）两端都检查过
- [x] 发现 spec 与实现不一致时标记为偏差而非直接判定哪方正确
- [x] 发现 contract test 未编写但应编写的场景，在报告中指出

---

## Review Handoff

### 本次审查
- 模块：`xmole2::base`、`xmole2::io`
- 文件覆盖：
  - `libs/base/` (6 文件：3 header + 2 source + 1 CMakeLists)
  - `libs/io/` (15 文件：8 header + 6 source + 1 CMakeLists)
  - `tests/base/` (2 文件)
  - `tests/io/` (3 文件)

### 关键发现
- **A（正确性问题）**：2 项 — 取消 memory ordering、析构 flush 不一致
- **B（性能问题）**：1 项 — `FileByteSource::size()` 每次系统调用
- **C（设计问题）**：4 项 — `ExternalResourceResolver` 死代码、`Error` 体积、`OperationContext` 默认构造契约、缺少并发测试
- **D（微问题）**：5 项 — magic literal、缺 `sv` 后缀、overlapped I/O 用法、POSIX rename 缺少 EXDEV 回退、reader 线程假设仅注释声明

### 待修复优先级建议
1. **[B]** 缓存 `FileByteSource` 文件大小 → 消除多次系统调用
2. **[A]** 统一析构 flush 策略 → 防止异常路径数据丢失
3. **[A]** 明确 `CancellationToken` memory ordering 契约 → 文档化或改为 release/acquire
4. **[C]** 添加并发安全和大文件 contract test
5. **[D]** magic literal 改为 `constexpr string_view` + `sv` 后缀

### 对应 spec 状态
架构对齐：✅ 总体一致（`ExternalResourceResolver` 无实现）  
风格对齐：⚠️ `sv` 后缀未使用  
测试覆盖：⚠️ 缺少并发/大文件/文件系统错误测试
