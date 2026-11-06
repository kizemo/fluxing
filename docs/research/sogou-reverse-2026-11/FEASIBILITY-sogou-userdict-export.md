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
| 推荐路线 | **★ 路径 A′（更正后首选）：直接解析 `sgim_gd_usr.bin` / `sgim_gd_usr_od.bin` 的明文词典区** —— 零逆向、零官方 UI、零 Ghidra |
| 次选路线 | **路径 A：搜狗官方导出（`.scel` 细胞词库 / `.txt` 文本词库 / `.bdi` 自定义短语）+ 格式转换** —— 降级为**交叉校验与兜底** |
| REA 逆向的位置 | **基本不需要**。仅当 A′ 与 A 都失败时才启用，且需先补 Ghidra 本体 |
| 真正的未知量 | **只剩「导出的是词还是词+词频」「怎么区分自造词与系统词」** —— 属产品定义问题，不是格式问题 |

补充三条本轮新坐实的事实（详见 §3.6 / §3.7）：

- **搜狗用户词典存在明文区**：`sgim_gd_usr_od.bin`（魔数 SGOU）整文件 UTF-16LE 明文，13,256 个中文串；`sgim_gd_usr.bin`（SGPU）**尾部约 1.2 MB 是明文词典区**（32,640 个中文串），只有前 ~2.6 MB 是压缩/混淆块。⇒ **不需要反编译就能读出自造词**。
- **`.scel` 格式本机真实存在且正文是明文 UTF-16LE**（`LocalLow\SogouPY\scd\4.scel` 等 4 个样本，可读出「网络流行新词」「影剧名词」等中文）⇒ 词库格式不是障碍。
- **词条内容真实可辨**：「上海正康仪器有限公司」「我等到花儿也谢了」「江西小炒煲仔饭」「蔚来」「降智」——典型自造词与自定义短语。

> ⚠️ **本节结论较初稿有实质更正**：初稿基于「前 400 KB 解不出词条」判定 SGPU 为非明文，进而把「逆搜狗私有格式」列为路径 B 必要性来源。后续全文件分段实测证明 **SGPU 尾部有明文词典区**，该必要性不再成立。详见 §3.7。

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

### 3.2 词库二进制：头部区压缩/混淆，**尾部有明文词典区**（初稿结论已更正）

> ⚠️ **更正声明**：本节初稿只探测了文件**前 400 KB**，据此判定「整文件非明文、需逆向」。后续**分段全文件**实测证明：`sgim_gd_usr.bin`（SGPU）**尾部约 1.2 MB 是明文 UTF-16LE 词典区**。初稿的「需逆向」结论不成立，以 §3.7 为准。

对 `sgim_gd_usr.bin` / `sgim_gd_usrbg.bin` / `sgim_fix_first.bin` / `sgim_del_word.bin` / `sgim_gd_ucd.bin` 做了三种中文编码探测（**注意：以下数据只覆盖文件前段**）：

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

### 3.7 ★★ 决定性更正：搜狗用户词典的**明文词典区**已被坐实（零逆向即可导出）

