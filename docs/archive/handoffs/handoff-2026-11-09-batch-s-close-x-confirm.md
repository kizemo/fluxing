# 批次 S · spec 045 快捷键设置 —— 圆形 ✕ 脏态确认 P0 修复 + 方案下拉候选源评估

> 日期：2026-11-09
> 分支：`Fluxing`（远端 `kizemo/Fluxing`，**不推 origin**）
> 上一批：[批次 R · spec 045 T301–T304 可测入口 + T201/T202/T203 界面补全](handoff-2026-11-09-batch-r-t201-t304.md)

---

## 1. 背景

批次 R 提交了三块（`022efcf` WeaselServer / `a87e13c` test(shortcut-settings) / `15abe1c` docs(spec-045)），
本批做**装机真机验收**，并对验收暴露的缺陷做修复。

批次 R 之前的真机遗留（同一功能面）：
- T201 方案下拉只显示「全局」一项；
- T202 圆形 ✕ 是否可点；
- T203 有未保存修改时关闭是否弹确认。

---

## 2. 本批结论（先看这节）

### 2.1 验收矩阵（真机，装机副本 `D:\Program Files\fluxing`）

| 验收项 | 结果 | 证据 |
|---|---|---|
| Fix A 窗口真的显示在屏幕上（非探针 PASS） | **PASS** | `tmp/v-batchR5-01-dialog.png`、`tmp/v-batchR5-02-screen.png`；`DIALOG rect=1520,704,2320,1384 size=800x680 ON_SCREEN=True`（虚拟屏 3840×2160） |
| chips 点击筛选 | **PASS** | 行数 `all=36 → chip1(切换)=6 → back=36`；`tmp/v-batchR5-03-chips-before/after.png` |
| 方案下拉可展开 | **PASS（但只有「全局」一项）** | `COMBO_COUNT=1 CURSEL=0`，`COMBO_ITEM[0]=全局`；`tmp/v-batchR5-04-dropdown.png` |
| 「+ 自定义」子 modal | **PASS** | `SETTEXT=True READBACK=[F7]`、`MODAL_OK=True`、行数 36→37；`tmp/v-batchR5-05-modal.png` |
| 圆形 ✕ 可点 | **PASS** | 探针 R7：空白处点击(400,30)面板存活、闲置 3.5s 面板存活 ⇒ 面板不会误自毁；点 ✕ 后面板立即销毁 ⇒ 命中判定生效 |
| 脏态点**底栏「取消」**弹确认 | **PASS** | `tmp/v-batchR4-07-confirm-cancel.png`；文案「有 1 处快捷键修改尚未保存，关闭后将丢失。确定关闭？」，按钮 `是(&Y)` / `否(&N)`，点「否」后面板保留 |
| 脏态点**圆形 ✕**弹确认 | **修复前 FAIL → 修复后 PASS** | 修复前：`tmp/v-batchR7-report.txt` `A_MSGBOX=(none) AFTER_MS=5083`、`A_DLG_ALIVE=False`（面板 10ms 内消失）；修复后：`tmp/v-batchR8-report.txt` `A_MSGBOX=未保存的修改 AFTER_MS=24`、`A_DLG_ALIVE=True`，点「否」后面板保留；回归：`tmp/v-batchR9-report.txt` `CONFIRM_ON_X=未保存的修改`（文案「有 2 处…」与状态行「2 修改」一致）、`DIALOG_OPEN_AFTER_X_NO=yes` |
| 真实配置未被改动 | **PASS** | `default.custom.yaml` 前后 sha256=`50A46CC77DBB08D26384A291E3CFD4A865E89977FEF1B65547F93ABF7876669D`、mtime=`2026-11-09 01:41:18` 完全一致；全程未点「保存」 |

### 2.2 P0 缺陷（修复前）

**症状**：状态行显示 `● 未保存 (1 修改)   ✓ 无冲突`，但点右上角圆形 ✕ ⇒ 面板 10ms 内消失，
5 秒内**没有任何确认框**，这一轮修改被静默丢弃。点底栏「取消」在面板处于前台时又能正常弹框。

**根因**：`MessageBoxW` 的 owner 就是面板本身（`ShortcutSettings::RequestCancel()`）。
面板在 `WM_ACTIVATEAPP(FALSE)` / `WM_KILLFOCUS` 里按 `kShowGraceMs` 宽限后调 `Hide()` → `DestroyWindow(s_hwnd)`；
owner 一旦被销毁，**确认框会连带被系统关闭**。真机上探针进程抢不到前台（面板非前台），
弹框瞬间就触发失焦自毁 ⇒ 用户根本看不到确认框。

