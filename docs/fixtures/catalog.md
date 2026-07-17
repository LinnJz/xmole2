# Fixture Catalog

实际 fixture 位于不提交 Git 的本地 `testdata/` 或临时证据目录 `out/evidence/`。只有来源、授权、SHA-256、格式/dialect、预期能力和隐私状态全部明确的 fixture 才能进入自动测试或受控制品库。

| 本地路径 | 类型 | 来源/授权 | 敏感信息 | 状态 |
|---|---|---|---|---|
| `testdata/legacy-v0/unpacked-docx/docx_no_pwd/` | 解包 DOCX | 旧测试目录；原始来源待补 | 待复核 | Quarantined |
| `testdata/legacy-v0/unpacked-docx/毕业设计论文/` | 解包 DOCX | 旧测试目录；来源和授权待补 | 可能包含个人/学校信息 | Quarantined，禁止 CI/分发 |
| `out/evidence/compoundfilereader-upstream/test/data/unicode.dat` | CFB v3 Unicode directory 互操作 fixture；SHA-256 `67BE3CF47AAAE38755A3882A72FE6253D34868B748D08C689CD36072DF5A2633` | Microsoft CompoundFileReader `test/data/unicode.dat`，<https://github.com/microsoft/compoundfilereader>，文件引入 commit `d0f3914ac1b1134387a077d3c76ee1d8eb756be1`，核对 HEAD `4ddb0602600833bc925d76c0f0382ba88c1c9f60`；MIT | 未发现已知敏感信息；仅含测试目录/流名与 payload | Local-only manual interoperability；不提交 payload，不作为单一规范 oracle；预期 hardened parser 成功打开并验证 directory ordering |

Quarantined 内容只能用于本地人工调查。未来 CI fixture 必须由测试代码合成，或从受控位置下载并在使用前校验本目录记录的哈希。
