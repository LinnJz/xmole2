# Fixture Catalog

实际 fixture 位于不提交 Git 的本地 `testdata/`。只有来源、授权、SHA-256、格式/dialect、预期能力和隐私状态全部明确的 fixture 才能进入自动测试或受控制品库。

| 本地路径 | 类型 | 来源/授权 | 敏感信息 | 状态 |
|---|---|---|---|---|
| `testdata/legacy-v0/unpacked-docx/docx_no_pwd/` | 解包 DOCX | 旧测试目录；原始来源待补 | 待复核 | Quarantined |
| `testdata/legacy-v0/unpacked-docx/毕业设计论文/` | 解包 DOCX | 旧测试目录；来源和授权待补 | 可能包含个人/学校信息 | Quarantined，禁止 CI/分发 |

Quarantined 内容只能用于本地人工调查。未来 CI fixture 必须由测试代码合成，或从受控位置下载并在使用前校验本目录记录的哈希。

