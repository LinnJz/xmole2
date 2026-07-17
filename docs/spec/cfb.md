# CFB 容器规范

状态：Normative Baseline
适用 target：`xmole2::cfb`

## 1. 边界与依赖

CFB 层只依赖 `xmole2::base`、`xmole2::io` 和标准库，负责 `[MS-CFB]` 物理 header、DIFAT/FAT、directory、MiniFAT、root mini-stream mapping 与 regular/mini stream 顺序读取。它不解释 DOC/XLS/PPT 记录、OLE property set、加密协议或产品格式检测。

public header 只出现 xmole2 自有类型与标准库类型。CFB 不依赖平台 OLE handle、参考项目 API、异常错误模型或整文件物化。

## 2. Header

实现以 `[MS-CFB]` v20240423 §2.2 为格式基线，必须验证 signature、Header CLSID、major version、byte order、sector shift、mini-sector shift、reserved bytes、mini-stream cutoff、声明 sector 计数、chain 起点与物理文件对齐。

Major Version 只接受 3 和 4。Minor Version 对 major 3 和 4 都 **SHOULD** 为 `0x003E`，不是 major-specific 的 `0x0003`/`0x0004`；reader 接受并保留其他 16-bit minor value，不把 advisory value 当作损坏。Header CLSID、byte order、sector shift、reserved bytes 和 mini-stream cutoff 的 MUST 约束仍是致命格式校验。

`read_header()` 只读取固定 512-byte header，不读取后续 sector。返回的 `CompoundFileHeader::minor_version` 必须是输入中的原始值。

## 3. DIFAT/FAT 与 sector 角色

DIFAT/FAT reader 必须验证 header DIFAT、扩展 DIFAT chain、FAT sector 顺序、声明容量、padding、role marker、sector 边界、chain cycle 和表项对物理文件的覆盖。FAT/DIFAT sector 不得同时承担 directory、MiniFAT、root mini stream 或用户 stream 角色。

所有 sector/offset/容量运算必须防溢出，并受 `max_cfb_sector_count`、`max_cfb_stream_chain_length` 与 `max_memory_bytes` 约束。

## 4. Directory

Directory reader 按物理 directory ID 返回 owning `DirectoryEntry` 数组，并验证 chain、128-byte entry 布局、类型、UTF-16 名称、引用边界、单一入边、cycle 和从 root 的可达性。

字段兼容与 MUST/SHOULD 处理如下：

- version 3 stream/root 的 Stream Size 使用低 32 位；高 32 位即使非零也必须接受并忽略，归一化后的值不得超过 `0x80000000`；version 4 保留完整 64 位。
- stream CLSID、creation time、modified time 和 child ID 的 MUST 约束是致命错误；State Bits 仅 SHOULD 为零，reader 接受并保留非零值。
- root entry 的 creation time 必须为零；modified time 可以为零或有效 FILETIME；root directory 自身没有 siblings，因此其 color 可以为 red 或 black。
- unallocated entry 除 left/right/child ID 必须为 `NOSTREAM` 外，其余 116 bytes 必须全部为零。
- 每个 storage 的 sibling-tree root 必须为 black，red node 的直接 sibling child 不得为 red。
- 每个 sibling tree 必须按 `[MS-CFB]` §2.6.4 的名称比较严格递增：先比较名称的 UTF-16 code-unit 数量；长度相同时，对每个 UTF-16 code unit 应用 Unicode simple uppercase 后逐项比较。代理 code unit 保持原值，不把代理对解码成 scalar value 后再变换；比较结果相等表示同一 storage 内名称重复，必须拒绝。

名称变换固定使用 Unicode 17.0.0 `UnicodeData.txt` 第 12 字段 `Simple_Uppercase_Mapping` 的 BMP 映射；输入 SHA-256 为 `2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C`。私有生成表必须由 `cmake/GenerateCfbUnicodeSimpleUppercase.cmake` 从该输入可复现地产生并保留 Unicode License V3；它不是 locale、平台 API 或完整字符串 case folding。`[MS-CFB]` v20240423 明确允许实现采用更新的 Unicode 版本。

