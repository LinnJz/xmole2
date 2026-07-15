# 本地参考快照目录

`references/` 整体不提交 Git，也不参与配置、构建、测试或安装。需要研究时按本表恢复到本地；不能确认 commit 的快照只允许阅读，不允许复制实现。

| 本地目录 | 版本/来源 | 许可证 | 状态 |
|---|---|---|---|
| `references/rdocx/` | rdocx 0.1.2，<https://github.com/tensorbee/rdocx> | MIT OR Apache-2.0 | 可按 release/tag 恢复，复用前记录 provenance |
| `references/aspose-cells-foss/` | 26.4.0 目录快照，<https://github.com/aspose-cells-foss/Aspose.Cells-FOSS-for-Cpp> | MIT | 具体 commit 待补；当前只读 |
| `references/aspose-slides-foss/` | main 目录快照，<https://github.com/aspose-slides-foss/Aspose.Slides-FOSS-for-Cpp> | MIT | 具体 commit 待补；当前只读 |

恢复后必须保留上游 LICENSE/notice。参考输出不是规范 oracle；以选定标准版本和 Office/LibreOffice 互操作证据为准。

