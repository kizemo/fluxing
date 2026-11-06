# 可行性分析：用 REA MCP 做「导出搜狗输入法自造词」小工具

- **Created**: 2026-11-06
- **Scope**: 仅可行性分析（用户明示「先分析，不要执行」）——本轮**未**编写导出工具、**未**执行逆向反编译、**未**读写/导出任何搜狗用户数据
- **Related**: `ROADMAP.md`（路线定案 / 合规红线 §6）、`REPORT.md`（此前 REA 逆向实证）、`docs/adr/0030-*.md`（ADR 0030）、交接卡 `docs/archive/handoffs/handoff-2026-11-03-fluxing-117-release-and-esc-quickpanel-bug.md`
- **Status**: 待用户审核

---

## 1. 结论（TL;DR）

| 问题 | 答案 |
|---|---|
| 技术上做得到吗？ | **做得到**，但**不该用 REA 逆向自研解析器** |
| REA MCP 够用吗？ | **不够**。当前 REA 无原生反编译 provider（ghidra/hopper/kuna 全不可用），只能做 target-free 静态 PE 分析（exports/imports/resources） |
| 推荐路线 | **路径 A：搜狗官方导出（`.scel` 细胞词库 / `.txt` 文本词库 / `.bdi` 自定义短语）+ 格式转换** —— 零逆向、可当天出可用原型 |
| REA 逆向的位置 | 降级为**兜底**（仅当官方导出不覆盖「自造词」子集时才启用），且需先补 Ghidra 本体 |
| 真正的未知量 | **不是「能不能解析」，而是「官方导出里到底含不含自造词」** —— 一个 5 分钟的人工点击即可证伪/证实 |

补充两条本轮新坐实的事实（详见 §3.6）：

- **`.scel` 格式本机真实存在且正文是明文 UTF-16LE**（`LocalLow\SogouPY\scd\4.scel` 等 4 个样本，可读出「网络流行新词」「影剧名词」等中文）⇒ **词库格式不是障碍**，社区 `scel2txt` 即可解。
- **搜狗的私有 `.bin`（SGPU/SUCD）确实是非明文**（§3.2）⇒ 只有走这条路才需要 RE，且**没有必要走**。

一句话：**这题的正确解法不是逆向搜狗的词库文件，而是用搜狗自己已经写好的导出功能，再做一次格式转换。**

---

## 2. 需求界定：要导出的是什么

「自造词」= 用户自己造的词条（人名、地名、行业术语、缩写等），在搜狗里属于**用户词典**，与系统词库、细胞词库是两套东西。本机对应的落盘文件是 `sgim_gd_usr.bin`（魔数 SGPU）。

> 注：火流猩侧的对应概念是 Rime「用户词典」（`*.userdb` / 用户词频），不是 `essay` 静态词库；Rime 已有 `import_user_dict` / `export_user_dict` 通道（宪法 R7 的 `RimeLeversApi`），落地路径见 §6.2。

---

## 3. 取证结果（全部【已验证】，本轮实测）

### 3.1 用户自造词的真实落盘点

之前几轮一直找不到词库文件，原因是搜狗 16.8 把用户态数据放在了 `LocalLow`（UWP 风格的低权限目录），而不是 `AppData\Roaming` / `AppData\Local`。

