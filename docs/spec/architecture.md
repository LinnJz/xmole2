# xmole2 架构规范

状态：Normative Baseline  
版本：0.1  
适用范围：xmole2 1.0 之前的全部设计与实现

本文使用“必须”“禁止”“应当”“可以”表达不同强度的约束。实现与本文不一致时，默认视为实现缺陷。重大方向调整必须先提交 ADR，再同步修改规范和测试。完整变更流程见 `development-process.md`。

## 1. 目标与非目标

xmole2 是面向 DOCX/DOC、XLSX/XLS、PPTX/PPT 的跨平台 C++23 Office 文档处理库。它统一文档的打开、保存、格式检测、诊断、资源控制和转换流程，但不建立跨 Word、Spreadsheet、Presentation 的统一内容对象模型。

核心引擎不追求 Aspose 源码 API 兼容。未来如需兼容层，必须作为独立 facade，不得反向污染领域模型。

高保真同格式往返是硬性架构要求；跨格式转换允许有损，但必须显式预检并报告。

## 2. 总体依赖结构

依赖只允许从上层指向下层：

```text
office facade
├── words:  model ← docx codec / doc codec ← optional layout
├── cells:  model ← xlsx codec / xls codec ← optional calc/render
└── slides: model ← pptx codec / ppt codec ← optional render

OOXML codecs → drawingml → ooxml-core → xml
OOXML codecs → opc → zip → io → base
Binary codecs → cfb → io → base
Binary codecs → crypto → base
office-encryption → cfb + crypto
renderers → product model + graphics → exporters
```

禁止形成以下依赖：

- Words、Cells、Slides 互相依赖。
- 领域模型依赖 codec、XML、OPC、CFB、渲染器或第三方库。
- OPC 依赖 CFB 或 Office 加密。
- DrawingML 宿主放置语义进入共享值对象。
- 核心 codec 依赖 PDF、SVG、raster、HTML 或计算引擎。
- public header 包含参考实现或第三方项目的内部头文件。

## 3. 规范 target

| CMake target | 职责 | 允许的直接依赖 |
|---|---|---|
| `xmole2::base` | Error、Result、诊断、ID、资源预算、取消、基础值类型 | 标准库 |
| `xmole2::io` | ByteSource、ByteSink、随机访问、SourceLease、临时文件、原子替换 | base |
| `xmole2::xml` | 安全解析、token/source span、patch writer、确定性 writer | base、io |
| `xmole2::crypto` | AES、RC4、Hash、KDF 等密码学 primitive/port | base、io |
| `xmole2::zip` | ZIP 物理容器与 entry 流 | base、io |
| `xmole2::cfb` | CFB/OLE2 storage、stream、sector 和目录 | base、io |
| `xmole2::opc` | Part、Content Types、Relationships、PartStore | base、io、xml、zip |
| `xmole2::office-encryption` | ECMA-376 Agile/Standard 加密封装 | base、io、crypto、cfb |
| `xmole2::ooxml-core` | QName/profile、MC、扩展保留、共享 OOXML 机制 | base、xml |
| `xmole2::drawingml` | DrawingML 值对象与内部 codec | base、xml、ooxml-core |
| `xmole2::words-model` | WordDocument 与 Word 领域对象 | base |
| `xmole2::words-docx` | WordprocessingML/OPC codec | words-model、opc、ooxml-core、drawingml |
| `xmole2::words-doc` | DOC 二进制 codec | words-model、cfb、crypto |
| `xmole2::cells-model` | Workbook、worksheet、cell、formula AST | base |
| `xmole2::cells-xlsx` | SpreadsheetML/OPC codec | cells-model、opc、ooxml-core、drawingml |
| `xmole2::cells-xls` | BIFF/XLS codec | cells-model、cfb、crypto |
| `xmole2::cells-calc` | 依赖图与可选公式计算引擎 | cells-model |
| `xmole2::slides-model` | Presentation 与 slide 领域对象 | base |
| `xmole2::slides-pptx` | PresentationML/OPC codec | slides-model、opc、ooxml-core、drawingml |
| `xmole2::slides-ppt` | PPT 二进制 codec | slides-model、cfb、crypto |
| `xmole2::office-runtime` | OfficeRuntime、格式检测、codec registry | base、io |
| `xmole2::words` / `cells` / `slides` | 对应产品 codec 的聚合组件 | 对应 model 与 codecs |
| `xmole2::office` | 包含三个产品的便捷聚合组件 | office-runtime、words、cells、slides、office-encryption |