**修复**（`WeaselServer/ShortcutSettings.cpp` / `.h`）：
1. 新增文件级标记 `g_inConfirmDialog`：`RequestCancel()` 弹框前置 true、返回后置 false；
   `WM_ACTIVATEAPP` / `WM_KILLFOCUS` 在该标记为真时**跳过 `Hide()`** ⇒ 确认框不再被 owner 销毁带走。
2. 抽出纯函数 `ShortcutSettings::CountDirtyChanges(rows)`：状态行（`RefreshStatus`）、
   关闭确认（`RequestCancel`）、方案切换确认（`ID_SCHEME`）三处共用同一判据，
   消除「状态行说 N 处修改、确认框却说 1 处」的口径漂移（constitution R7：生产与单测共用同一实现）。
3. 单测 `TestT203DirtyCount()` 覆盖：改过 1 行 ⇒ 1；`accept_new` 空 ⇒ 0；`accept_new == accept` ⇒ 0。

---

## 3. T201 方案下拉（只评估，未改架构）

**现状**：当前实现 `ListSchemaStems(ParentDirOf(s_yamlPath))` 枚举的是**用户目录**，
而用户目录里没有任何 `*.schema.yaml` ⇒ 候选只剩硬编码的「全局」（真机 `COMBO_COUNT=1` 实测）。

- 用户目录 `D:\Program Files\fluxing\user1\fluxing`：`*.schema.yaml` **0 个**
- 共享目录 `D:\Program Files\fluxing\weasel\data`：**11 个**
  （`double_pinyin_abc` / `flypy` / `jiajia` / `mspy` / `sogou` / `ziguang` / `double_pinyin` /
  `radical_pinyin` / `rime_ice` / `melt_eng` / `t9`）

**不需要接 librime `RimeGetSharedDataDir`**：
- `include/WeaselUtility.h:39` 已声明 `WeaselSharedDataPath()`，
  `RimeWithWeasel/WeaselUtility.cpp:12-15` 实现为 `<exe 目录>\data` —— 正是共享目录。
- `WeaselServer/xmake.lua` 已 `add_deps(..., "RimeWithWeasel", ...)`；
  `RimeWithWeasel/xmake.lua` 是 `add_files("./*.cpp")` 静态库，`WeaselUtility.cpp.obj` 已在其中；
  `WeaselServer/ShortcutSettings.cpp:12` 已 `#include <WeaselUtility.h>`。
  ⇒ **改动面 = `ShortcutSettings.cpp` 一行枚举目录切换，零新依赖、零架构改动。**

**建议方案（待采纳，未实施）**：
1. 候选列表来源：共享目录 `WeaselSharedDataPath()` 的 `*.schema.yaml` stems（+ 保留「全局」）。
2. 写盘仍落**用户目录** `<stem>.custom.yaml`（`.custom.yaml` 是用户覆盖，语义不变）。
3. 切换前校验用户 `default.custom.yaml` 的 `schema_list` 是否已包含该 stem，
   未包含则提示「该方案尚未启用，先在方案列表中启用」——避免写出永远不生效的覆盖文件。
4. 建议候选集取「共享目录 stems ∩ 用户目录已有 `.custom.yaml` 的 stems」，
   让「切换方案」语义等价于「编辑当前已在用的方案的键位」。

---

## 4. 本轮踩到的坑（重要，别重复踩）

1. **失焦自毁会吞掉确认框**（本批 P0）：`MessageBox(owner=面板)` + 面板失焦自毁 ⇒ 框瞬间消失。
   任何「面板上弹确认框」的实现都要有「弹框期间禁止自毁」的保护。
2. **探针抓不到瞬时弹窗 = 假阴性**：面板非前台时，确认框存活时间 < 10ms，
   10ms 轮询也只抓到 `A_TRACE` 第一格就是 `False`。判据必须是「面板是否消失」+「框是否出现」两条一起看。
3. **PostMessage 的点击不激活窗口**：探针用 `PostMessage(WM_LBUTTONDOWN/UP)`，
   面板不会因此获得焦点 ⇒ 真机用户点 ✕ 时的前台状态与探针不同 ⇒ 暴露了「非前台时确认框被吞」的缺陷。
   ⇒ 自动化探针必须显式 `ForceForeground`（`AttachThreadInput` + `SetForegroundWindow`），
   否则会测出「面板非前台」这条真实用户路径上的问题（这次正好命中）。
