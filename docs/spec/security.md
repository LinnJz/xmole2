# 安全与资源规范

## 1. 威胁模型

所有 Office 文件默认不可信。解析、解密、计算、渲染和保存必须共享同一个 ResourceBudget，不能由各层独立设置不协调的限制。

## 2. ResourceBudget

预算至少覆盖：

- 输入、总解压、单 Part/stream、entry 数量；
- XML 深度、节点、属性、文本和实体策略；
- CFB sector、目录项、stream chain、循环；
- 关系数量、路径长度、嵌套和外部关系；
- KDF 成本、密码尝试、解密输出；
- 图片像素、字体、嵌入对象和渲染复杂度；
- worksheet 行列、公式图和计算步数；
- 内存、临时磁盘、诊断数量和取消状态。

所有计数与字节运算必须检查溢出、窄化和累计预算。预算耗尽返回具体 domain/code，不得表现为普通格式错误。

产生诊断的操作必须在调用 `DiagnosticSink::report()` 前执行 `max_diagnostic_count`。达到上限时停止产生新的可恢复诊断并返回对应 domain 的资源限制错误；`CollectingDiagnosticSink` 只负责保存已接受的诊断，不得用静默丢弃掩盖预算耗尽。

默认预算必须安全且适用于普通交互场景。可信批处理只能通过显式选项放宽具体限制；禁止以“本地文件”或“已通过扩展名检测”为由自动切换到无限预算。

## 3. 外部访问

默认禁止网络、外部进程和任意路径读取。外部关系只能经调用者注入的 `ExternalResourceResolver` 获取。resolver 接收规范化 URI、资源类型、来源位置和同一 `OperationContext`；请求借用字段只在调用期间有效，响应必须 owning。

所有调用必须通过 `resolve_external_resource()`：空 resolver 返回 `BaseErrorCode::ExternalAccessDenied`。入口在调用前后检查取消，并拒绝超过 `max_input_bytes`、`max_single_resource_bytes` 或 `max_memory_bytes` 的响应；上层按共享操作状态累计 `max_external_resource_count`。自定义 resolver 仍必须限制 scheme、根目录/域名、超时、重定向和内容类型，且不得通过内部重试放宽调用者预算。未解析的外部关系保存时保留。具体公共契约见 ADR-0002。

渲染或计算因外部资源不可用而无法完整执行时，必须产生结构化诊断并遵守调用策略；禁止静默等待、隐式重试网络或无说明地使用空内容替代。

## 4. 主动内容

解析、格式检测和渲染不得执行 VBA、ActiveX、嵌入程序或 shell 命令。压缩路径、关系 URI 和输出路径必须防止 traversal。XML 默认禁用危险实体行为。

## 5. 密码与密钥

密码由调用者提供的 credential provider 按需获取，不写日志、不进入诊断文本，并尽可能缩短明文驻留时间。crypto primitive 不负责交互；office-encryption 和具体 codec 只消费凭据接口。

## 6. 临时输出

临时文件使用安全创建方式、受预算控制，并在成功、失败或取消后清理。原路径保存必须先完成临时文件写入、flush/close 和验证，再执行平台适配的原子替换。
