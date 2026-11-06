# 0037. 导入搜狗输入法用户词库：托盘入口挂在部署器子命令，解析器只做签名扫描，拼音取随包 cn_dicts

- **状态**: Accepted
- **日期**: 2026-11-06
- **决策者**: Fluxing maintainers
- **相关**: ADR 0030（继续火流猩 fork，不做搜狗逆向重写）；ADR 0012 / 0028（IPC 只增不改、薄 TSF 壳）；constitution P2 / P5 / R7 / P8；ROADMAP §6.2（第三方资产红线）；lessons-learned L10（leveldb 维护锁）

## Context

用户从小狼毫迁到火流猩后，**自造词全丢**。搜狗输入法没有官方「导出用户词库」入口
（本机从未导出过，`.scel` 只覆盖细胞词库、不含用户自造词），而**用户自造词是搜狗
私有二进制格式**。于是产生一个真实需求：**把搜狗用户词库搬进火流猩**。

三条硬约束同时压着这件事：

1. **合规红线（ROADMAP §6.2 / P8）**：搜狗词库、`.cupf`、`sysmodel.bin`、`.scel`、
   搜狗私有 `.bin` 等资产**不得进仓库、不得进安装器**，**逆向所得只作规格参考，
   不复制代码/数据入洁净基线**。⇒ 功能必须是「**运行时读取用户本机已有的搜狗文件**」，
   而不是「把词库打包进火流猩」。
2. **IPC 四边同步成本**（CLAUDE.md §2）：动 `include/WeaselIPCData.h` 就要 server /
   TSF / deployer 三端同步 + 一个 ADR + 一个 `test/TestWeaselIPC/*` 测试。本需求
   **没有任何跨进程数据要传** ⇒ 不应该为了它开一条新 IPC 协议。
3. **librime leveldb 独占锁**（L10）：`RimeLeversApi::{export,import}_user_dict`
   **必须**夹在 `StartMaintenance()` / `EndMaintenance()` 之间，否则 leveldb LOCK 失败。

同时，搜狗用户词库的**私有容器格式**没有公开文档。本机实测两个文件：

- `sgim_gd_usr.bin`（3,860,140 B，头部魔数 `SGPU`）
- `sgim_gd_usr_od.bin`（461,136 B，头部魔数 `SGOU`）

分段探全文件后发现：**不是全密文**。尾部存在大段明文 UTF-16LE 词典区
（`SGOU` 整文件明文，扫出 13,256 条中文串；`SGPU` 尾部明文区约 32,640 条）。
可见真实词条含单字（如「体验版」）⇒ 词条长度下限 1 字。

实测坐实的记录布局：

```
[u16 A = len+2][u16 B = len][词条: B 字节, UTF-16LE, 全 CJK]
[u16 flag=1][u16 seq][u32 保留][u16 K][u16 × K 音节索引]...
```

其中**音节是 u16 索引，不是明文拼音**；文件内**没有** ASCII 拼音表
⇒ **拿不到搜狗自己的拼音**，必须另找拼音来源。