4. `powershell`（Windows PowerShell 5.1）读**无 BOM UTF-8** 脚本会乱码报 `MissingEndCurlyBrace`
   ⇒ L0 门禁必须用 PS 7 直接调用 `& .\scripts\gate\L0-static.ps1 -Mode all`。
5. `xbuild.bat` 依赖 VS 开发环境变量 ⇒ 必须走 `tmp/build-installer.cmd`（内部 `call vcvars32.bat`）。
6. Bash 工具 hard safety policy 会拦截「`cd` 之后写/删」的复合命令 ⇒ 用绝对路径、不改 cwd。
7. **单测 exe 有两个**：msbuild 输出在**仓库根** `Release\TestShortcutSettings.exe`；
   `test\TestShortcutSettings\Debug\TestShortcutSettings.exe` 是旧文件（14:51）⇒
   直接跑 Debug 那个会得到旧数字 `PASS=246`，误以为新断言没生效（应为 `PASS=249`）。

---

## 5. 复现命令

```powershell
# 构建 + 打包（内部会 call vcvars32.bat）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-installer.cmd"

# 单测
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-test-shortcut.cmd"

# L0 静态门禁（PS 7）
Set-Location F:\soft\00selfmade\rime_claude
& .\scripts\gate\L0-static.ps1 -Mode all

# 静默装机（只允许 D:\Program Files\fluxing）
& 'F:\soft\00selfmade\rime_claude\scripts\install\silent-install.ps1' `
    -InstallRoot 'D:\Program Files\fluxing' `
    -Installer 'F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.2.1-installer.exe'

# 三实验判别探针（空白点击对照 / 失焦自毁基线 / 脏态点 ✕）
powershell -NoProfile -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\tmp\probe-batch-R7.ps1" -OutBase batchR7
# 报告 tmp/v-batchR7-report.txt
```

---

## 6. 状态

| 项 | 结果 |
|---|---|
| 单测 | `PASS=249 FAIL=0`（246 基线 + 3 条 T203 计数断言） |
| WeaselServer 构建 | 通过；`output/Win32/WeaselServer.exe` 2448384 B（16:39:14） |
| 安装包 | `output/archives/fluxing-1.2.1-installer.exe` 42973909 B（16:40:35） |
| L0 静态门禁 | `checks=15 pass=15 fail=0 skip=0`，EXIT=0 |
| 静默装机 | exit 0，`ALL HARD CHECKS PASSED`；装机副本 sha256 `2760DF26E1E4E2477FDEAF6C0C00E02BAD7861A04BD9D61D0547B34B19456690` 与构建产物**完全一致**；WeaselServer 由安装器重启（pid 366508 / 16:42:25）⇒ 验的就是新代码 |
| 真机回归 | `tmp/v-batchR8-report.txt`（P0 定向）+ `tmp/v-batchR9-report.txt`（全流程，含 chips / 下拉 / modal / 取消路径） |
| 真实配置 | **未改动**：`default.custom.yaml` 前后 sha256 `50A46CC7…6669D`、mtime `2026-11-09 01:41:18` 完全一致，全程未点「保存」 |

### 6.1 提交清单

| Commit | Scope | 内容 |
|---|---|---|
| `056f970` | `fix(WeaselServer)` | `ShortcutSettings.cpp` / `.h`：确认框存活期禁止自毁 + `CountDirtyChanges` 统一口径 |
| `7bc2ed2` | `test(shortcut-settings)` | `TestShortcutSettings.cpp`：`TestT203DirtyCount()` |
| （本块自身） | `docs(spec-045)` | 本 handoff + `CLAUDE.md` 指针 + `lessons-learned.md` L209 |

### 6.2 提交 hash

`056f970` / `7bc2ed2` / docs 块（本块自身，hash 见 `git log --oneline -5`）。
分支 `Fluxing`，相对 `kizemo/Fluxing` 领先 32（**未 push**）。

---

## 7. 下一步建议

1. **T201 方案下拉**按 §3 的建议方案实施（改动面 1 个文件 + 1 条 ADR），候选源切到共享目录。
2. **真机点「保存」**的验收仍需用户授权（会写 `default.custom.yaml` 并触发一次 Rime 部署），
   本批全程未点「保存」，用前后哈希 + mtime 证明配置未被改动。
3. `docs/adr/` 下一个可用编号：**0047**（T201 方案下拉候选源变更用）。