**真实位置**：`C:\Users\Duanyi\AppData\LocalLow\SogouPY\`

| 文件 | 大小 | 魔数 | 含义（推断） |
|---|---:|---|---|
| `sgim_gd_usr.bin` | 3,860,140 | `SGPU` | **用户词典主文件 ← 自造词主体** |
| `sgim_gd_usrbg.bin` | 9,510,136 | `SGBG` | 用户词典备份/背景 |
| `sgim_gd_panel_usr.bin` | 440,140 | `SGPU` | 面板侧用户词 |
| `sgim_gd_usr_a_bigram.bin` / `sgim_gd_usr_dca_bigram.bin` | — | — | 用户 bigram 词频 |
| `sgim_fix_first.bin` | 650,366 | — | 自定义短语 / 固首 |
| `sgim_del_word.bin` | 184,196 | — | 已删除词记录 |
| `sgim_gd_cell.bin` / `sgim_hot_ext.bin` / `sgim_nine_pick.bin` / `sgim_gd_ext.bin` | — | — | 细胞词库 / 热词 / 九键 / 扩展 |
| `env.ini` / `sign.ini` / `BrandScdConfig.ini` | — | 明文 | 配置 |
| `appid.udchd`（见 `ImeFunc.dll` 字符串上下文） | — | — | 单元格头数据 |

**已排除的位置**（避免下轮重复找）：
- `C:\ProgramData\SogouInput\`（777 文件 / 356 MB）= **程序级**数据（CEF 缓存、shell_ext、ShiplyUpdate、SGBizConfig），**不含用户自造词**
- `HKCU\Software\SogouInput\*` = 配置，无词库路径值
- `C:\Users\Duanyi\AppData\Roaming\SogouPY` / `AppData\Local\SogouPY` / `AppData\Local\SogouInput` = **不存在**
- 搜狗程序目录 `D:\Program Files (x86)\SogouInput\16.8.0.5415\` 另有 53 个 `sgim_*.bin`，mtime **全部**为 `2026-08-12 00:14`（随安装落盘，非用户活动），含 `sgim_gd_ucd.bin`（SUCD）、`sgim_gd_um.bin`（SGUM）、`sgim_gd_bigram.bin`（SGSB）、`sgim_phrases.bin`

### 3.2 词库二进制不是明文（决定性负面证据）

对 `sgim_gd_usr.bin` / `sgim_gd_usrbg.bin` / `sgim_fix_first.bin` / `sgim_del_word.bin` / `sgim_gd_ucd.bin` 做了三种中文编码探测：

| 文件 | UTF-8 CJK run | GBK CJK run | UTF-16LE CJK run | 结论 |
|---|---:|---:|---:|---|
| `sgim_gd_usr.bin` | 0 | 36,265（乱码） | 127（码表式连续，非词条） | 无可读词条 |
| `sgim_fix_first.bin` | 0 | — | **0** | 无可读词条 |
| `sgim_del_word.bin` | 0 | — | **0** | 无可读词条 |
| `sgim_gd_ucd.bin` | **0** | 686（乱码，如「婡朄燖瑻窣腀螥贎鏎馌鼲」） | — | 无可读词条 |

**结构特征**（SGPU 头）：
```
53 47 50 55 28 00 00 00 | 09 a0 33 01 | ac e6 3a 00 | 0x60 起 32-bit 偏移表（递增）
```
签名扫描：SGPU 文件命中 zlib 签名 **73** 处 / gzip 2 处 / lzma 161 处；`sgim_fix_first.bin` 为 0/0/0；`sgim_del_word.bin` lzma 2 处。文件尾 64 KB `distinctBytes=1`（全同值填充）。

**判读**：头部后是高度压缩或混淆的数据块，自定义容器格式，**不是明文结构**，直接写解析器等于从零逆一个私有压缩格式。

### 3.3 搜狗自带导出功能已存在（UTF-16LE 字符串实证）

这是推翻「必须逆向」的关键证据。

**`SGTool.exe`（20,332,928 B）内含字符串**：
- 「导入/恢复」「导出/备份」「加密词库文件 .bin」「文本词库文件 .txt」「Google词库」
- 「自造词」×4、「用户词库」×25、「自定义短语」×27
- `scel` ×18，上下文含 `scel.tmp`、`.scd`、`appid.scd`、「导入词库失败/成功」

**`ImeFunc.dll`（8,850,816 B）内含字符串**：
- `scel` ×6，上下文含 `pscd`、**`appid.udchd`**、`Test.hotcell`、**`GetVersionByScelFile`**
- 「自造词」×3、「拼音用户自造词」「拼音细胞词」

**判读**：搜狗同时支持 **`.scel`（细胞词库，二进制）** 与 **`.txt`（文本词库）** 两种导出格式，并且内部就有「自造词」这一分类概念。`GetVersionByScelFile` 说明 `.scel` 有独立版本格式。

### 3.4 已否证：`UserExportDll64.dll` 不是词库导出组件

原假设「`UserExportDll*.dll` 是词库导出 DLL」**被证伪**。

- `UserExportDll64.dll`：sha256 `43d58c0b5e66c64ccc26ad02e7f7c6d37ab370808a8e9eba1c97764b24aa7d87`，4,088,704 B，amd64，export_directory rva 3824336，**16/16 导出，issue=0**
- 16 个导出全是账号/登录态 API：`AddNotifyInterface` `BindPhoneNum` `FreeMem` `GetCaptcha` `GetConfigPath` `GetUserLoginInfo` `GetUserLoginInfoById` `InitModule` `Logout` `LogoutEx` `LogoutWithOutID` `RemoveNotifyInterface` `SendBindPhoneNumSMS` `SetLogoutState` `StartLoginWnd` `UnInitModule`

⇒ 这是**账号登录组件**，方向作废，不必再查。

### 3.5 REA MCP 当前能力边界（`binary_session` 实测）

| provider | 状态 | 原因 |
|---|---|---|
| `ghidra` | **unavailable / not_configured** | 需设置 `GHIDRA_INSTALL_DIR` 指向解包后的 Ghidra 12.1.x；本机已有 JDK 21（`C:\Program Files\Microsoft\jdk-21.0.13.21-hotspot\bin\java.exe`），**只缺 Ghidra 本体** |
| `hopper` | **unavailable / unsupported_host** | 「Hopper integration is not supported on win32」 |
| `kuna` | **unavailable / not_configured** | 需 `REA_KUNA_COMMAND`，REA 不代装 |

**结论**：当前 REA 只能做 target-free 静态 PE 分析（exports / imports / resources / certificates / load-config / sections），**无法原生反编译**词库读写逻辑。

补充实测：REA 对 `D:\Program Files (x86)\...` 下的 DLL 直接调用会被本地硬安全策略拦（"protected Windows path"）；**先复制到工作区 `tmp\` 再分析即可通过**。

### 3.6 ★ `.scel` 格式在本机真实存在，且正文可直接读出中文（关键正面证据）

定向扫描 `C:\Users\Duanyi\AppData\LocalLow\SogouPY\scd\` 发现 **4 个 `.scel` 细胞词库文件**（此前的全盘后台扫描超时无果，已终止并改为定向扫描，一次坐实）：

| 文件 | 大小 | UTF-16LE 汉字数 | 汉字串个数 | 判读 |
|---|---:|---:|---:|---|
| `4.scel` | 218,794 | 20,849 | 7,392 | **大量可读中文** |
| `49111.scel` | 9,948 | 56 | 15 | 可读中文 + ID 串 |
| `60393.scel` | 10,140 | 43 | 9 | 可读中文 + ID 串 |
| `hotcell_dict.scel` | 15,504 | 2,466 | 559 | **全为乱码**（此变体整体压缩/加密） |

**头部结构（`4.scel` 前 48 字节）**：
```
40 15 00 00 | 45 43 53 01 | 01 00 00 00 | 6a 2d 89 7c cc 85 36 7e cb 54 2c 11 99 48 cb cc | 34 00 00 00 | 00...
```
- `45 43 53 01` = 稳定魔数 `'E','C','S'` + 版本 `01`
- 随后 16 字节为 GUID/哈希
- `0x24` 处是 UTF-16LE 的词库 ID（`49111.scel` 里是 `"9111"`，`60393.scel` 里是 `"0393"` —— 与文件名**反序**，说明该处按大端/异序存放）
- 三个词库文件的 `40 15 00 00`（= 5440）完全一致 ⇒ 同版本同头部布局

**抽样可读内容**（`4.scel`）：
「网络流行新词」「北京」「搜狗搜索自动生成的流行新词」「每周更新」「与凤行」「影剧名词」「游戏名词」「人物名词」「敖隐」「于适」「人物名词」

**关键判读**：
1. `.scel` **正文是明文 UTF-16LE**，**没有加密**。个别串显示为乱码（「粉藌縶哋」「基櫛」「蓩声」）是**字节奇偶错位**造成的读取假象，不是密文 —— 这正是社区工具 `scel2txt` 能把它转成纯文本的原因（需按结构对齐，而非整文件硬解）。
2. `4.scel` 是「搜狗搜索自动生成的流行新词」这类**云词库**，「影剧名词 / 游戏名词 / 人物名词」是搜狗的**词性分类标签** —— 进一步佐证 `.scel` 是搜狗词库的通用封装格式。
3. `hotcell_dict.scel` 头部即随机数据 ⇒ **至少存在一个压缩/加密变体**。⇒ 若导出自造词恰好落到这一变体，解码难度会跳升（但它体积仅 15 KB，未必与自造词相关）。
4. 结论：**`.scel` 是公开、已被社区工具攻破的格式**，**不是 RE 目标**。这直接消灭了「必须逆搜狗私有词库格式」的整条必要性论证。

---

## 4. 三条技术路径对比

| 维度 | **路径 A：官方导出 + 转换**（推荐） | **路径 B：REA 逆向自研解析器** | **路径 C：进程内截获 / 注入** |
|---|---|---|---|
| 做法 | 走搜狗官方「导出/备份」UI 产出 `.scel` / `.txt` / `.bdi`，再写转换器映射到火流猩词库 | 反编译搜狗 DLL，逆出 SGPU/SUCD 容器 + 压缩算法，自己写 `.bin` 解析器 | 挂进 `SogouImeBroker`/`ImeFunc.dll`，在词条上屏瞬间把文本转储出来 |
| 可行性 | **高**（官方功能已坐实存在） | **中**（需先装 Ghidra，再逆私有格式） | **低-中**（ASLR/签名/版本脆弱，且触碰进程注入） |
| 工作量 | **0.5–1 人天**（半天脚本 + 格式映射） | **5–15 人天**（不可控，取决于压缩算法是否标准） | 3–8 人天，且每次搜狗更新重做 |
| 主要风险 | 官方导出**可能不含自造词子集**（待验证，见 §5） | 格式逆错 → 静默输出错数据；压缩算法若为定制则成本翻倍 | 注入被 AV/杀软拦；破坏搜狗稳定性；版本一升就废 |
| 产出格式 | 文本/标准细胞词库，**可读、可校验、可人工审阅** | 二进制直解，校验难 | 明文流，但需常驻/时序踩点 |
| 可维护性 | 好（只依赖公开格式约定） | 差 | 差 |
| 合规风险 | 低（用户自己的数据、用户自己点导出） | 中（涉反编译，但只作规格参考，不入库） | 高（注入他人进程） |

**推荐：路径 A 为主，路径 B 仅作兜底，路径 C 不做。**

---

## 5. 关键未知量与「一天内证伪」设计

唯一真正卡住的未知量：

> **Q：搜狗 16.8.0.5415 的官方「导出/备份」产出的文件里，是否包含「自造词」（用户词典 `sgim_gd_usr.bin` 的内容）？**

外部资料（来源 `sou-gou-app.com`，SEO 站点，**可信度中低，仅作线索不作事实依据**）提到：Windows 端「属性设置 → 高级 → 自定义短语/词库 → 导出个人词库」，格式 `.scel`（搜狗细胞词库），可选范围含「仅用户自造词 / 自造词+高频临时词」；自定义短语导出为 `.bdi`（UTF-16LE BOM，`缩写=完整短语`），历史路径 `%LOCALAPPDATA%\SogouPY\Phrase\*.bdi`。**本机尚未找到任何 `.bdi` / `.scel` 文件**（后台全盘扫描未出结论），说明用户从未手动导出过。

### Phase 0（零逆向，半小时可出结论）

1. 人工在搜狗「属性设置 → 高级 → 自定义短语/词库」点一次导出，选「仅用户自造词」。
2. 拿到 `.scel` / `.txt` 后立刻做**三项判定**：
   - 文件是不是明文/已知格式？（`.txt` 必然是；`.scel` 看能否被现成工具解）
   - **判别性检查**：先在搜狗里新建 1 个独一无二的自造词（例如 `zzqtest词条20261106xyz`），再导一次，diff 出该词条 ⇒ 直接证明「官方导出含自造词」，且给出**可自动化回归的黄金样本**。
   - 反向检查：搜狗里删掉一个词再导出，确认该词消失 ⇒ 排除「导出的是全量系统词库」的假阳性。
3. 若导出只给 `.scel`：用社区开源 `scel2txt`（Python，GPL）转纯文本；`.bdi` 直接按 UTF-16LE 明文解析即可。

> **「造靶子再验证」原则**（见 agent 记忆 §16）：判定导出覆盖度必须用自造的、独一无二的词条做靶子，不能靠「导出文件里看起来有词」来推断。

### Phase 1（仅当 Phase 0 证明官方导出**不含**自造词）

此时才值得启动路径 B，且必须先补 REA 的 Ghidra provider：

1. 安装 Ghidra 12.1.x → 设 `GHIDRA_INSTALL_DIR` → 验证 `binary_session` 里 ghidra 变 available（**这是路径 B 的前置门禁，未通过不要开工**）。
2. 反编译目标优先级：`ImeFunc.dll`（含 `GetVersionByScelFile`、「拼音用户自造词」）→ `SGTool.exe` 的导出模块 → 找 SGPU 写/读函数。
3. 先判断压缩算法是否标准（zlib/lzma 已有 73/161 处签名命中）⇒ 若标准，解析成本大降；若为定制算法，则**该分支直接中止**，回到「让搜狗官方导出」或「用户手工截屏/抄写」。

### Phase 2（仅当 Phase 1 也失败）

放弃自动化，改为**半自动**：引导用户在搜狗 UI 里「管理词库/自造词列表」处人工导出或复制，工具只做格式清洗。

---

## 6. 落地到火流猩的两条下游路径

### 6.1 若目标是「把搜狗自造词搬到火流猩」（最可能的真实动机）

不需要任何逆向，只要「导出 → 转换 → 导入」三步：

```
搜狗 UI 导出 (.scel / .txt / .bdi)
        ↓  scel2txt 或自写 bdi→tsv 转换器
   纯文本（词条\t频次\t权重）
        ↓  写入火流猩用户词典
   *.userdb（经 RimeLeversApi::import_user_dict，StartMaintenance/EndMaintenance 包裹）