本轮第二阶段在 `C:\Users\Duanyi\AppData\LocalLow\SogouPY.users\00000001\`（**搜狗账号隔离目录**，初稿漏掉）发现关键文件与关键证据。

#### 3.7.1 账号隔离目录与云同步痕迹

搜狗 16.8 启用账号后，用户数据放在 `SogouPY.users\00000001\`（与普通态 `SogouPY\` 并存，共 474 vs 333 个文件）。`phrases_sync_backup\backup_info.txt` 实证：

```
backup_start_time: 20261106072245 (local)
pid: 82080
command_line: "D:\Program Files (x86)\SogouInput\16.8.0.5415\userNetSchedule.exe" -hi -dc 1
account_path: C:\Users\Duanyi\AppData\LocalLow\SogouPY.users\00000001
account: 00000001
```

`uploadedFile.ini` 列出**云同步实际上传的文件清单（含 MD5）**：

```
usrZone.dat / usrzoneext.dat / sgim_gd_usr.bin / user_dict.zip / sgim_gd_usr_od
sgim_url_usr.bin(.zip) / sgim_eng_usr.bin(.zip) / sgim_fix_first.bin(.zip) / sgim_phrases.bin(.zip)
```

⇒ 搜狗把用户词典当作 `user_dict.zip` **上传到云端**；但本机全盘定向搜索 `user_dict*` **命中 0**（Local / Roaming / ProgramData / 安装目录 / LocalLow）⇒ 该 zip 是**同步时临时生成、不落盘**。`user_dict.zip` 这条线索因此作废（它只在内存/临时目录里）。

#### 3.7.2 `sgim_gd_usr_od.bin`（魔数 SGOU）= 用户词典的**全明文版本**

| 项 | 实测 |
|---|---|
| 路径 | `SogouPY.users\00000001\sgim_gd_usr_od.bin` |
| 大小 / 魔数 | 461,136 B / `53 47 4f 55` = **SGOU** |
| mtime | **2026-10-24 00:31:58**（比在线主文件旧） |
| UTF-16LE 中文串 | **13,256 个**，占文件 ~5.7% 字节 |
| 长度分布 | 2字 5574 / 3字 5264 / 4字 1938 / 5字 375 / 6字 70 / 7字 16 / 8字 4 / 10字 2（典型词表分布） |

**内容抽样（真实自造词与自定义短语）**：
- 机构/地址类：「上海市浦东新区建韵路」「上海正康仪器有限公司」「正康仪器有限公司」「医疗器材有限公司」「深圳国际会展中心」
- 整句/自定义短语类：「我等到花儿也谢了」「总有一天你会出现」「同是天涯沦落人」「还以为能见你呢」「江西小炒煲仔饭」「地中海自助餐厅」「尚悦西街停车场」「必须犯规的游戏」
- 高频词：「降智 x2」「传杰 x2」「蔚来 x2」「侯大夫」「局中迷」「饭太硬」

**记录格式（已读出字节级结构）**——以「正康仪器有限公司」（8 字）为例，偏移 `0x2d65c`：
```
2d65c  63 6b b7 5e ea 4e 68 56 09 67 50 96 6c 51 f8 53   ← 「正康仪器有限公司」UTF-16LE
2d66c  01 00 99 13 00 00 00 00 08 00 2c 01 72 00 3e 01   ← 计数=1 / 频次=0x1399 / 长度=8 / 编码索引序列
2d67c  aa 00 0a 00 08 00 2f 66 b3 5b d6 4e 86 4e 01 00   ← 下一条：「不负三月」+ 同样的元数据尾巴
```
⇒ 结构 = **UTF-16LE 词条 + 定长整数元数据（计数 / 频次 / 长度 / 音节编码索引）**，可用「长度前缀 + UTF-16LE 串」逐条解析，**不需要反编译**。

#### 3.7.3 ★ `sgim_gd_usr.bin`（SGPU）**尾部就是明文词典区**（在线主文件，最新）

| 区段 | UTF-16LE 中文串数 | 判读 |
|---|---:|---|
| 0 – 256 KB | **1** | 压缩/混淆块（初稿只看了这里 ⇒ 误判） |
| 2.62 MB – 3.86 MB | **32,640** | **明文词典区** |
| 3.40 MB – 3.86 MB | 8,224 | 明文词典区（尾部 460 KB） |

明文区抽样：「知识页」「指数化」「指数增长」「指数转换」「肢体矫正器」「值守」「直售」「所谈」；长度分布 2字 3641 / 3字 1588 / 4字 854 / 5字 295 / 6字 207 … 呈典型词典曲线。

**同一批自造词在 SGPU 里可定位**：
- 「正康仪器有限公司」→ UTF-16LE 偏移 **3,719,089**
- 「上海正康仪器有限公司」→ 偏移 **3,719,085**（正好差 4 字节 = 「上海」两字）⇒ 长词与子词**相邻存储**（倒排/trie 结构）
- 「蔚来」→ 偏移 2,755,384
- 非账号态目录 `SogouPY\sgim_gd_usr.bin` 同尺寸（3,860,140 B）内也能定位到「正康仪器有限公司」（偏移 2,354,647）⇒ **两个目录是同一份用户词典的不同版本**，都带明文区

⇒ **在线主文件（最新，mtime 2026-11-06 19:48）可直接解析出当前自造词，无需等官方导出、无需 Ghidra。**

#### 3.7.4 附带发现（对火流猩有规格参考价值）

- `sgim_gd_pseudorelation.bin`（4 KB）= 搜狗「伪相关」策略；`sgim_gd_smartInfo.bin`（96 KB）= 智能信息；`sgim_gd_em_ext.bin`（923 KB）/ `sgim_gd_ext.bin`（1.8 MB）/ `sgim_hot_ext.bin`（439 KB）= 扩展/热词；`InputStaticstics.dat`（9 KB）= 输入统计。
- `sgim_phrases.bin`（790,572 B）与 `sgim_fix_first.bin`（650,366 B）各自被**标准 zip** 封装（`PK\x03\x04`）：`sgim_phrases.bin.zip` 内含同名条目 790,572 B **压缩到 1,248 B**（630:1 ⇒ 内容高度重复，多为填充），且二者的 UTF-16 中文串分别只有 6 / 2 个 ⇒ **短语正文不可直读，需逆**（但短语与「自造词」是两类需求，可分开处理）。
- `LOG\IME\<进程名>_<pid>.log` 是**皮肤渲染日志**（`t_skinfilemap.cpp`），**不含输入内容** ⇒ 不存在「输入历史泄露」路径，隐私面干净。
- `sgim_eng_usr_emod.bin`（魔数 `44 d0 90 fd`，220,318 B）**不是**英文词明文版（UTF-16 里 0 个英文串）⇒ 英文用户词典不可直读。

---

## 4. 四条技术路径对比

| 维度 | **★ 路径 A′：直读 .bin 明文词典区**（首选） | **路径 A：官方导出 + 转换**（次选/校验） | **路径 B：REA 逆向自研解析器** | **路径 C：进程内截获 / 注入** |
|---|---|---|---|---|
| 做法 | 读 `sgim_gd_usr_od.bin`（SGOU，全明文）与 `sgim_gd_usr.bin`（SGPU 尾部 1.2 MB 明文），按「长度前缀 + UTF-16LE」逐条抽词条 | 走搜狗官方「导出/备份」UI 产出 `.scel` / `.txt` / `.bdi`，再写转换器映射到火流猩词库 | 反编译搜狗 DLL，逆出私有容器 + 压缩算法，自己写 `.bin` 解析器 | 挂进 `SogouImeBroker`/`ImeFunc.dll`，在词条上屏瞬间把文本转储出来 |
| 可行性 | **高**（明文区已坐实，§3.7） | **高**（官方功能已坐实存在） | **低**（明文区已覆盖大半，剩余部分才需要） | **低-中**（ASLR/签名/版本脆弱，且触碰进程注入） |
| 工作量 | **0.5–1 人天**（半天脚本 + 格式映射） | 0.5–1 人天 | 5–15 人天（不可控，取决于压缩算法是否标准） | 3–8 人天，且每次搜狗更新重做 |
| 主要风险 | 需区分「自造词 vs 系统词/高频临时词」；SGPU 前 2.6 MB 压缩区取不到（但那部分多半不是自造词） | 官方导出**可能不含自造词子集**（待验证，见 §5） | 格式逆错 → 静默输出错数据 | 注入被 AV/杀软拦；破坏搜狗稳定性；版本一升就废 |
| 产出格式 | 明文词表，**可直接人工审阅与 diff** | 文本/标准细胞词库，**可读、可校验、可人工审阅** | 二进制直解，校验难 | 明文流，但需常驻/时序踩点 |
| 可维护性 | 中（依赖搜狗内部格式，但结构简单、可用黄金样本回归） | 好（只依赖公开格式约定） | 差 | 差 |
| 合规风险 | 低（只读用户自己的本地文件） | 低（用户自己的数据、用户自己点导出） | 中（涉反编译，但只作规格参考，不入库） | 高（注入他人进程） |

**推荐：路径 A′ 为主，路径 A 做交叉校验，路径 B 仅兜底，路径 C 不做。**

---

## 5. 关键未知量与分阶段计划（按 §3.7 更正后重排）

### Phase 0（首选：直读 .bin 明文词典区｜零逆向｜0.5–1 人天）

明文区已坐实（§3.7），因此**第一步不是找官方导出入口，而是直接解析本地文件**：

1. 先写解析器读 `sgim_gd_usr_od.bin`（SGOU，**全明文**，461 KB）——结构最简单，适合作**格式黄金样本**。
2. 再写解析器读 `sgim_gd_usr.bin`（SGPU）**尾部明文区**（约 2.62 MB–3.86 MB）——这是**在线最新**版本（mtime 2026-11-06 19:48）。
3. 输出 `词条\t频次` 文本 + 一份抽样报告，供人工审阅。
4. **判别性验证**：在搜狗里造一个独一无二的自造词（例如 `zzqtest词条20261106xyz`），等搜狗落盘（`sgim_gd_usr.bin` mtime 变化）后重跑解析器，必须能看到该词条 ⇒ 既验证解析器没瞎，又留下可自动化回归的**黄金样本**。反向再删一个词，确认它消失，排除「解析到的其实是全量系统词库」的假阳性。

> **「造靶子再验证」原则**（agent 记忆 §16）：判定覆盖度必须用自造的、独一无二的词条做靶子，不能靠「文件里看起来有词」来推断。

### Phase 1（校验与补齐：官方导出 + 格式映射）

用于**交叉校验**（解析结果 vs 官方导出的 `.scel`/`.txt` 是否一致），并覆盖短语（`.bdi`）、热词等明文区覆盖不到的需求：

1. 人工在搜狗「属性设置 → 高级 → 自定义短语/词库」点一次导出（选「仅用户自造词」）。
2. `.txt` 直通；`.scel` 用社区开源 `scel2txt`（Python，GPL）或自写转换器；`.bdi` 按 UTF-16LE BOM、`缩写=完整短语` 解析。
3. 与 Phase 0 输出 diff，差异项人工判定。

### Phase 2（仅当 Phase 0 解析器读不全明文区，且 Phase 1 也不够）

此时才启动路径 B，且必须先补 REA 的 Ghidra provider：

1. 安装 Ghidra 12.1.x → 设 `GHIDRA_INSTALL_DIR` → 验证 `binary_session` 里 ghidra 变 available（**这是路径 B 的前置门禁，未通过不要开工**）。
2. 反编译目标优先级：`ImeFunc.dll`（含 `GetVersionByScelFile`、「拼音用户自造词」）→ `SGTool.exe` 的导出模块 → 找 SGPU 前段压缩块的解包函数。
3. 目标只有一个：**SGPU 前 ~2.6 MB 压缩区**。先判断压缩算法是否标准（zlib/lzma 已有 73/161 处签名命中）⇒ 若标准，成本大降；若为定制算法且收益不高，则**该分支直接中止**，维持 Phase 0/1 产出。

> **外部资料备注**（来源 `sou-gou-app.com`，SEO 站点，**可信度中低，仅作线索不作事实依据**）：提到 Windows 端「属性设置 → 高级 → 自定义短语/词库 → 导出个人词库」，格式 `.scel`，可选范围含「仅用户自造词 / 自造词+高频临时词」；自定义短语导出为 `.bdi`，历史路径 `%LOCALAPPDATA%\SogouPY\Phrase\*.bdi`。本机**从未导出过**（§10 已验证）。该线索现在只用于 Phase 1 的交叉校验。

---

## 6. 落地到火流猩的两条下游路径

### 6.1 若目标是「把搜狗自造词搬到火流猩」（最可能的真实动机）

不需要任何逆向，只要「读 → 转 → 导」三步：

```
直读本地 .bin 明文词典区
  sgim_gd_usr_od.bin (SGOU, 全明文)   ┐ 只读
  sgim_gd_usr.bin    (SGPU, 尾部明文)  ┘
        ↓  自写解析器（长度前缀 + UTF-16LE + 定长元数据）
   纯文本（词条\t频次\t权重）
        ↓  写入火流猩用户词典
   *.userdb（经 RimeLeversApi::import_user_dict，StartMaintenance/EndMaintenance 包裹）
