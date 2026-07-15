# ZIP 容器规范

状态：Normative Baseline  
适用 target：`xmole2::zip`

## 1. 边界与依赖

ZIP 层只依赖 `xmole2::base`、`xmole2::io`，并以 PRIVATE 方式使用固定版本的 minizip-ng 后端。它负责 ZIP 物理 entry 索引与顺序解压流，不解释 OPC Part、Content Types 或 Office 产品语义。

public header 禁止出现 minizip-ng handle、结构体、错误码或其他第三方类型。后端替换不得改变调用方 API。

## 2. 打开与索引

`ZipArchive::open()` 取得 move-only `SourceLease`，只读取 ZIP 目录和 entry 元数据，不解压 entry payload。成功后索引在 archive 生命周期内不可变，并保持打开时的 source identity。

索引至少记录 entry 序号、名称、压缩/解压尺寸、CRC32、压缩方法、加密与目录标志。按名称查找区分大小写；重复名称会使打开失败，禁止静默选择其中一个 entry。

entry 名称视为 ZIP 内部 UTF-8/字节名称，不映射到本地文件系统。名称必须拒绝绝对路径、反斜杠、`.`/`..` 路径段、Windows drive 前缀和内嵌 NUL；目录名称可以单个 `/` 结尾。当前读取切片不提供 extraction API。

## 3. 流式读取

`ZipEntryReader` 是 move-only 顺序 reader。每个 reader 使用独立的后端游标，archive 销毁后仍可继续读取稳定 source。`read()` 最多填充目标 span 并推进 position，零表示已完整读取且 CRC/尺寸验证成功。

读取不会把整个 entry 物化到内存。调用者读完声明的解压尺寸后，下一次 `read()` 或显式 `finish()` 完成 CRC 与 data descriptor 验证；未消费完整 entry 时调用 `finish()` 返回 InvalidArgument。reader 实例只允许单线程驱动。

当前 portable contract 只承诺 Store 和 Deflate。加密 entry 返回 EncryptedEntry，其他压缩方法返回 UnsupportedCompression；加密编排属于 `office-encryption`，不得在 ZIP 层接收密码。

## 4. 资源、取消与诊断

打开索引必须检查 source 大小、entry 数量、名称长度、索引内存估算、单 entry 解压尺寸以及所有 entry 解压尺寸之和。所有累计运算必须防溢出。

entry 读取再次应用当前 `OperationContext` 的 `max_single_resource_bytes`、`max_expanded_bytes` 和取消状态，并按实际输出检查声明尺寸。索引与读取通过 progress sink 分别报告 `zip.index` 和 `zip.read`。

预算耗尽、取消、格式损坏、完整性失败和不支持能力使用 Zip domain 的稳定错误码。minizip-ng 错误码保存在 `native_code`；底层 I/O Error 作为 cause 保留，其 native_code/cause 链不得丢失。

## 5. Contract 与负向测试

contract 必须覆盖：不解压 payload 的索引、Store/Deflate 分块读取、名称查找、archive 销毁后的 reader、EOF/CRC、entry 数量与解压预算、取消、底层 I/O cause/native_code、损坏目录、重复名称、路径穿越、加密 entry 与不支持压缩方法。