Directory index 仍不宣称为完整 CFB conformance certificate；实现按 `[MS-CFB]` §4.1 在打开阶段执行本节列出的安全相关验证。调用方不得根据数组物理顺序推断层级或名称顺序。

## 5. MiniFAT 与 stream reader

MiniFAT reader 必须验证 MiniFAT chain、root mini-stream FAT mapping、sector role 冲突、mini-sector 边界、cycle、共享与声明长度；映射阶段不得读取 root mini-stream payload。

`CfbStreamReader` 是 move-only 单线程顺序 reader。`open()` 读取并验证 allocation/directory 元数据，但不读取所选 payload；reader 持有独立的底层随机 reader，因此创建它的 `SourceLease` 销毁后仍可继续读取稳定 source。

小于 header cutoff 的非空 stream 使用 MiniFAT/root mapping，大于等于 cutoff 的 stream 使用 FAT。`read()` 最多返回 destination 大小和声明剩余大小中的较小值，零表示 EOF；不得暴露最后 sector 的 padding。所有 stream chain 必须匹配声明长度、不得循环、共享或占用 metadata sector。

## 6. 资源、取消、错误与性能

所有公开长路径接受同一 `OperationContext`，在循环中检查取消，并应用 input、single-resource、sector、directory-entry、stream-chain 与 memory budget。预算耗尽返回 CFB domain 的 `ResourceLimitExceeded`，不得伪装为普通损坏。

底层 I/O failure 必须作为 cause 保留，native_code 同时保留在 cause 和 CFB wrapper。progress phase 使用 `cfb.header`、`cfb.difat`、`cfb.fat`、`cfb.directory`、`cfb.minifat`、`cfb.root_mini_stream`、`cfb.stream.validate` 与 `cfb.stream.read`。

当稳定 `SourceLease` 的缓存长度覆盖目标范围、但底层 source 在 `read_exact()` 中提前返回 EOF 时，CFB wrapper 必须把该失败分类为当前格式阶段的损坏：header 使用 `InvalidHeader`，DIFAT/FAT 使用 `InvalidSectorTable`，directory 使用 `InvalidDirectory`，MiniFAT 使用 `InvalidMiniFat`，stream payload 使用 `InvalidStream`；底层 I/O `UnexpectedEndOfFile` 仍保留为 cause。其他 I/O 失败使用 `ReadFailed`，取消与资源限制继续映射为对应 CFB domain code。

`cfb.stream.validate` 的 completed/total 只统计实际需要 allocation-chain 验证的非空 stream entry，不得使用物理 directory ID 或全部 directory slot 数量代替。

不得在没有代表性 fixture 和可复现 benchmark 的情况下，以预计算完整 stream sector/offset chain 替换当前按需映射；额外缓存必须同时证明吞吐收益和 memory-budget 代价。

## 7. Contract 与负向测试

contract 必须覆盖：v3/v4 header、advisory minor value 保留、Header MUST 字段、DIFAT/FAT、directory chain、v3 Stream Size 高 DWORD 兼容与 2 GiB 上限、root/stream/unallocated entry 字段约束、directory graph/color、Unicode simple-uppercase sibling 排序、大小写不敏感重复名、代理 code-unit 恒等、MiniFAT/root mapping、regular/mini lazy payload、reader 逃逸生命周期、EOF、共享/短链、预算、I/O 与纯计算验证阶段的运行中取消、progress 精确 completed/total、完整 I/O cause/native_code、缓存长度内提前 EOF 的阶段错误映射、以虚拟 source 验证的 64-bit 最大 sector 地址边界、跨 FAT sector table 边界的 stream chain，以及 destination 恰好等于剩余 stream 大小时的读取与 EOF。
