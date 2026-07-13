# Office 统一架构设计会议决议纪要

日期：2026-07-15  
状态：Navigation Summary（非事实源）  
形式：`grill-me` 逐项设计审查  
范围：xmole2 1.0 前 Words、Cells、Slides 的统一工程架构

本文是三十三次设计问答的导航摘要。完整且不可由本文件改写的会议事实源是 `2026-07-15-office-architecture-all-sessions.md`；`docs/spec/` 是规范性要求。摘要与事实源不一致时先以事实源纠正摘要和 spec；spec 的后续方向性变更仍必须通过 ADR。

## Q01：是否追求 Aspose API 兼容

决议：核心引擎不追求 Aspose 源码 API 兼容。若未来需要兼容层，应作为独立 compatibility facade，不能污染核心领域模型、类型系统和生命周期。

规范落点：`architecture.md` §1。

## Q02：“统一 Office”的边界

决议：统一打开、保存、格式检测、诊断等生命周期能力，但不统一三类内容模型。内容操作分别进入 WordDocument、Workbook、Presentation。

规范落点：`architecture.md` §1、§5；`public-api.md` §1。

## Q03：高保真往返

决议：高保真同格式往返是硬要求。模型采用“语义对象 + 未识别内容保留”的混合方式，不能因未实现某项能力而丢弃扩展节点、关系、嵌入对象或其他合法内容。

规范落点：`fidelity.md` §1–§3。

## Q04：大型 XLSX 与内存模型

决议：大型 XLSX 必须具有受控内存占用。OPC 只建立 Part 索引并按需读取；未修改 Part 直拷；XML Part 按需解析并记录 dirty；worksheet 提供行级流式读取；完整随机编辑是显式高内存模式。DOCX/PPTX 默认按 Part 懒加载，不要求细粒度流式解析。

规范落点：`architecture.md` §4.1；`public-api.md` §6；`testing.md` §5。

## Q05：真实模块边界、CFB 与加密位置

决议：拆成多个独立 CMake target，即使最终提供统一安装包和 umbrella 组件。整体不是线性链：ZIP/OPC 与 CFB/OLE2 是并行容器；crypto 是底层 primitive；office-encryption 负责编排 CFB 外壳中的 OOXML 加密；DOC/XLS/PPT codec 直接依赖 CFB，并处理自身的格式专用加密记录。

旧 `common/ole2` 应表达为 `cfb` 容器；各产品的 `ole2` 目录应表达为 DOC/XLS/PPT binary codec。领域模型不得依赖 DrawingML 或 OOXML 存储结构。

规范落点：`architecture.md` §2–§5。

## Q06：原格式保存与跨格式转换

决议：`save()` 沿用来源 codec；`save_as()` 只改变位置、选项或同 codec 变体；`convert_to()` 显式切换 codec 并先生成 ConversionReport。存在损失时调用者必须显式接受。新建文档显式选择输出格式，便捷 API 可以使用现代默认格式。

规范落点：`public-api.md` §8；`fidelity.md` §3–§4。

## Q07：领域对象所有权

决议：WordDocument、Workbook、Presentation 独占内部状态。Paragraph、Cell、Slide 等是稳定逻辑 ID 的轻量句柄；小型纯值类型按值传递。禁止公开内部容器、XML 节点和裸指针。修改必须经过受控 API 并自动标记相关语义节点与 Part。大型文档只通过显式 `clone()` 深复制。

规范落点：`public-api.md` §2–§3。

## Q08：句柄失效契约

决议：Document move、容器扩容、插入/删除其他节点不影响句柄。删除目标节点后返回 StaleHandle；文档销毁后返回 DocumentExpired；clone 使用新的 ID 空间。内部 state 可使用 shared/weak 生命周期控制，但这不代表线程安全。

规范落点：`public-api.md` §2–§3。

## Q09：线程模型

决议：同一文档实例不支持并发读写，按线程约束对象使用；不同文档实例可以并行处理。全局常量与已构建 registry 必须只读或线程安全。

