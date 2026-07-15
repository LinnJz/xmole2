# I/O 层规范

状态：Normative Baseline  
适用 target：`xmole2::io`

## 1. 边界与依赖

I/O 层只依赖 `xmole2::base` 和标准库，负责稳定字节源、顺序字节输出、平台文件句柄、临时存储与原子替换。它不知道 ZIP、CFB、OPC 或任何 Office 产品语义。

public header 禁止暴露 Win32 handle、POSIX fd 或第三方类型。平台实现位于 private source。

## 2. ByteSource

`ByteSource` 是随机访问 port：

- `size(context)` 返回稳定字节序列的长度；
- `read_at(offset, destination, context)` 最多返回 destination 长度，零表示 EOF；
- 实现不得改变调用者未报告为已读取的字节；
- 所有路径必须检查取消、输入大小与单资源预算；
- 系统失败使用 Io domain，保留 native_code；上层 wrapper 不得丢失 cause/native_code。

自定义 source 可经 `SourceLease::acquire()` 注入，用于 runtime 与故障测试。RandomAccessReader 必须防御违反 read length 契约的 source。

## 3. RandomAccessReader

RandomAccessReader 是 move-only cursor view。`read()` 推进 cursor，`read_exact()` 在提前 EOF 时返回 UnexpectedEndOfFile，`seek()` 只允许 `[0, size]`。Reader 持有共享 source 状态，因此可以在创建它的 SourceLease move 或销毁后继续读取。

每个 reader 实例只允许单线程驱动。

## 4. SourceLease

SourceLease move-only，负责保持打开时的 source identity：

- Windows 文件使用允许读取和重命名/替换、但不主动允许原地写入的新句柄共享模式；
- 路径后来被重命名或替换时，lease 仍读取原打开对象；
- owning buffer 受 max_input、max_single_resource 与 max_memory 限制；
- 不提供隐式 borrowed buffer；未来借用 API 必须把生命周期编码到类型。

`detach(context)` 将 source 复制到受预算控制的临时存储，成功后替换 lease 自身的 source 并释放其原引用。已经创建的 reader 仍拥有原 source；detach 不强制使外部 reader 失效。

## 5. ByteSink

ByteSink 是顺序输出 port。`write()` 成功时必须完整消费输入 span；部分写入后发生的失败仍必须计入 `bytes_written()` 和预算。`flush()` 持久化文件缓冲，`close()` 幂等并在关闭前 flush。关闭后的写入返回 SinkClosed。

本阶段文件 sink 由 TemporaryFile 与 AtomicFileWriter 提供，不暴露“直接截断任意目标文件”的便捷工厂，避免绕过原子保存。

## 6. TemporaryFile

TemporaryFile 使用排他创建，默认位于系统临时目录，也可指定已存在目录。文件名冲突必须安全重试；不得跟随预先存在的同名文件。

对象销毁时关闭并删除临时文件。`seal(context)` 关闭 sink 后返回 SourceLease，删除责任转移到该 lease 的底层 source；最后一个引用释放时删除文件。写入受 max_temporary_storage_bytes 和取消控制。

## 7. AtomicFileWriter

AtomicFileWriter 必须在目标同目录创建临时文件：

1. create 不修改目标；
2. sink 写入临时文件；
3. commit 检查取消，flush 并 close；
4. 仅在前述步骤成功后原子替换目标；
5. 失败、取消或未 commit 的析构路径删除临时文件，目标保持原内容。

commit 成功后重复 commit 返回 AlreadyCommitted。实现保证同文件系统的原子名称替换；跨掉电持久性不超出平台 flush/replace primitive 的保证。

## 8. Contract 与负向测试

contract 必须覆盖：内存 source、稳定文件 identity、随机读取/EOF、预算、取消、故障 cause/native_code、恶意 source 返回长度、detach、临时文件生命周期、临时预算和原子提交前后目标内容。

