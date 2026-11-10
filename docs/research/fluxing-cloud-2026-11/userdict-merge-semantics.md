# 词库 / 短语合并语义取证（T102）

- 批次：X · T102（最高优先，R1 风险，T503 硬前置）
- 状态：**结论已定案**（源码 + 实测双证；唯一未实测项已单列 §8）
- 取证日期：2026-11-10
- 取证环境：Windows 10.0.26300 x64 / MSVC 2022 BuildTools x86 / librime 1.13.1（`librime/dist_Win32`）
- 探针与产物：`tmp/t102/`（**不入库**，见 `.gitignore` 现状；复跑见 §10）

---

## 0. 结论摘要（TL;DR）

**`export_user_dict` + `import_user_dict` 不能作为云同步的合并原语；`backup_user_dict` + `restore_user_dict`（内部即 `UserDbMerger`）可以。**

| 原语对 | 能否承载「跨设备双向合并」 | 决定性原因 |
|---|---|---|
| `export` + `import` | **否** | ① `export` **只导 `commits`**，`dee` 与逐条 `tick` 全部丢失（§3.1）；② `export` **跳过墓碑**，删除无法跨设备传播（§3.2）；③ `import` **不写 `tick`**，导入后条目时间戳停在旧值（§3.3）；④ `import` 把 `-1` 放大成 `-2`、把 `0` 写成垃圾条目（§3.3）；⑤ `import` 用可创建的 `Open()`，对不存在的库**静默建库**（§6） |
| `backup` + `restore` | **是**（T503 基座） | 快照是**完整列族**（含墓碑、`dee`、逐条 `tick`、`/tick`、`/user_id`）；`restore` 走 `UserDbMerger`，按 `commits` 绝对值择优、`dee` 取 max、逐条 `tick` 抬升（§4）——但**删除不传播**，见 §4.4 硬约束 ① |

**对 T503 的一句话**：用备份快照作为传输单元，用 `restore_user_dict` 作为合并动作；**不要**自己实现「读 TSV → 逐条 Update」，那等于重新踩一遍 `import` 的五个坑。

---

## 1. 取证方法论

### 1.1 为什么必须用隔离世界

`RimeLeversApi::{export,import,backup,restore}_user_dict` 直接操作 **leveldb 目录**。若指向真机 `%APPDATA%\Fluxing`，会污染用户真实词库，且 leveldb LOCK 会让运行中的 `WeaselServer` 报错。故探针每次运行都自建一个最小世界：

```
tmp/t102/art/world/w_<phase>/
  shared/     # default.yaml + t102.schema.yaml + t102.dict.yaml（C++ 内联写出）
  user/       # user_data_dir，userdb 落在这里
  log/
```

`traits` 只设 `shared_data_dir / user_data_dir / log_dir / distribution_* / app_name`，不读真实 `installation.yaml`。**实测确认真机 `%APPDATA%\Fluxing` 未被触碰**（该目录在本机只含 `build/`、`installation.yaml`、`user.yaml`）。

### 1.2 双观测通道

单靠 C API 返回值无法区分「没写」与「写了但读不到」，故探针用两条独立通道互证：

| 通道 | 实现 | 用途 |
|---|---|---|
| ① 逻辑视图 | `export_user_dict` 的 TSV 正文 | 看「引擎认为活着的条目」 |
| ② 物理视图 | `ScanTriplets()` 正则扫 leveldb 目录内明文，模式 `c=-?[0-9]+ d=-?[0-9]+(?:\.[0-9]+)?(?:e[-+]?[0-9]+)? t=[0-9]+` | 看「磁盘上真实写下的三元组」 |

> **② 的读法**：leveldb 的 SST/MANIFEST 里保留**历史版本**，所以三联体条数 **>** 活条目数是常态，不是 bug。判断增删要按 key 去重后看最新 `tick` 的那条。

### 1.3 产物清单（实测输出片段路径）