### 3.1 渲染与导出目标

可选渲染目标包括 `xmole2::graphics`、`xmole2::words-layout`、`xmole2::cells-render`、`xmole2::slides-render` 以及 PDF、HTML、SVG、raster exporter。它们不得成为核心模型或 codec 的反向依赖。

`graphics` 只提供跨产品的字体发现与 fallback、text shaping、画笔、路径、颜色管理和图片解码。Word 分页、Excel 网格打印、PowerPoint 画布布局分别属于三个产品 renderer，禁止建立统一 Office layout engine。Exporter 消费 renderer/graphics 输出；领域模型和 codec 不依赖任何 exporter。

`office-facade` 可以通过 capability 查询发现当前 runtime 中可用的 renderer/exporter，但不得因此让核心 `xmole2::office-runtime` 或文档处理组件强制链接渲染依赖。无渲染需求的用户必须能够只安装、链接文档模型与 codec 组件。

### 3.2 签名与主动内容目标

`xmole2::office-signatures` 是可选模块，消费 crypto、OPC/CFB 和必要的领域定位接口，负责签名检测、覆盖范围和验证；核心容器与 codec 不依赖它。VBA、ActiveX 和嵌入 OLE 的解析/提取能力位于可选模块，依赖相应容器与领域模型；核心 OPC、CFB 和 codec 禁止依赖脚本执行环境。

## 4. 容器、格式与加密管线

OPC 与 CFB 是并行且语义不同的容器，不实现统一 `Package` 接口。两者只共享底层 I/O、资源预算和临时存储设施。

```text
普通 OOXML: Source → ZIP → OPC → DOCX/XLSX/PPTX codec → 领域模型
加密 OOXML: Source → CFB → OfficeEncryption → ByteSource → ZIP → OPC → codec
旧版格式:   Source → CFB → DOC/XLS/PPT codec → 领域模型
```

`crypto` 不知道 Office 格式；`office-encryption` 知道加密协议但不解析 OPC。旧版格式的加密记录由相应 binary codec 处理并复用 `crypto`。

### 4.1 懒加载与大型工作簿

OPC 打开阶段只建立 Part 索引，不得默认将全部 entry 解压到内存。Part 至少具有 Raw/Lazy、Parsed、Dirty 状态；未修改 Part 保存时从 SourceLease 直接复制。DOCX 与 PPTX 默认按 Part 懒加载。

XLSX worksheet 必须提供行级流式 reader，其内存随读取窗口而不是 worksheet 总大小增长。需要完整随机访问和任意结构编辑时，调用者必须显式进入高内存 materialized edit mode；该模式仍受 ResourceBudget 限制。流式模式与随机编辑模式是不同能力契约，禁止由实现根据文件大小静默切换语义。

## 5. 领域模型与 codec

`WordDocument`、`Workbook`、`Presentation` 是三个独立顶层模型。DOC/DOCX 共享 WordDocument，XLS/XLSX 共享 Workbook，PPT/PPTX 共享 Presentation。codec 负责存储表示与领域模型之间的映射，并维护格式专用的保真 sidecar。

DrawingML 只共享稳定值对象和内部解析能力，例如 Color、Fill、LineStyle、Geometry、Effect、Transform2D、Theme。宿主对象必须分为 WordShape、SpreadsheetShape、SlideShape；Word anchor、Excel cell anchor 和 PowerPoint shape tree 禁止进入共同基类。

Chart 可以共享内部 chart model，但关系、放置、数据来源与保存策略由各产品负责。

禁止建立大型 `DrawingObject` 继承树。确需跨宿主检查共享能力时，只能使用 discriminated variant、capability 查询或职责单一的窄接口，不能以共同基类掩盖 placement 和 ownership 差异。

## 6. 代码生成

采用混合生成：生成 QName、命名空间、枚举映射、属性元数据、简单类型校验和 XML dispatch 表；手写领域模型、跨 Part 关系、继承、样式、公式、版式和编辑语义。生成输出提交仓库，普通构建不运行生成器。