规范落点：`public-api.md` §9。

## Q10：ABI 承诺

决议：1.0 前不承诺跨版本 C++ ABI，只承诺版本管理下的源码 API。顶层对象使用 PImpl；每个 target 具有导出宏和 public/private headers。插件和语言绑定使用版本化 C ABI；1.0 前再确定长期 ABI 策略。

规范落点：`public-api.md` §10；`architecture.md` §7。

## Q11：OOXML 代码生成

决议：采用混合生成。QName、命名空间、枚举映射、属性元数据、简单类型校验和重复 XML dispatch 自动生成；领域模型、跨 Part 关系、继承、样式、公式、版式和编辑语义手写。生成结果提交仓库，普通构建不要求生成器。输入 schema 固定版本并记录 Transitional、Strict、Microsoft 扩展来源。生成的 CT/ST 层不得成为公共领域 API。

规范落点：`architecture.md` §6。

## Q12：DrawingML 共享范围

决议：共享 Color、Fill、LineStyle、Geometry、Effect、Transform2D、Theme 等稳定值对象，以及内部解析、序列化、扩展保留和主题解析。宿主句柄分别为 WordShape、SpreadsheetShape、SlideShape；anchor/cell-anchor/shape-tree 分开。Chart 可共享内部模型，但关系、位置、数据来源和保存策略归宿主产品。禁止大型 DrawingObject 继承树；通用访问只使用 variant、capability 或窄接口。

规范落点：`architecture.md` §5。

## Q13：渲染与导出

决议：渲染/导出是独立可选模块。Words layout、Cells render、Slides render 分开；共享 graphics 提供字体发现、fallback、text shaping、画笔、路径、颜色管理和图片解码；PDF、HTML、SVG、raster 是输出后端。领域模型与 codec 不反向依赖渲染器。office facade 可以发现能力，但不得强制携带渲染依赖。

规范落点：`architecture.md` §3.1。

## Q14：保存冲突策略

决议：禁止静默丢失。Preserve 是同格式默认，无法确认安全时返回 FidelityConflict；Strict 遇到未知或降级风险即失败；BestEffort 允许降级但必须逐项写入 SaveReport。`convert_to()` 先 preflight，损失需要显式接受。未受影响的未知内容原样保留且不制造噪声诊断。

规范落点：`fidelity.md` §3–§4。

## Q15：原子修改与事务

决议：单个便捷操作原子完成并保持模型有效；复杂编辑使用 EditSession，commit 时验证引用、关系、格式和保真冲突，失败回滚。dirty part、诊断和 generation 在提交时统一更新。回滚创建的句柄变为 StaleHandle。事务 journal 为未来 undo/redo 留出接缝，但 1.0 不承诺通用历史栈。

未决细节：会议要求句柄在事务期间具有明确的可见性规则，但没有选定“事务内独占可见”“提交前快照”或其他具体模型。实现 EditSession 前必须通过 ADR 决定；不得把任一方案写成既有共识。

规范落点：`public-api.md` §7。

## Q16：Transitional、Strict 与扩展

决议：三者从架构上是一等输入，能力可以分阶段实现。文档和 Part 记录 dialect/profile；领域模型不写死 2006 URI；同格式保存维持原 dialect；新建默认 Transitional，可显式 Strict。Microsoft 扩展已理解部分进入模型，未知部分进入保真层。格式验证器独立于容错解析器；“可打开”不等于“符合标准”。

规范落点：`fidelity.md` §5。

## Q17：是否统一 OPC 与 CFB 的语义接口

决议：不统一。只共享 ByteSource、ByteSink、RandomAccessReader、资源预算和临时存储。ZipArchive 与 CompoundFile 保持独立；OpcPackage 只建立在 ZIP/PartStore 上；binary codec 依赖 CompoundFile；加密 OOXML 解密后再进入 OPC。用户统一入口使用 OfficeDocument/OfficeLoader，OpcPackage 保持内部协议概念。