| 场景 | 报告 | 导出件 |
|---|---|---|
| A 导出字段集 | `tmp/t102/art/report_export.txt`（1,284 B） | `export_A.userdb.txt`（172 B） |
| B 墓碑（导出通道） | `tmp/t102/art/report_tombstone.txt`（1,219 B） | `export_B.userdb.txt`（151 B，**空正文**） |
| B2 墓碑（备份通道） | 同上 | 快照 `tmp/t102/sync/d2427409…/t102.userdb.txt`（172 B） |
| C 导入 | `tmp/t102/art/report_import.txt`（1,462 B） | `import_C.userdb.txt`（31 B）/ `export_C_after.userdb.txt`（173 B） |
| D 备份落点 | `tmp/t102/art/report_remote.txt`（817 B） | 见 §5 |
| E 恢复合并 | `tmp/t102/art/report_restore.txt`（1,442 B） | `export_E_after.userdb.txt`（194 B）/ 受控快照 `snapshot_remote.userdb.txt`（214 B） |
| F 返回码 | `tmp/t102/art/report_missing.txt`（775 B） | `import_F.userdb.txt`（9 B） |

汇总：`tmp/t102/all-reports.txt`（6,998 B）。控制台日志：`tmp/t102/console_<phase>.{out,err}`、`run.log`、`build.log`。

---

## 2. 源码基准（结论的「为什么」）

| 主题 | 位置（`librime/src/rime/`） |
|---|---|
| 三元组打包 `c=<commits> d=<dee> t=<tick>` | `dict/user_db.h:20-23`（`UserDbValue::Pack()`） |
| 学习侧写三元组 | `dict/user_dictionary.cc:412-443`（`UpdateEntry`） |
| 读侧跳过墓碑 | `dict/user_dictionary.cc:532-544` |
| `UserDbImporter::Put` | `dict/user_db.cc:247-263` |
| **`UserDbMerger::{MetaPut,Put,CloseMerge}`** | `dict/user_db.cc:182-239` |
| `get_tick_count`（缺省 1） | `dict/user_db.cc:171-180` |
| **`Backup/Restore/Export/Import` 四原语** | `lever/user_dict_manager.cc:51-152` |
| C API 转发（**不吞返回值**） | `lever/levers_api_impl.h:220-240` |
| `IsUserDb()` = `/db_type == "userdb"` | `dict/user_db.cc:142-145` |
| 快照 parser/formatter | `dict/user_db.cc:60-97` |
| **`create_if_missing = !readonly`** | `dict/level_db.cc:54-58` |
| `LevelDb::Open/OpenReadOnly/Backup/Restore` | `dict/level_db.cc:193-276` |
| `sync_dir` 初始为相对路径 `"sync"` | `deployer.cc:18-21` |
| `user_data_sync_dir()` = `sync_dir / user_id` | `deployer.cc:149-151` |
| 部署任务从 `installation.yaml` 读 `sync_dir` | `lever/deployment_tasks.cc:104-112` |
| `SyncSnapshot` 遍历 `sync_dir` 子目录 | `lever/user_dict_manager.cc:178-208` |
| `get_user_data_sync_dir` / `get_sync_dir_s` | `rime_api.h:392` / `:499` |

---

## 3. 三条可复跑结论（T102 Done 判据）

### 3.1 导出字段集：只有 `commits` 能出库

**实测**（`tmp/t102/art/report_export.txt`）：学习两次后，物理通道读到三联体

```
c=1 d=1 t=1
c=1 d=1 t=3
c=2 d=1.99501 t=2
```

调 `export_user_dict("t102", …)` 返回 **2**，导出件 `export_A.userdb.txt` 全文：

```
# Rime user dictionary export
#@/db_name	t102
#@/db_type	userdb
#@/rime_version	1.13.1
#@/tick	3
#@/user_id	698c…
好	hao	1
你	ni	2
```

**结论**：导出正文每行只有 `code <TAB> phrase <TAB> commits`。**`dee` 与逐条 `tick` 100% 丢失**；头部 `#@/tick` 是库级 tick，不是条目级。

**为什么这不只是「少两个字段」**：`dee` 是衰减后的词频权重（读侧按它排序），`tick` 是合并时判断新旧与衰减幅度的唯一依据。两者丢失后，接收端无法还原「这个词有多重」与「它是什么时候学的」——**合并退化为按 commits 比的粗糙择优**，且衰减语义彻底失效。