生成器输入必须固定 schema 版本并记录 Transitional、Strict 与 Microsoft 扩展来源。生成的 `CT_*`/`ST_*` 层不得成为公开领域 API。

## 7. Runtime 与扩展

Codec 使用显式 registry 注册，不依赖全局静态初始化：

```cpp
auto runtime = OfficeRuntime::builder()
  .with_words()
  .with_cells()
  .with_slides()
  .build();
```

初期 codec 是普通链接库。未来动态插件通过版本化 C ABI 注册到同一 runtime；1.0 前不承诺 C++ ABI 稳定。

用户只链接所需产品组件；registry 缺少匹配 codec 时返回 `CodecUnavailable`。测试必须能够显式注入 mock codec、故障 Source 和受限能力 codec。聚合 `xmole2::office` 提供包含当前构建全部内置 codec 的便捷 runtime，但 `xmole2::office-runtime` 本身不得隐式拉入三个产品。

`OfficeDocument` 的具体 C++ 表示尚未在本次会议确定。`std::variant`、类型擦除 facade 或其他 discriminated holder 的选择必须在实现 office-runtime 前由 ADR 决定；在此之前禁止把任一方案写成既定公共 API。

### 7.1 异步集成

核心库保持同步且可取消，不内置线程池。未来协程、future 或框架异步包装必须位于独立 integration target，只负责调度同步核心操作，不能复制解析、保存或诊断逻辑。

## 8. 工程与发布

项目采用 monorepo、多 target、多安装组件。实现必须遵循以下逻辑源码布局；如物理路径需要调整，必须保持相同 public/private 与 target 边界：

```text
libs/<module>/include/xmole2/<module>/  # public headers
libs/<module>/src/                     # private implementation
tests/{unit,contract,roundtrip,...}/
testdata/
docs/{spec,adr}/
references/                            # 不参与构建
deprecated/                            # 不参与构建
```

第三方依赖必须为 `PRIVATE`，公共 API 只出现 xmole2 与经评估的标准库类型。具体版本、职责、查找和引入规则由 `dependencies.md` 规定。实现应优先选择标准库提供的跨平台能力；标准库不足时，优先选择当前 vcpkg `x64-windows-static-md` 可提供的包。1.0 前以 vcpkg manifest/version baseline 管理依赖；1.0 后同时提供固定版本的 FetchContent 或 submodule 获取路径，不能强制使用者采用 vcpkg。

absl 容器、pugixml 节点、minizip-ng 对象、fmt formatter、frozen 容器以及其他第三方类型不得出现在 public header、公开成员或函数签名中。`fmt` 仅用于内部消息构造；XML、ZIP、crypto 和容器实现通过 xmole2 自有 port/adapter 隔离。

更换 ZIP、XML、crypto 或容器实现不得要求使用者修改业务代码。确需向使用者开放的扩展能力必须定义 xmole2 自有窄接口，禁止透传第三方对象。

发布组件至少包括 `xmole-words`、`xmole-cells`、`xmole-slides` 和聚合的 `xmole-office`。

三个产品必须分别拥有独立 contract test、benchmark 和可审计的依赖闭包。模块间依赖通过安装 target 或清晰的 CMake target 表达，禁止依赖仓库相对源码路径；该约束用于保留将来按团队、版本或授权模式拆仓的能力。

## 9. 平台

架构从第一天保持 Windows、Linux、macOS 可移植。Windows x64 是首要开发平台，随后接入 Linux x64，公共 I/O 稳定后接入 macOS。平台相关文件共享、Unicode、临时文件和原子替换只存在于 io 适配层。

公共路径 API 使用 `std::filesystem::path` 或 xmole2 自有路径抽象，禁止暴露 Win32 handle。二进制 codec 必须显式处理字节序、整数宽度、窄化与未对齐访问，不能依赖目标机器布局。CFB、ZIP、XML 和六个格式 codec 的 contract test 必须跨目标平台运行。

## 10. 当前架构处置

原 `src/` 仅具有目录级划分，全部源码仍聚合为单一 target；`common/oxml` 已包含 Word 专属类型，Facade 公开 OPC 与原始 XML，无法满足领域隔离、懒加载和受控修改要求。因此原实现冻结于 `deprecated/legacy-v0/`，只作为第三方库用法、算法和性能经验参考。