规范落点：`architecture.md` §4；`public-api.md` §1。

## Q18：懒加载输入的生命周期

决议：文档可以持续持有 SourceLease。`open(path)` 保证打开时内容的稳定视图并使用适当共享模式，避免无必要阻止读取或重命名；`detach()` 物化所需数据并释放原源；`materialize()` 加载语义内容但不要求所有大型二进制 Part 入内存；`from_buffer()` 默认取得所有权，借用重载在类型中表达生命周期；原路径保存采用临时文件与原子替换。

规范落点：`public-api.md` §6；`security.md` §6。

## Q19：Codec registry

决议：使用显式 registry，初期 codec 以普通库链接，不立即做动态插件。禁止全局静态注册。用户只链接需要的产品；缺失 codec 返回 CodecUnavailable。测试可以注入 mock codec、故障 Source 和受限能力 codec。未来 C ABI 插件注册到同一 runtime。提供包含当前构建全部内置 codec 的便捷 runtime，但核心依赖保持显式。

规范落点：`architecture.md` §7；`testing.md` §2、§4。

## Q20：修改 XML Part 的字节保真

决议：未修改 Part 字节直拷；dirty XML Part 中未触碰子树尽可能以 source span/patch writer 保持原字节；必须重写的子树保证语义、未知属性/元素和相对顺序，不承诺字节相同；新建输出确定性。签名覆盖字节的任何变化必须报告。

规范落点：`fidelity.md` §1–§2、§6。

## Q21：签名、宏、ActiveX 与嵌入对象

决议：初期以检测和高保真保留为主。office-signatures 检测签名、覆盖范围和验证状态，重签名以后实现；VBA 作为受保护内容保留而不执行；ActiveX/OLE 作为 opaque object 保留并提供元数据/提取接口。破坏签名或关联关系必须进入 SaveReport，Preserve 无法安全处理时失败。可选模块依赖容器和领域模型，核心 OPC/CFB/codec 不依赖脚本执行环境。

规范落点：`architecture.md` §3.2；`fidelity.md` §6；`security.md` §4。

## Q22：Excel 公式计算边界

决议：cells-model 保存公式文本、AST、依赖和缓存值； XLSX/XLS codec 负责公式序列化、共享公式、数组公式和缓存结果；cells-calc 独立负责函数、依赖图、增量重算和循环引用。没有计算引擎仍可读写公式；公式改变但未重算时设置 workbook calculation flags。外部链接、宏函数和未实现函数产生结构化诊断。

规范落点：`architecture.md` §3；`fidelity.md` §7。

## Q23：错误模型

决议：统一 Error 信封而非巨大中央枚举；Result 使用 `std::expected<T, Error>`。Error 包含 domain、domain-local code、severity、message、DocumentLocation、cause、native_code。可恢复问题进入 DiagnosticBag，失败进入 Error。普通文件错误不用异常；调用者不得解析错误文本。内存耗尽等进程级异常策略单独决策。

规范落点：`public-api.md` §4。

## Q24：仓库与发布

决议：当前采用 monorepo、多 target、多独立安装组件。共享层可以原子演进，产品具有独立测试、benchmark 和依赖闭包。发布 xmole-words、xmole-cells、xmole-slides、xmole-office。源码依赖不得依靠仓库相对路径，以保留将来按团队、版本或授权需求拆仓的能力。

规范落点：`architecture.md` §8；`testing.md` §4。

## Q25：开发顺序

决议：先完成 DOCX/XLSX/PPTX 三格式最小垂直切片，再深入 Words。切片覆盖 runtime 分派、懒加载 Part 索引、三个顶层对象、核心属性、最小编辑、同格式保存、未修改直拷、dirty 重写、CapabilityReport/SaveReport/诊断和真实样本往返。

规范落点：`testing.md` §2；`migration.md` M3。

## Q26：旧测试与 fixture