**源码对应**：`lever/user_dict_manager.cc` 的 `Export()` 用 `TsvWriter`，写入字段集由 `dict/user_db.cc:60-97` 的 `userdb_entry_formatter` 决定，只含 `commits`。

### 3.2 墓碑：`export` 通道上彻底消失，`backup` 通道完整保留

**实测（导出通道）**：`delete_candidate_on_current_page(0)` 返回 `true` 后，物理通道立刻看到墓碑

```
c=-2 d=1.99501 t=2
```

但紧接着 `export_user_dict` 返回 **0**，`export_B.userdb.txt`（151 B）**正文为空**（只剩头）。

⇒ **删除操作在 `export` 通道上不可见**。若用 export/import 做同步，A 机删掉的词会在 B 机「死而复生」，且下次 B 机回传后 A 机也复活——单向且不可逆的语义错误。

**实测（备份通道）**：同一状态调 `backup_user_dict`，快照 `tmp/t102/sync/d2427409…/t102.userdb.txt`（172 B）正文为

```
ni 	你	c=-2 d=1.99501 t=2
```

⇒ **墓碑在快照里是完整一行**，可跨设备传输。

**源码对应**：`Export()` 走 `TsvWriter`，其遍历回调跳过 `v.commits <= 0` 或等价条件；`Backup()` 走 `UserDbHelper::UniformBackup`，是**整库字节级快照**，不做语义过滤。

### 3.3 导入：不写 `tick`、放大地雷、造垃圾条目

**实测**（`tmp/t102/art/report_import.txt`）：喂三条审计过界的记录

```
你	ni	100
好	hao	-1
测	ce	0
```

`import_user_dict` 返回 **3**（行数，不是错误码）。导入后物理通道：

| 输入 | 落盘三元组 | 问题 |
|---|---|---|
| `ni 100` | `c=100 d=1 t=1` | **`dee` 与 `tick` 未重算**，`t` 仍是导入前的旧 tick |
| `hao -1` | `c=-2 d=1.99501 t=3` | **`-1` 被放大成 `-2`**（`min(v.commits, -abs(o.commits))`） |
| `ce 0` | `c=0 d=0 t=0` | **垃圾条目**，且 `export` 会把它导出成 `测 ce 0` |

导入后 `export_user_dict` 返回 **2**（`你`/`测`；`好` 是墓碑被跳过），正文含 `测	ce	0`。

**源码对应**（`dict/user_db.cc:247-263`，`UserDbImporter::Put` 原文语义）：

```cpp
if (v.commits > 0) { o.commits = max(o.commits, v.commits); o.dee = max(o.dee, v.dee); }
else if (v.commits < 0) { o.commits = min(v.commits, -abs(o.commits)); }
// v.commits == 0：两个分支都不进，直接写默认值
db_->Update(key, o.Pack());   // ← 全程不赋值 o.tick
```

三个行为由此直接推出：
- **`o.tick` 从未被赋值** ⇒ 导入条目永远拿不到新时间戳（上表 `t=1` / `t=3` 是本地旧值或默认值）。
- **`-1` → `-2`** ⇒ `min(-1, -abs(已有 commits))`，若本地已有 `c=-2`，取 `min(-1,-2) = -2`。墓碑计数只增不减，且**方向依赖本地状态** ⇒ 不可预测。
- **`0` → 垃圾** ⇒ 两分支都不进，`o` 保持默认构造（`c=0 d=0 t=0`），照样 `Update` 写盘。**导入端无法用 `0` 表达任何语义**。

### 3.4 补充实测：`backup` 的落点不在 `<user_data>/sync`

**实测**（`tmp/t102/art/report_remote.txt`）：`backup_user_dict` 返回 `true`，但 `<user>/sync` 是**空目录**；快照实际落在**进程 CWD** 下的 `sync/<user_id>/`。