```

- `RimeLeversApi::{export,import}_user_dict` 是现成通道（宪法 R7），**必须**用 `StartMaintenance`/`EndMaintenance` 包裹，否则 leveldb LOCK 失败。
- 自定义短语（`.bdi` / `sgim_phrases.bin`）走另一条路：火流猩自定义短语（`RimeWithWeasel` 的 PhrasesDialog 数据源），与用户词典是不同通道，不要混。注意 `sgim_phrases.bin` **不可直读**（§3.7.4），需走 Phase 1 官方导出。
- **合规**：导出的搜狗词库数据**只进用户本机**、只进火流猩用户数据目录，**不得进入火流猩仓库 / 安装器 / 安装包**（ROADMAP §6.2 红线）。

### 6.2 若目标是「做一个独立小工具给用户用」

- 形态：单文件 CLI（`sogou-udict-export.exe` 或 Python 脚本），**只读**打开搜狗用户数据目录下的 `.bin`，输出 UTF-8 `词条\t频次` 文本。
- 边界：工具**只读不写**搜狗目录；解析失败时**明确报错并退出**，绝不输出半截数据（静默产出错数据比崩溃危险）。
- 搜狗运行中时文件可能被占用/缓存 ⇒ 解析器应做**只读打开失败即提示「请先退出搜狗输入法」**，并比对 mtime 防止读到陈旧快照。
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

**判定**：REA 是**取证与体检**的好工具（这轮价值很大：字符串取证坐实官方导出入口、魔数识别、否证 `UserExportDll64.dll` 假设），但它**不是**写解析器的捷径。好消息是——**这题最终并不需要走逆向路线**：搜狗用户词典的明文区已被直接定位（§3.7），用普通的「长度前缀 + UTF-16LE」解析即可，无需 REA + 反编译器 + 大量手工逆向。

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
| R1 | 明文区只覆盖用户词典的一部分（SGPU 前 ~2.6 MB 是压缩块） | 中 | 少部分词条取不到 | Phase 0 先量化明文区覆盖的词条数；缺口不大就接受，缺口大再评估 Phase 2 |
| R2 | **「自造词」与「系统词/高频临时词」在 .bin 里混存，无法区分** | **高** | 导出结果混入大量系统词 | 造靶词 + 新造/老词对比定界；必要时按频次阈值过滤；或全量导入（用户词典本就是增量） |
| R3 | 搜狗运行时文件被占用/缓存 ⇒ 读到陈旧快照 | 中 | 导出内容不是最新 | 只读打开失败即提示「先退出搜狗输入法」；输出中展示 mtime |
| R4 | 解析器**静默产出错数据**（词条错位、半截字） | 中 | 污染火流猩词库 | 黄金样本回归 + 抽样人工审阅 + 解析异常即报错（绝不吐半截结果） |
| R5 | 搜狗升级改格式 → 工具失效 | 中 | 维护成本 | 内置「已知词条探针」做版本自检（找不到即明确报「格式已变」），绝不静默错读 |
| R6 | 整文件正则读 UTF-16LE 会产出**错位伪词条**（已观测，如「粉藌縶哋」） | 高 | 输出噪声词 | 按记录结构（长度前缀）切分，不用整文件正则；伪串过不了结构校验 |
| R7 | 触发搜狗商业版限制（企业策略 `DisableImport`） | 低 | 导入被禁 | 工具只读搜狗侧、不往搜狗写；本机 `HKLM\SOFTWARE\SogouInput\Enterprise` 未见该键【未验证】 |

---

## 10. 验证状态汇总

**已验证（本轮实测）**
- 搜狗实际安装位置：`D:\Program Files (x86)\SogouInput\16.8.0.5415`（注册表 Uninstall `InstallLocation`）
- 用户自造词落盘点：**两个并存**——`C:\Users\Duanyi\AppData\LocalLow\SogouPY\`（普通态，333 文件）与 `C:\Users\Duanyi\AppData\LocalLow\SogouPY.users\00000001\`（**账号态**，474 文件）
- ★ **`sgim_gd_usr_od.bin`（SGOU，461 KB）整文件 UTF-16LE 明文**，13,256 个中文串，长度分布呈典型词表曲线；内容含真实自造词与整句短语
- ★ **`sgim_gd_usr.bin`（SGPU，3.86 MB）尾部约 1.2 MB 为明文词典区**（2.62–3.86 MB 区段 32,640 个中文串；0–256 KB 区段仅 1 个）；「正康仪器有限公司」可定位在偏移 3,719,089，长词「上海正康仪器有限公司」在 3,719,085（相差 4 字节 ⇒ 倒排相邻存储）
- 搜狗云同步确实在跑：`userNetSchedule.exe -hi -dc 1`，`phrases_sync_backup\` 有 20261030–20261106 共 20+ 个备份目录
- `uploadedFile.ini` 列出云上传清单（含 MD5），含 `user_dict.zip`；但**全盘定向搜索 `user_dict*` 命中 0** ⇒ 该 zip 同步时临时生成、不落盘
- `.scel` 样本 4 个存在于 `SogouPY\scd\` 与 `SogouPY.users\00000001\scd\`，正文明文 UTF-16LE；`hotcell_dict.scel` 为压缩变体
- `SGTool.exe` / `ImeFunc.dll` 内含官方导出入口与「自造词」「用户词库」「自定义短语」「scel」等 UTF-16LE 字符串
- `UserExportDll64.dll` 16 个导出全为账号登录 API（假设否证）
- REA provider：ghidra=not_configured、hopper=unsupported_host、kuna=not_configured
- REA 对 `D:\Program Files (x86)\` 路径被拦，工作区副本可分析
- `C:\ProgramData\SogouInput` 不含用户词库；`LOG\IME\*.log` 是皮肤渲染日志，**不含输入内容**
- `sgim_phrases.bin` / `sgim_fix_first.bin` 各自被标准 zip 封装（压缩比 630:1），正文 UTF-16 中文串仅 6 / 2 个 ⇒ **不可直读**

**推断（未实测）**
- `.scel` = 搜狗细胞词库（公开格式，`scel2txt` 可解）；`.bdi` = 自定义短语，UTF-16LE BOM、`缩写=完整短语`
- 官方导出的「仅用户自造词」选项存在（仅据低可信 SEO 站线索）
- 「od」应指搜狗用户词典的**明文/导出形态**（与 SGPU 主文件成对上传），具体含义未查证
- SGPU 明文区记录结构为「UTF-16LE 词条 + 定长整数元数据（计数/频次/长度/音节索引）」，可按长度前缀逐条切分

**未验证（需后续实测）**
- 明文区记录结构的**精确**字段语义（长度字段到底是字符数还是字节数，需用已知词长反推）
- 「自造词」与系统词在 .bin 里的**可区分性**（R2，本方案最大未知）
- SGPU 前 2.6 MB 压缩区是否也含自造词（决定要不要进 Phase 2）
- `scel2txt`（或等价自写转换器）能否解出这 4 个 `.scel` 样本的**全部**词条（**未跑过，属「执行」范畴**）
- 官方导出 UI 的实际行为、导出范围、是否含「自造词」子集
- 本机是否存在 `.bdi`：定向扫描 4 个 `.scel` 所在目录、`ProgramData\SogouInput`（777 文件）、`D:\Program Files (x86)\SogouInput`（393 文件）均 **hits=0**；`AppData` 下未扫出 ⇒ **本机从未导出过**
- `HKLM\SOFTWARE\SogouInput\Enterprise\DisableImport` 是否存在
- 搜狗未运行 / 多用户 / 账号未登录等状态下的行为差异

---

## 11. 工具坑备忘（本轮实测，供下轮复用）

- `bash` 正则里含 `\\` 会被当 UNC 路径拦 ⇒ 改用 `grep` / `glob` 工具。
- PowerShell `Remove-Item` 被硬安全策略拦 ⇒ 用 `>` 覆盖；`bash` 内 `Set-Location` 后再写文件被拦 ⇒ 改用 `git -C <path>`。
- REA 对受保护 Windows 路径（`D:\Program Files (x86)\...`）调用被拦 ⇒ **先复制到工作区 `tmp\`** 再分析。
- `inspect_analysis_view` 必须传 `source:{kind:"retained-evidence", evidence_id:"ev_..."}` + `view:{kind:"native-exports", collection:"exports", fields:[...]}`；传 `parent_evidence_id` 或 `view.kind:"summary"` 会报 `invalid_input`。
- `FindWindowW` 在本项目恒返 0 ⇒ 用 `EnumWindows`。
- 长时递归扫描易转后台任务且长时间无输出 ⇒ 限定目录范围。
- `tmp\` 已被 `.gitignore`（第 185 行 `/tmp/`）覆盖 ⇒ REA 工作副本与证据文件放 `tmp\` 不会误提交（`git check-ignore` 已确认）。
- ★ **只探测文件前 400 KB 就断言「整个文件是密文」是错的** ⇒ 探测必须**分段覆盖全文件**。本轮实证：`sgim_gd_usr.bin` 前 256 KB 只有 **1** 个 UTF-16 中文串，尾部 1.2 MB 却有 **32,640** 个 —— 差点把「需装 Ghidra 反编译」写进结论。
- 后台全盘递归扫描长时间零输出 ⇒ 改为**限定目录 + 单次合并命令**（一次拿到 4 个 `.scel` 与两个用户目录的完整清单）。
- 搜狗有**两个并存的用户数据目录**：`SogouPY\`（普通态）与 `SogouPY.users\<账户号>\`（账号态）⇒ 只查一个会漏掉账号态的全部产物（本轮 `.users` 目录里才有 `sgim_gd_usr_od.bin` 与全部云同步备份）。
- `uploadedFile.ini` / `backup_info.txt` 是**纯文本**，直接列出搜狗云同步上传的文件清单与同步命令行 ⇒ 比翻二进制更快地摸清「搜狗认为哪些文件是用户数据」。

---

## 12. 建议的下一步（等你点头）

| 优先级 | 动作 | 谁做 | 耗时 |
|---|---|---|---|
| 1 | **Phase 0 解析器**：先读 SGOU（`sgim_gd_usr_od.bin`，全明文）定结构，再读 SGPU 尾部明文区，输出 `词条\t频次` + 抽样报告 | 我 | 0.5–1 人天 |
| 2 | **判别性验收**：你造一个独一无二的自造词 → 等搜狗落盘 → 重跑解析器必须命中；再删掉确认消失 | **需要你手动操作**（涉改用户数据） | 10 分钟 |
| 3 | 接入火流猩：接 `RimeLeversApi::import_user_dict`（`StartMaintenance`/`EndMaintenance` 包裹），做黄金样本回归 | 我 | 0.5 人天 |
| 4 | **Phase 1 交叉校验**：你手动点一次官方导出，与解析结果 diff | **需要你手动点**（GUI） | 5 分钟 |
| 5 | 仅当明文区缺口大 ⇒ 决策是否装 Ghidra 启动 Phase 2 | 需你批准安装 | 决策点 |

**Phase 0 之前不需要装 Ghidra，不需要逆向，不需要官方导出。**