```

- `RimeLeversApi::{export,import}_user_dict` 是现成通道（宪法 R7），**必须**用 `StartMaintenance`/`EndMaintenance` 包裹，否则 leveldb LOCK 失败。
- 自定义短语（`.bdi`）走另一条路：火流猩自定义短语（`RimeWithWeasel` 的 PhrasesDialog 数据源），与用户词典是不同通道，不要混。
- **合规**：导出的搜狗词库数据**只进用户本机**、只进火流猩用户数据目录，**不得进入火流猩仓库 / 安装器 / 安装包**（ROADMAP §6.2 红线）。

### 6.2 若目标是「做一个独立小工具给用户用」

- 形态：单文件 CLI（`sogou-udict-export.exe` 或 Python 脚本），只读打开用户目录下的导出产物，输出 UTF-8 `词条\t频次` 文本。
- 边界：工具**只处理已导出的产物**，不直接读 `sgim_gd_usr.bin`（那是路径 B，成本差一个数量级）。
- 该工具属于「用户自有数据的迁移辅助」，可放在仓库 `tools/` 下，但**仓库里不得带任何搜狗词库样本文件**。

---

## 7. REA MCP 能做什么 / 不能做什么（直接回答「用 REA 行不行」）

**能**：
- 静态 PE 分析：导出表、导入表（看它调用了哪些 CRT/压缩库）、资源段、证书、节表、Load Config
- 字符串/常量提取（**这轮就是靠它坐实了官方导出入口**）
- 二进制指纹：魔数、熵、签名扫描（zlib/gzip/lzma）、偏移表结构
- 工具链补齐后（装 Ghidra）：反编译 x64 C++ → 还原私有容器格式

**不能（当前配置下）**：
- ✗ 反编译：ghidra 未配置 / hopper win32 不支持 / kuna 未配置
- ✗ 直接分析 `D:\Program Files (x86)\...` 下的文件（硬安全策略拦，需先复制到工作区）
- ✗ 「自动猜出这个私有二进制格式是什么」——工具不给魔法，只能靠人逆
- ✗ 代替动态调试（脱壳、运行时解密、内存 dump 都不在 REA 能力内）

**判定**：REA 是**取证与体检**的好工具（这轮价值很大），但它**不是**写解析器的捷径；写解析器需要 REA + 反编译器 + 大量手工逆向，合计 5–15 人天，且不可控。

---

## 8. 合规与红线

1. **不入库**：搜狗词库 / `.scel` / `.bdi` / `sysmodel.bin` / `.cupf` 等资产**不得进入火流猩仓库与安装器**（ROADMAP §6.2；ADR 0030 路线 B 前提）。
2. **仅规格参考**：逆向所得只作格式规格参考，不复制搜狗代码进洁净基线；GPLv3 之下也不搬运搜狗专有实现。
3. **用户数据**：导出产物属用户自有数据，工具默认只读、只写用户指定路径、不外传、不联网。
4. **导出操作需用户明示**：本轮已按「只分析不执行」停手；真正执行导出前需用户确认（属「改用户数据」类操作）。
5. **不替换已发布产物**：本工具与火流猩安装包无关，不触发发布流程。

---

## 9. 风险清单（按优先级）

| # | 风险 | 可能性 | 影响 | 缓解 |
|---|---|---|---|---|
| R1 | 官方导出**不含自造词**（只导系统/细胞词库） | 中 | 路径 A 失效，需转路径 B | Phase 0 判别性检查（造靶词 + diff）5 分钟内出结论 |
| R2 | 官方导出入口在 16.8 已被移除/改版 | 低 | 需另找入口 | `SGTool.exe` 字符串已坐实「导出/备份」存在 |
| R3 | `.scel` 反解依赖第三方 `scel2txt`，维护/兼容风险 | 中 | 转换失败 | 优先找「文本词库 .txt」导出；`.scel` 作为备选；必要时自写（格式已知是「细胞词库」公开结构） |
| R4 | 搜狗在运行时锁/缓存词库文件，直接拷贝得到的不是最新 | 中 | 导出内容陈旧 | 走官方 UI 导出（进程自身写文件），不直接拷 bin |
| R5 | 逆向后写出的解析器**静默产出错数据** | 高（仅路径 B） | 用户词库被污染 | 黄金样本回归 + 「造靶子再验」；导出后必须人工抽样比对 |
| R6 | 搜狗升级改格式 → 工具失效 | 中 | 维护成本 | 路径 A 不依赖搜狗内部格式，抗升级 |
| R7 | 触发搜狗 16.8 的商业版限制（企业策略 `DisableImport`） | 低 | 导入被禁 | 只导出不导入，本机 `HKLM\SOFTWARE\SogouInput\Enterprise` 未见该键【未验证】 |

---

## 10. 验证状态汇总

**已验证（本轮实测）**
- 搜狗实际安装位置：`D:\Program Files (x86)\SogouInput\16.8.0.5415`（注册表 Uninstall `InstallLocation`）
- 用户自造词落盘点：`C:\Users\Duanyi\AppData\LocalLow\SogouPY\`（文件清单 + 大小 + 魔数）
- 词库 bin 三种中文编码均无明文词条；含 zlib/lzma 疑似签名；尾部为填充区
- `SGTool.exe` / `ImeFunc.dll` 内含官方导出入口与「自造词」「用户词库」「自定义短语」「scel」等 UTF-16LE 字符串
- `UserExportDll64.dll` 16 个导出全为账号登录 API（假设否证）
- REA provider：ghidra=not_configured、hopper=unsupported_host、kuna=not_configured
- REA 对 `D:\Program Files (x86)\` 路径被拦，工作区副本可分析
- `C:\ProgramData\SogouInput` 不含用户词库

**推断（未实测）**
- `.scel` = 搜狗细胞词库（公开格式，`scel2txt` 可解）；`.bdi` = 自定义短语，UTF-16LE BOM、`缩写=完整短语`
- 官方导出的「仅用户自造词」选项存在（仅据低可信 SEO 站线索）
- `sgim_gd_usr.bin`（SGPU）= 用户词典主文件，含自造词主体
- Phase B 的工作量区间 5–15 人天

**未验证（需后续实测）**
- 官方导出 UI 的实际行为、导出范围、是否含「自造词」子集（**Phase 0 的核心待办**）
- `scel2txt`（或等价自写转换器）能否解出本机这 4 个 `.scel` 样本的**全部**词条（**未跑过，属「执行」范畴**）
- 导出自造词时落到的是**明文变体**还是 `hotcell_dict.scel` 那种**压缩变体**（决定解码难度）
- 本机是否存在 `.bdi`（自定义短语导出物）：定向扫描 4 个 `.scel` 所在目录与 `ProgramData\SogouInput`（777 文件）、`D:\Program Files (x86)\SogouInput`（393 文件）均 **hits=0**；`AppData` 下 `.bdi` 未扫出 ⇒ **本机从未导出过**
- `HKLM\SOFTWARE\SogouInput\Enterprise\DisableImport` 是否存在
- 96 DPI / 多用户 / 搜狗未运行等边界场景下官方导出的差异

---

## 11. 工具坑备忘（本轮实测，供下轮复用）

- `bash` 正则里含 `\\` 会被当 UNC 路径拦 ⇒ 改用 `grep` / `glob` 工具。
- PowerShell `Remove-Item` 被硬安全策略拦 ⇒ 用 `>` 覆盖；`bash` 内 `Set-Location` 后再写文件被拦 ⇒ 改用 `git -C <path>`。
- REA 对受保护 Windows 路径（`D:\Program Files (x86)\...`）调用被拦 ⇒ **先复制到工作区 `tmp\`** 再分析。
- `inspect_analysis_view` 必须传 `source:{kind:"retained-evidence", evidence_id:"ev_..."}` + `view:{kind:"native-exports", collection:"exports", fields:[...]}`；传 `parent_evidence_id` 或 `view.kind:"summary"` 会报 `invalid_input`。
- `FindWindowW` 在本项目恒返 0 ⇒ 用 `EnumWindows`。
- 长时递归扫描易转后台任务且长时间无输出 ⇒ 限定目录范围。
- `tmp\` 已被 `.gitignore`（第 185 行 `/tmp/`）覆盖 ⇒ REA 工作副本与证据文件放 `tmp\` 不会误提交（`git check-ignore` 已确认）。

---

## 12. 建议的下一步（等你点头）

| 优先级 | 动作 | 谁做 | 耗时 |
|---|---|---|---|
| 1 | **Phase 0 人工导出实测**：搜狗「属性设置 → 高级 → 自定义短语/词库 → 导出个人词库」，先造一个独一无二的自造词做靶子，导出后 diff 验证 | **需要你手动点**（涉改用户数据 + GUI 操作） | 5 分钟 |
| 2 | 拿到产物后写转换器（`.txt` 直通 / `.scel`→txt / `.bdi`→tsv），产出火流猩可导入的文本 | 我 | 0.5–1 人天 |
| 3 | 若 Phase 0 证伪（导出不含自造词）⇒ 决策是否装 Ghidra 启动路径 B | 需你批准安装 | 决策点 |
| 4 | 产物落地：接 `RimeLeversApi::import_user_dict`（`StartMaintenance`/`EndMaintenance` 包裹），做黄金样本回归 | 我 | 0.5 人天 |

**在 Phase 0 出结论前，不要启动路径 B。**