**源码坐实**（`deployer.cc:18-21` + `lever/deployment_tasks.cc:104-112`）：`Deployer` 构造时把 `sync_dir` 初始化为**相对路径** `"sync"`；只有跑过部署任务、从 `installation.yaml` 读到 `sync_dir` 时才会变成绝对路径，否则是 `user_data_path / "sync"`。

**对 T503 的直接影响**：云同步侧**不能手工拼** `<user_data>/sync/<user_id>`。必须用 `get_user_data_sync_dir(char*, size_t)`（`rime_api.h:392`）或 `get_sync_dir_s`（`:499`）取真实目录，否则在「跑过部署」与「没跑过部署」两种装机状态下会读到不同位置，表现为「有的机器第一次同步就读不到快照」。

---

## 4. `UserDbMerger` 语义全解（T503 基座）

`restore_user_dict` 的实现路径（`lever/user_dict_manager.cc:51-152`）：

```
temp db（.temp 后缀，先 Exists() 则 Remove()）→ Open() → Restore(snapshot)
  → IsUserDb() → GetDbName() → dest->Open() → DbSource source(temp) ; UserDbMerger merger(dest) ; source >> merger
```

`UserDbMerger`（`dict/user_db.cc:182-239`）三步：

```cpp
UserDbMerger(Db* db) { our_tick_ = get_tick_count(db); their_tick_ = 0; max_tick_ = our_tick_; }

MetaPut(key, value) {                       // 读快照头
  if (key == "/tick") { their_tick_ = stoul(value); max_tick_ = max(our_tick_, their_tick_); }
  return true;                              // ← 其它头字段（含 /user_id）被忽略
}

Put(key, value) {                           // 逐条合并
  UserDbValue v(value);
  if (v.tick < their_tick_) v.dee = algo::formula_d(0, their_tick_, v.dee, v.tick);   // 远端条目自身老化
  UserDbValue o;
  if (db_->Fetch(key, &our_value)) o.Unpack(our_value);
  if (o.tick < our_tick_)   o.dee = algo::formula_d(0, our_tick_,   o.dee, o.tick);   // 本地条目自身老化
  if (abs(o.commits) < abs(v.commits)) o.commits = v.commits;                          // 逐字段，不整体替换
  o.dee = max(o.dee, v.dee);                                                          // dee 单调不减
  o.tick = max_tick_;                                                                 // tick 一律抬升
  return db_->Update(key, o.Pack()) && ++merged_entries_;
}

CloseMerge() { db_->MetaUpdate("/tick", max_tick_); db_->MetaUpdate("/user_id", deployer.user_id); … }
```

### 4.1 实测验证（场景 E）

**受控快照**（`tmp/t102/art/snapshot_remote.userdb.txt`，214 B）：

```
#@/tick	4
#@/user_id	deadbeefcafebabe
ni 	你	c=100 d=1 t=2
hao 	好	c=-5 d=0.9 t=3
ce 	测	c=3 d=3 t=4
ha 	号	c=2 d=2 t=1
```

**restore 前本地**（世界 `w_restore`，学习 你×1、好×5）：`你 c=1 d=1 t=1`、`好 c=5 d=4.95037 t=6`。

**`restore_user_dict` 返回 `true`**。restore 后物理通道：

| key | 三元组 | 解读 |
|---|---|---|
| 你（旧版残留 SST） | `c=1 d=1 t=1` | 历史版本，非活值 |
| 你 | `c=100 d=0.99005 t=6` | abs 取胜 100；**`dee` 被 `formula_d(0,4,1,2)` 衰减 1→0.99005**；tick 抬到 6 |
| 号 | `c=2 d=1.97022 t=6` | 新增；`dee` 2→1.97022（远端自身老化，`t=1 < their_tick_=4`） |
| 测 | `c=3 d=3 t=6` | 新增；`dee` 原样（`t=4 == their_tick_`，不触发老化） |
| 好 | `c=5 d=4.95037 t=6` | **远端墓碑 `c=-5` 与本地活条目 `c=5` abs 打平 ⇒ 本地胜，条目保留** |

restore 后 `export` 返回 **4**，正文 `测 ce 3` / `号 ha 2` / `好 hao 5` / `你 ni 100`；头 `#@/tick 6`、`#@/user_id c6c319d4-…`（**本地 user_id 覆盖了外来 `deadbeefcafebabe`**）。