还有一条容易踩空的事实：搜狗**同时存在两个用户目录** ——
`%LOCALAPPDATA%\..\LocalLow\SogouPY\`（普通态）与
`%LOCALAPPDATA%\..\LocalLow\SogouPY.users\<数字>\`（账号态，多一层）。
只查前者会**漏掉账号态的全部产物**。

## Decision

### D1 功能落在 `WeaselDeployer.exe`，托盘菜单只做「拉起子命令」

- 托盘右键菜单新增一项「导入搜狗用户词库」（简 / 繁 / 英**三个语言变体同步**，
  加速键 `&G`），菜单 ID `ID_WEASELTRAY_SOGOU_IMPORT = 40022`。
- 该菜单项只做一件事：`execute(dir / L"WeaselDeployer.exe", L"/sogou")` ——
  与既有 `/deploy` / `/dict` / `/sync` **完全同款**。
- `WeaselDeployer.exe` 新增 `/sogou` 子命令 ⇒ `Configurator::SogouImport()` ⇒
  模态对话框 `SogouImportDialog`（IDD `IDD_SOGOU_IMPORT = 32789`）。

**为什么不是新开 IPC 协议**：本需求没有跨进程数据流；部署器**已经**持有 Rime
levers API 与 `MaintenanceGuard`，托盘**已经**有拉起部署器的通道。挂在这里 ⇒
**不动 `include/WeaselIPCData.h` ⇒ 不触发四边同步**。

**为什么不是放进 TSF 前端**：TSF 是热路径，且 P2 红线禁止在
`ITfTextInputProcessor::*` 回调内做阻塞 I/O；词库解析要读两个 3.8 MB 的二进制
+ 扫 17 MB 的 `base.dict.yaml`，**必然是阻塞 I/O** ⇒ 只能放在独立进程。

### D2 搜狗解析用「签名扫描」，不硬编码偏移

`WeaselDeployer/SogouUserDictParser.{h,cpp}` 用**结构签名**在字节流里找记录，
而不是绑定某版本搜狗的固定偏移：

- 判据：`u16 A == u16 B + 2`；`B` 为偶数且 `2 <= B <= 64`；其后 `B` 字节是**全 CJK
  UTF-16LE**（逐字符判定，排除二进制噪声）。
- 命中记录后读后缀：`[u16 flag=1][u16 seq][u32 保留][u16 K][u16 × K 音节索引]`。
- **自校验降级**：只有当 `K == 词条字数` 时才采信音节索引；否则**丢弃音节索引但保留
  词条**（计入 `stats.skipped`）。⇒ 即便搜狗改了后缀布局，**词条照样能导入**，
  只是拼音要靠回退路径补。
- 解析器与拼音解析器**纯 C++17、无 WinAPI 依赖**（仅 `<filesystem>`）⇒
  `test/TestSogouUserDict` 能直接编译**同一份源文件**（R7 同源）。

### D3 拼音来源 = 随包 `cn_dicts`，不引入任何搜狗资产

搜狗给不了明文拼音 ⇒ 拼音必须自己找。**已否决**：`radical_pinyin.dict.yaml`、
搜狗音节索引→拼音（文件内无 ASCII 拼音表）、`reverse_lookup_dictionary` /
`ReverseDb` 反查、`rime_ice.dict.yaml` 源词条。**采用随包的两张表**：

| 表 | 规模 | 用途 | 样例（物理列序 = 词条 \t 码 \t 权重） |
|---|---|---|---|
| `cn_dicts/base.dict.yaml` | 17,183,621 B / 557,579 行 | **整词** → 无调全拼（多音字取权重最高者） | `啊啊\ta a\t27365` |
| `cn_dicts/8105.dict.yaml` | 122,829 B / 8,105 字 | **逐字**回退 | `这\tzhei\t17648808` |

- 落点在 `<安装目录>\data\cn_dicts\`（`output/install.nsi:1051` 已把 `*.dict.yaml`
  打进安装器），部署器用 `WeaselSharedDataPath()` 定位。
- 解析策略：**整词优先**（只在 wanted 词条集合内收），整词查不到才**逐字回退**
  （8105 全量单字表）。
- 同一词条多行 ⇒ 取权重最高者；码内空格分隔**不做 trim**（只剥行尾 `\r`）——
  否则「空���分隔的多音节码」会被拼坏。

### D4 目标码空间 = 无调全拼、空格分隔

火流猩活跃方案 `double_pinyin_sogou.schema.yaml` 的主翻译器是
`script_translator`、`dictionary: rime_ice` ⇒ **查库用的码空间仍是全拼**，
双拼只是 speller 层的**输入**方式。实测真实 userdb dump 里 key 形如
`A I bei hou \tAI背后` ⇒ **userdb 的码就是空格分隔的无调全拼音节**。
⇒ 导入时写**无调全拼、空格分隔**，与 `rime_ice` 同一码空间。

### D5 导入格式与权重：TSV 行 = `词条 \t 码 \t commits`，`commits` 固定 100

- 格式坐实（librime 源码）：`rime_table_entry_parser`（`table_db.cc`）读
  `row[0]=词条`、`row[1]=码`、`row[2]=commits`，key = `trim(code) + " \t" + 词条`；
  `rime_table_entry_formatter` 反向输出 `词条 \t 码 \t commits` ⇒ parser 与
  formatter 互逆。
- **`commits` 取 100**（不是 1）：导入产生的 key 是**新 key**，`tick` 记 0，
  `dee = (commits+1)/1e8`；在 `present_tick ≈ 12368` 的真实库上，
  `formula_d(d, t, da, ta) = d + da*exp((ta-t)/200)` 把 `d` 压到 ~0，
  实际权重几乎全由 `m = s - (s-u)·(1-exp(-t/10000))^10` 决定。
  估算（**推断，未实机复验**）：`C=100` ⇒ weight ≈ −7.2（≈用户手打 7 次的等效权重），
  `C=1000` ⇒ ≈ −4.9，`C=10` ⇒ ≈ −10.5。
  ⇒ 取 **100** 作为「一次批量导入 ≈ 手打 7 次」的折中。**若真机排序明显偏前/偏后，
  改这一个常量即可**，不需要改格式。

### D6 导入必须夹在维护区间内

`Configurator::SogouImport()` 的结构与既有 `DictManagement()` 同款：

```
CreateMutex → MaintenanceGuard<weasel::Client> guard(client)
            → run_task("installation_update") → join_maintenance_thread()
            → SogouImportDialog dlg; dlg.DoModal();   ← 模态框在 guard 作用域内
            → CloseHandle(mutex) → guard 析构 = EndMaintenance()
