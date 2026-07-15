# ADR-0002：外部资源解析器采用默认拒绝的受预算 owning-byte 契约

状态：Accepted  
日期：2026-07-16

## Context

ADR-0001、架构会议 Q31/Q32 与 `security.md` 已确定：核心操作不得隐式访问网络、进程或任意文件，外部关系只能通过调用者注入的 `ExternalResourceResolver` 获取，并且解析过程必须贯通 `OperationContext`。此前 `OperationContext` 只有该类型的前向声明，没有可实现的请求、结果、默认拒绝路径或 contract test。

`xmole2::base` 不能反向依赖 `xmole2::io`，因此解析器的公开结果不能暴露 `ByteSource`。同时，未限制大小的 owning buffer 会绕过单资源和内存预算。

## Decision

在 `xmole2::base` 定义窄的同步解析契约：

- `ExternalResourceRequest` 借用本次调用期间有效的规范化 URI 和资源类型，并可携带来源位置；
- `ExternalResource` 拥有返回 bytes、最终 URI 与内容类型；resolver 未填写最终 URI 时，统一入口使用请求 URI，空内容类型表示未知；
- `ExternalResourceResolver::resolve()` 接收原始 `OperationContext`，自定义实现必须遵守其中的取消、预算、诊断和调用者策略；
- 调用方统一经 `resolve_external_resource()` 进入。`external_resources == nullptr` 时返回稳定的 `BaseErrorCode::ExternalAccessDenied`，不执行任何隐式访问；
- 统一入口在调用解析器前后检查取消，并在成功返回后执行 `max_input_bytes`、`max_single_resource_bytes` 与 `max_memory_bytes` 边界检查；文档级 `max_external_resource_count` 累计由持有共享操作状态的上层负责；
- `OperationContext::external_resources` 是非 owning 指针，解析器必须存活到操作结束。请求中的 `string_view` 不得被解析器保存到调用返回之后；返回值必须独立拥有内容；
- 核心库不串行化不同操作对同一 resolver 的调用；调用者把一个 resolver 实例共享给并发操作时，该实现必须自行保证线程安全；
- 解析器错误原样传播，以保留 domain、code、location、cause 与 native_code。

## Consequences

默认构造的 `OperationContext` 具有可调用且确定性的外部访问拒绝语义，调用者可以实现受策略控制的文件、缓存或网络适配器，而 base 不依赖平台 I/O 或网络库。

当前接口会把单个外部资源物化为受内存预算限制的 bytes。未来若真实场景需要大型外部资源流式读取，应通过独立 ADR 引入不造成 base→io 反向依赖的流式 port；不得在现有接口中偷偷返回无界延迟流。