### 4.2 幂等性实测

对同一状态**第二次**应用同一快照：返回仍 `true`，`export` 结果与第一次**逐字节相同**（`#@/tick 6`，四条同值）。

**为什么幂等**：`their_tick_` 恒为快照头里的 `4`（不随本地 tick 变化），故远端条目的老化幅度每次相同；本地 `o.tick` 已等于 `max_tick_`，`o.tick < our_tick_` 不再成立 ⇒ 不再二次衰减。

> **注意边界**：这是「同一快照重复 apply」的幂等，**不等于**「任意顺序 apply 多个快照」可交换。见 §8。

### 4.3 五条必须写进 T503 的语义

| # | 语义 | 实证 |
|---|---|---|
| 1 | 按 **`abs(commits)`** 择优，**相等时本地留** | 「好」`c=5` vs 墓碑 `c=-5`，本地活条目保留 |
| 2 | `dee = max(本地衰减值, 远端衰减值)` ⇒ **永不减小** | 「你」`dee` 1→0.99005 是衰减，但取值仍是 max |
| 3 | **所有被合并的 key，`o.tick` 一律抬到 `max(本地 /tick, 快照 /tick)`** | 四条全部 `t=6`（本地 `/tick` 已到 6） |
| 4 | `/user_id` **总被本地 deployer user_id 覆盖**，外来值丢弃 | `deadbeefcafebabe` → `c6c319d4-…` |
| 5 | 老化只按**快照自身 tick 差**算，不随本地 tick 前进 | 第二次 apply 与第一次逐字节相同 |

### 4.4 三条致命边界（T503 设计必须正面处理）

**① 删除不传播。** 墓碑 `c=-5` 与活条目 `c=5` 打平时本地胜 ⇒ **A 机删掉的词，B 机合并后仍存在**；B 机回传后 A 机也复活。语义上「删除」退化为「降权到打平」。这是 `UserDbMerger` 的**设计选择**（保守，避免误删），不是 bug，但**必须在产品口径上承认**：跨设备删除不保证生效，只能通过「删除后重新学习」自然回升。

**② `dee` 单调不减。** 长时间多端同步后，`dee` 会被各端的历史最大值托住，**无法自然衰减到 0**。若运营上依赖「长期不用的词自动消失」，该机制在本方案下失效（除非另做定期 compaction，属新需求）。

**③ 全局 `/tick` 单调抬升。** 每次合并把 `max_tick_` 写回本地 `/tick`。若某端时钟/学习进度异常大（例如手动改过快照），会把全库 tick 抬到高位，**此后本地所有条目的老化基准都被拉高**，表现为「大量词频突然变低」。T503 需要在合并前校验快照 `/tick` 与本地 `/tick` 的差值上限。

---

## 5. 传输单元形态（云端要存的东西）

**实测字节级形态**（四份样本互证，UTF-8 **无 BOM**，**CRLF** 行尾）：

```
# Rime user dictionary
#@/db_name<TAB>t102
#@/db_type<TAB>userdb
#@/rime_version<TAB>1.13.1
#@/tick<TAB>3
#@/user_id<TAB><uuid>
<code><space><TAB><phrase><TAB>c=<n> d=<double> t=<tick>
```

正文样例行：`ni <TAB>你<TAB>c=3 d=2.98506 t=3`。注意 `code` 后是**一个空格再一个 TAB**（`"code " + "\t"`），不是两个 TAB。

**落点**：`<sync_dir>/<user_id>/<dict_name>.userdb.txt`（§3.4 已说明 `sync_dir` 必须由 API 取，不能手拼）。

**四份实测样本**（`tmp/t102/sync/`）：

| 目录前缀 | 大小 | `/tick` | 特征 |
|---|---|---|---|
| `09d10088…` | 168 B | **无** | 空库备份，无 `/tick` 头 ⇒ 接收端 `their_tick_ = 0` |
| `3e5d25cc…` | 217 B | 3 | 正常 |
| `806b39d1…` | 193 B | 4 | 含墓碑 |
| `d2427409…` | 172 B | 2 | 墓碑样本 |