决议：旧 API 测试归档到 deprecated，不进入默认 CI；fixture 统一进入 testdata 并登记来源、授权、格式、能力和敏感信息。经确认的协议行为改写成新 contract test；旧实现不是规范 oracle。新架构从第一条切片同步建设 unit、contract、round-trip、fuzz、benchmark。

规范落点：`testing.md`；`migration.md` §1；`testdata/manifest.md`。

## Q27：参考项目

决议：rdocx、Aspose.Cells FOSS、Aspose.Slides FOSS 是只读参考，不参与默认构建/安装/CI。保留 LICENSE、版本、URL 和时间。新实现不得依赖其内部头文件或命名空间。复用前检查许可证并记录 provenance；测试场景可以重写但输出不是 oracle。

规范落点：`migration.md` §1；`references/README.md`。

## Q28：第三方类型是否进入公共 API

决议：禁止。absl、pugixml、minizip-ng、fmt、frozen 等只能作为 PRIVATE 实现依赖。公共 API 只使用 xmole2 和经评估的标准库类型。ZIP/XML/crypto/容器通过内部 adapter 隔离；扩展点使用 xmole2 自有窄接口。

规范落点：`architecture.md` §8；`public-api.md` §10。

## Q29：平台与依赖管理

决议：从架构第一天保持 Windows/Linux/macOS 可移植。Windows x64 首要，尽早补 Linux，I/O 稳定后接入 macOS。路径、Unicode、共享打开、临时文件、原子替换位于 io 适配层；公共 API 使用 `std::filesystem::path` 或 xmole2 路径抽象，不公开 Win32 handle；字节序、整数宽度、未对齐访问显式处理；CFB/ZIP/XML/codec contract 跨平台运行。

依赖优先标准库或 vcpkg 可用包。1.0 前使用 vcpkg；1.0 后提供固定版本 FetchContent 或 submodule 路径，不能强制开发者使用 vcpkg。

规范落点：`architecture.md` §8–§9；`testing.md` §4。

## Q30：不可信输入与 ResourceBudget

决议：所有 Office 输入默认不可信，全部解析链共享 ResourceBudget。预算覆盖输入/解压、Part/stream、XML、CFB、关系/路径、KDF、图片/字体/嵌入对象、worksheet、公式、内存、临时磁盘、诊断与取消。默认值安全且实用，可信批处理必须显式放宽。

规范落点：`security.md` §1–§2。

## Q31：外部资源

决议：默认禁止隐式网络、进程和任意文件访问。只能通过 ExternalResourceResolver；默认拒绝。自定义 resolver 受 scheme、根目录/域名、大小、超时、重定向、内容类型和 ResourceBudget 限制。未解析关系保存时保留。渲染或计算缺失外部资源时产生结构化诊断，不得静默阻塞。

规范落点：`security.md` §3。

## Q32：同步、取消与异步包装

决议：核心操作同步，不内置线程池。长操作接受 OperationContext，统一承载 ResourceBudget、CancellationToken、ProgressSink、DiagnosticSink、ExternalResourceResolver。调用者负责调度；取消返回 Cancelled 并清理临时输出。未来协程/异步适配器放在独立 integration target。

规范落点：`public-api.md` §5；`architecture.md` §7.1。

## Q33：规范治理

决议：spec 是软件生命周期的信息基线。architecture、public-api、fidelity、security、testing、migration 分别承担明确职责；重大变化先写 ADR，再同步 spec 和测试。CI 检查依赖方向、第三方 public-header 泄漏和 forbidden include。实现与 spec 不一致默认是实现缺陷，禁止为迎合代码静默修改规范。

规范落点：`docs/spec/README.md`、`development-process.md`、`docs/adr/`。

## 未决事项

三十三项会议没有决定 `OfficeDocument` 的具体 C++ 表示是 `std::variant`、类型擦除 facade，还是另一种 discriminated holder。该选择涉及公共 API 和 ABI，必须在实现 office-runtime 前通过独立 ADR 决定。

会议同样没有决定 EditSession 中未提交修改对事务外句柄的可见性模型；该选择必须在实现事务前通过独立 ADR 决定。