```

对话框在 guard 作用域内 `DoModal()` ⇒ 每一次 `import_user_dict()` 都落在
`StartMaintenance` / `EndMaintenance` 之间 ⇒ **不会撞 leveldb 独占锁**（L10）。

### D7 交互形态：自动扫描 → 列出可导入来源 → 选来源 + 选目标词典 → 导入

- 自动扫描**两个**搜狗用户目录（`SogouPY\` 与 `SogouPY.users\<数字>\`），把
  `sgim_gd_usr.bin` / `sgim_gd_usr_od.bin` 列为可选来源；**`sgim_gd_usrbg.bin`
  是背景库，不导入**。
- 目标词典用 `user_dict_iterator` 列出真实 userdb（名字**不带** `.userdb` 后缀），
  空时兜底 `rime_ice`。
- 临时 TSV 写在 `%TEMP%`，**导入后立即删除**；搜狗文件与导出内容**全程只读**。

## Consequences

- ✅ 用户能在**输入法图标右键菜单**里直接完成迁移，不需要命令行。
- ✅ **未改 `include/WeaselIPCData.h` ⇒ 不触发 IPC 四边同步**（server / TSF /
  deployer 三端 + ADR + `test/TestWeaselIPC/*`）。
- ✅ 搜狗资产**零入库、零进安装器** ⇒ 合规红线守住，仓库仍只有自研代码。
- ✅ 解析器不绑定搜狗版本：偏移变了只要**签名判据**还成立就能用；后缀变了就
  **降级为「只取词条」**，功能不瘫。
- ✅ 拼音走随包 `cn_dicts` ⇒ 离线、零额外体积、零外部依赖。
- ⚠️ **导入的权重是「一次性估算」**：批量导入的词天然比手打的词更靠前。
  真机排序若不合理，只需调 D5 的 `commits` 常量（100 → 别的值）。
- ⚠️ `base.dict.yaml` 17 MB 全表流式扫描 ≈ 1–2 s（已用 `CWaitCursor` 兜底）；
  导入全程在部署器进程内，**不卡输入法前台**。
- ⚠️ 搜狗音节索引被**主动放弃**（`K != 字数` 即降级）⇒ 拼音靠回退拼，个别多音字
  可能拼错音；这是「不引搜狗资产」的代价。
- ⚠️ 用户词库目录在 `Program Files` 下（`D:\Program Files\fluxing\user1\fluxing`），
  部署器写入可能触发**权限问题** ⇒ 导入失败要能给出可读报错，不是静默。
- 🔁 **可逆**：导入只写 userdb；删掉对应 userdb 或用「用户词典管理」导出/导入即可回退。

## 备选方案与否决理由

| 备选 | 否决理由 |
|---|---|
| 在 TSF 前端里做（托盘菜单直接调前端） | P2 红线：解析要读 3.8 MB 二进制 + 17 MB 字典表，必阻塞；且要新开 IPC |
| 新增 IPC 命令（`action=sogou_import`） | 没有跨进程数据流；却要付四边同步 + 测试的代价 |
| 逆搜狗音节索引得到拼音 | 文件内**无** ASCII 拼音表 ⇒ 拿不到明文拼音；且贴合规红线 |
| 把搜狗词库打进安装器 | ROADMAP §6.2 红线：第三方资产不得入库/进包 |
| 用 `radical_pinyin.dict.yaml` 当拼音源 | 该表给的是**带调/部首**信息，与 userdb 的**无调全拼**码空间对不齐 |
| 硬编码搜狗文件偏移 | 换版本即失效；签名扫描对格式变化更耐打 |
| 直接按搜狗拼音写 userdb | 拿不到（见上）；且会与 `rime_ice` 码空间不一致 |

## See also

- [ADR 0030](0030-continue-fluxing-fork-no-rewrite-perf-baseline.md) — 路线定案：
  继续火流猩 fork，不做搜狗逆向重写；本 ADR 是该路线下的**最小可用迁移能力**
- [ADR 0012](0012-shift-ipc-response-body-and-session-local-ascii.md) /
  [ADR 0028](0028-thin-tsf-shell-and-additive-ipc.md) — IPC 只增不改、薄 TSF 壳
- [.specify/memory/constitution.md](../../.specify/memory/constitution.md) — P2 / P5 /
  R7 / P8
- [.specify/memory/lessons-learned.md](../../.specify/memory/lessons-learned.md) — L10
  （leveldb 维护锁）
- [ROADMAP](../../ROADMAP.md) — §6.2 第三方资产红线