> **可空性风险**：`09d10088…` 证明**快照可以没有 `/tick` 头**。`UserDbMerger::MetaPut` 只在遇到 `"/tick"` 时设 `their_tick_`，缺失则保持构造值 `0` ⇒ 所有远端条目都不触发老化，`max_tick_ = our_tick_`。**T503 必须处理缺 `/tick` 的快照**（按 0 处理即可，与源码一致）。

---

## 6. 返回码语义不对称（源码坐实，非封装层 bug）

**实测**（`tmp/t102/art/report_missing.txt`）：

| 调用（同一次进程内顺序执行） | 返回 | 期望 |
|---|---|---|
| `export_user_dict(未学习过的库)` | **-1** | ✅ 失败 |
| `import_user_dict(未学习过的库)` | **1** | ❌ 应为 -1 |
| `import_user_dict(不存在的 dict)` | **1** | ❌ 应为 -1 |
| `backup_user_dict(不存在的 dict)` | **true** | ❌ 应为 false |
| `restore_user_dict(不存在的快照)` | **false** | ✅ 失败 |

且 `w_missing` 下 **`t102.userdb` 目录被真的创建了出来**（`000003.log` 225 B / `CURRENT` / `LOCK` / `LOG` / `MANIFEST-000002`）。

**根因**（`dict/level_db.cc:54-58`）：`LevelDbWrapper::Open` 里

```cpp
options.create_if_missing = !readonly;
```

于是：

- `import_user_dict` 用 `Open()`（`readonly=false`，**可创建**）⇒ 对不存在的库**静默建库**并写入条目，返回「导入行数」而非错误码。
- `backup_user_dict` 因此返回 `true` —— **上一步已把库建出来了**。这是**探针调用顺序造成的假象**，不是封装层吞返回值（`lever/levers_api_impl.h:220-240` 是直接转发）。
- `export_user_dict` 用 `OpenReadOnly()`（**不可创建**）⇒ 不存在时 `-1`。

> **给 T503 的规则**：判「库是否存在」**只能用 `export_user_dict` 或直接查文件系统**，不能用 `import` / `backup` 的返回值。任何「先 import 试一下」的探测都会**在磁盘上留下空库**。

---

## 7. 与生产代码的一致性核对

生产侧现有用法（供 T503 抄结构，不抄语义）：

- `WeaselDeployer/Configurator.cpp` —— `MaintenanceGuard` 包裹 `import_user_dict`。**这是硬约束**：`Restore`/`Import` 内部 `dest->Open()` 是读写独占锁，必须由维护态包裹，否则 leveldb LOCK 失败（见 `.claude/rules/ipc-boundary.md:45`、`.claude/rules/commit-checklist.md:40`）。
- `WeaselDeployer/DictManagementDialog.cpp` —— 导入导出 UI 的调用范式。
- `WeaselDeployer/SogouUserDictParser.*` —— 外部词库 → TSV 的转换（**注意**：它输出的就是 `code/commits` 二列，所以外部导入天然受 §3.1 限制；这是可接受的一次性迁移，不适用于双向同步）。

---

## 8. 未验证项与残余风险（**不要当成已证结论引用**）

