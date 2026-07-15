# 公共 API 与生命周期规范

## 1. 顶层入口

`OfficeRuntime` 持有显式 codec registry。`OfficeLoader` 负责格式检测、解密编排和打开选定领域对象。统一入口必须将调用者导向 WordDocument、Workbook 或 Presentation，不提供跨产品内容基类。

`OfficeDocument` 的具体 C++ 表示尚未决议；它可以是 discriminated holder，但不能同时以含义不同的“variant/facade”作为契约。实现 office-runtime 前必须用 ADR 在 `std::variant`、类型擦除 facade 或其他方案中作出选择。

格式检测必须区分 `UnsupportedFormat`、`AmbiguousFormat`、`CodecUnavailable`、`EncryptedCredentialRequired` 与 `CorruptInput`。检测由 `office-runtime.md` 定义的多 detector 证据管线完成；公开 `DetectionReport` 必须保留候选、置信度和冲突证据，而不是只返回扩展名或布尔值。

## 2. 所有权与 PImpl

WordDocument、Workbook、Presentation 独占内部状态并使用 PImpl。大型文档禁止隐式深复制；深复制通过 `clone()` 完成并产生新的对象图和 ID 空间。

文档可以 move。move 后所有既有句柄继续有效。文档销毁后句柄操作返回 `DocumentExpired`。

## 3. 句柄契约

Paragraph、Cell、Slide 等领域节点由轻量句柄访问。句柄由内部 state weak reference、稳定 NodeId 和 Generation 标识，不暴露容器地址、XML 节点或裸指针。

- 容器扩容、插入其他节点、删除其他节点不影响句柄。
- 删除目标节点后返回 `StaleHandle`。
- clone 的句柄与原文档不可混用。
- 小型纯值对象按值传递。
- public API 禁止返回内部 `vector`、hash map 或第三方对象引用。

`shared_ptr`/`weak_ptr` 只用于生命周期控制，不代表线程安全。

## 4. 错误与诊断

统一结果类型：

```cpp
template<typename T>
using Result = std::expected<T, Error>;
```

统一错误信封：

```cpp
struct Error
{
  ErrorDomain domain;
  std::uint32_t code;
  Severity severity;
  std::string message;
  std::optional<DocumentLocation> location;
  std::shared_ptr<Error const> cause;
  std::optional<std::int64_t> native_code;
};
```

domain 至少区分 I/O、ZIP、CFB、OPC、XML、Crypto、Words、Cells、Slides、Rendering。code 在 domain 内定义；调用者禁止解析错误文本。cause 必须保留底层错误链和 native code。

可恢复问题进入 DiagnosticBag/DiagnosticSink；导致操作失败的问题进入 Error。普通文件错误不使用异常。内存耗尽等进程级异常策略另行 ADR 决定。

`CollectingDiagnosticSink` 是 base 提供的标准后验收集实现。它复制收到的完整 `Error` 信封，保留 location、cause 与 native_code，并提供 snapshot、take、clear、size 和 empty 操作；不得返回内部 vector 引用。snapshot 返回稳定副本，take 原子移出当前批次并清空 collector。

collector 可以安全接收并发 report；snapshot/take/clear 与 report 互斥。移动或销毁 collector 时调用者必须保证没有并发 report。生产诊断的操作仍负责执行 `max_diagnostic_count`，collector 不得静默截断或自行改变操作结果。

DocumentLocation 必须能够定位 Part、CFB stream、XML path、Word NodeId、sheet/cell、slide/shape。

## 5. OperationContext

所有可能长时间运行的操作接受 OperationContext：

```cpp
struct OperationContext
{
  ResourceBudget budget;
  CancellationToken cancellation;
  ProgressSink* progress;
  DiagnosticSink* diagnostics;
  ExternalResourceResolver* external_resources;
};
```

核心库提供同步 API，不内置线程池。调用者负责线程调度；取消返回结构化 `Cancelled` 并清理临时输出。

## 6. 懒加载与 SourceLease

`open(path)` 创建独占 SourceLease，以保证打开时内容的稳定视图。文档允许在生命周期内持有源文件/流。平台实现必须使用适当共享打开模式，避免无必要阻止其他进程读取或重命名源文件，同时保证不会因路径后来被替换而读取不同内容。`detach()` 将仍需数据物化到内存或临时存储并释放原源；`materialize()` 加载语义内容，但不要求所有大型二进制对象驻留内存。

`from_buffer()` 默认取得 owning buffer。`SourceLease::acquire(unique_ptr<ByteSource>)` 转移 source 独占所有权，`acquire(shared_ptr<ByteSource>)` 共享已有所有权；两者都允许 reader 在原 lease move 或销毁后继续持有 source。

禁止把普通左值通过空 deleter 包装成 shared_ptr 来模拟所有权。借用重载必须以类型编码生命周期并携带可共享 lifetime guard；没有 guard 的栈对象引用不得进入可能逃逸调用栈的 document/archive/reader。保存到原路径使用临时文件和原子替换，不能直接覆盖仍被 SourceLease 读取的源。文档销毁时必须统一释放 SourceLease、解析缓存和所属临时文件。

## 7. 修改与事务

单个便捷修改必须原子完成并保持模型有效。复杂编辑使用显式 EditSession：

```cpp
auto edit = document.begin_edit();
// 多步修改
auto result = edit.commit();
```

commit 验证引用完整性、格式约束和保真冲突；成功后修改原子生效。失败则回滚，回滚创建的句柄变为 StaleHandle。dirty part、诊断、generation 在提交时统一更新。事务 journal 必须为未来 undo/redo 留出接缝，但 1.0 不承诺通用撤销栈。

会议只确定“事务期间必须具有明确的句柄可见性规则”，尚未选择未提交修改是仅 EditSession 可见、由事务外观察提交前快照，还是采用其他模型。实现 EditSession 前必须通过 ADR 选定该契约；在 ADR 接受前，该部分 API 不得实现为公开稳定接口。

## 8. 保存与转换

- `save()`：沿用来源 codec，同格式高保真保存。
- `save_as()`：改变目标位置、选项或同一 codec 的格式变体。
- `convert_to()`：显式切换 codec，先返回 ConversionReport/preflight；报告存在损失、降级或未验证能力时，调用者必须显式接受对应保存策略后才能执行转换。
- 新建文档显式选择格式，便捷入口可默认现代 Transitional 格式。

所有保存操作返回 SaveReport 或包含它的成功结果，禁止只返回布尔值。

## 9. 线程模型

同一文档实例不承诺并发读写，默认按线程约束对象使用。不同文档实例可以并行。所有全局常量与 registry 构建结果必须只读或线程安全。

流式 worksheet reader 可以与其他独立文档操作并行，但每个 reader 实例只允许由单线程驱动。

## 10. ABI 与公开依赖

1.0 前只承诺版本化源码 API，不承诺跨版本 C++ ABI。顶层对象仍使用 PImpl，每个 target 具有导出宏和明确 public/private header。插件/语言绑定使用版本化 C ABI。

第三方库类型禁止出现在公开签名、公开成员和 public headers 中。