| # | 未验证项 | 现状 | 风险 | 建议动作 |
|---|---|---|---|---|
| 1 | **真并发下的锁行为** | 探针每次 `destroy_session` 后才调 levers 原语，`UserDictionary` 随会话销毁 ⇒ **无锁争用**。生产侧证据仅为源码 + `MaintenanceGuard` 用法 | 若云端同步线程与 TSF 学习线程同时写 userdb，可能出现 LOCK 失败或写丢失 | 列为 T503 首个单测：双进程同时 `restore` + 学习，断言无 `LOCK` 错误。**在 T503 开工前不得引用本文件声称「并发安全」** |
| 2 | **多快照任意顺序的可交换性** | 只测了「同一快照 apply 两次」（§4.2 幂等） | `abs(commits)` 择优 + `dee` 取 max 是**单调**运算，理论上可交换；但 `tick` 抬升与 `their_tick_` 依赖快照自身头 ⇒ **顺序可能影响 `dee` 衰减幅度** | T503 补测「A→B 后再 B'→A」与反序，比对最终三元组 |
| 3 | **`formula_d` 的具体曲线** | 只实测了若干点的结果（1→0.99005 @ Δtick=2；2→1.97022 @ Δtick=3），未精读公式 | 若 T503 需要预测衰减幅度，光看实测点不够 | 需要时精读 `algo::formula_d`（`librime/src/rime/algo/`） |
| 4 | **`rime_dict_manager.exe` CLI 契约** | `-h` 返回 `invalid arguments.` + `EXIT=1`；且必须先注入 `dist_Win32\lib` 到 PATH 否则 `0xC0000135` | 不可作为自动化捷径 | 不依赖 CLI，直接调 C API |
| 5 | **真实用户词库素材** | 本机 `%APPDATA%\Fluxing` 无任何 `*.userdb*` | 样本规模小（3 词），未见大库下的合并耗时 | T503 用真实装机库补测一次耗时 |

---

## 9. 对 T503 的实现约束清单（可直接当验收项）

1. **传输单元 = 备份快照**（`backup_user_dict` 产物），不是 export TSV。
2. **合并动作 = `restore_user_dict`**，不自己写「读表 → 逐条 Update」。
3. **`sync_dir` 必须用 `get_user_data_sync_dir()` 取**，禁止手工拼 `<user_data>/sync`。
4. 快照可能**缺 `/tick`**（按 0 处理）与**缺 `/user_id`**（忽略）。
5. `/user_id` 合并后**必为本地值**；上传前应**剥离或替换** `/user_id`，避免把本机 UUID 当账号标识外泄。
6. **删除不传播** —— 产品文案不得承诺「删除会同步到其他设备」。
7. **`dee` 单调不减** —— 不得依赖「长期不用自动消失」。
8. **合并前后 `/tick` 差值需设上限**，防止异常快照把全库 tick 抬飞。
9. 整个 restore 动作必须被 `StartMaintenance` / `EndMaintenance` 包裹。
10. 判库存在性**只用 `export_user_dict` 或文件系统**，不用 `import`/`backup` 返回值。

---

## 10. 复跑指引

```bat
:: 1) 编译探针（x86，不依赖 WeaselServer）
F:\soft\00selfmade\rime_claude\tmp\t102\build.cmd
:: 期望输出结尾：CL_EXIT=0 … BUILD_OK
```

```powershell
# 2) 跑七个 phase（必须用 -WorkingDirectory 指定，art/ 与 sync/ 都相对 CWD）
$root='F:\soft\00selfmade\rime_claude'; $dist="$root\librime\dist_Win32"; $wd="$root\tmp\t102"
$env:PATH="$dist\lib;$dist\bin;$wd;$env:PATH"
foreach($ph in @('export','tombstone','missing','remote','restore','import')){
  Start-Process -FilePath "$wd\t102_probe.exe" -ArgumentList $ph -WorkingDirectory $wd -NoNewWindow -Wait `
    -RedirectStandardOutput "$wd\console_$ph.out" -RedirectStandardError "$wd\console_$ph.err"
}
```

```powershell
# 3) 构造受控快照后再跑 restore（验证 UserDbMerger 语义）
# 头：#@/tick\t4 / #@/user_id\tdeadbeefcafebabe
# 正文：ni \t你\tc=100 d=1 t=2 ; hao \t好\tc=-5 d=0.9 t=3 ; ce \t测\tc=3 d=3 t=4 ; ha \t号\tc=2 d=2 t=1
# 见 tmp/t102/art/snapshot_remote.userdb.txt（214 B），CRLF + UTF-8 无 BOM
```

**判读要点**：`report_*.txt` 是主证；`export_*.userdb.txt` 是导出结果；`tmp/t102/sync/<user_id>/` 是备份快照。级别 db 目录内的三联体条数多于活条目数是正常现象（§1.2）。

**探针不入库**：`tmp/` 已在忽略范围；本文件是唯一入库的 T102 产物。