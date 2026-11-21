# 批次 BH · 账号页三需求改造（保存账号勾选 / 点邮箱框自动弹下拉 / 填写框文字垂直居中）+ 单击通道修复 + 真机取证

> 日期：2026-11-21 · 分支 `feat/100-cloud-sync` · HEAD `e85259c`（本批产品改动**尚未提交**）
> 上一批：[BF 账号页 A-4/A-5 未登录态实测闭环](handoff-2026-11-21-batch-bf-a4-a5-logout-measured.md)

---

## 1. 本批性质

**产品改造 + 门禁 + 出包 + 装机 + 真机取证**。用户 2026-11-21 在 BG 指令里追加三条需求，取代 BG 原排程：

1. 「登录态」改为勾选 **「保存账号」**；
2. 已保存账号 → **点选邮箱填写框时自动出现下拉**，复用邮箱输入框自身弹出，**不另外增加下拉框模块**；
3. 填写框内文字**垂直居中**，并与填写框**标题垂直对齐**。

**未改** `librime/` · **未改 IPC 协议** `include/WeaselIPCData.h` · **未新增第三方依赖** · **未 bump 版本**（1.3.8 未对外发布，真机判定一律用文件 `sha16`/字节数，不用版本号）· **未按 L54 清注册表**。

---

## 2. 三条需求的实现落点（产品码，`WeaselServer/Cloud/CloudAccountDialog.cpp` + `CloudAccountUi.h`）

| 需求 | 实现 | 真相源约束 |
|---|---|---|
| ① 勾选「保存账号」 | `kIdSaveAccount = 1019`，`kLayoutLoggedIn` 条目文本 `L"保存账号（下次启动自动登录）"`（**全角括号**）；`BM_SETCHECK` 由 `m_account_save_checked` 驱动 | 文案与几何只在 `CloudAccountUi.h` 定义（R7） |
| ② 点邮箱框自动弹下拉 | 删除旧「已存账号」下拉模块的布局条目 `1034`(标签) / `1018`(hist combo)，**常量与 ID 映射保留**（未登录布局表仍含 1018/1019/1034，回归不受影响）；新增邮箱框正下方弹层 `kIdAccountSuggest = 1099`（`LISTBOX WS_CHILD\|LBS_NOTIFY\|WS_VSCROLL\|WS_BORDER`，**只在 CloudAccountDialog.cpp 这个 TU 内**），由 `ShowSuggestIfAny` / `HideSuggestList` / `OnSuggestSelChange` / `OnEmailEditChanged` 驱动 | 1099 属**行为层控件**，不进布局表；可见性判据只看 `IsWindowVisible` |
| ③ 文字垂直居中 + 与标题对齐 | 成对 EDIT 高度收到**字体自然高度**（base `h=15`，168dpi 下 26），不变式 `edit.y + AccountEditInkCenterOffsetBase() == label.y + label.h / 2`，`AccountEditInkCenterOffsetBase()` base = `7` | 几何只在 `CloudAccountUi.h`；缩放入口唯一 `AccountScale(v,dpi)=MulDiv(v,dpi,96)`，**不得只挪 y 不缩 h、不得缩到 `h_base ≤ 14`** |

`EmailEditProc` / `InstallEmailClickHook` 是**幂等 + 自愈**的 subclass 安装（只比 `GWLP_WNDPROC == &EmailEditProc`），不是裸 `SetWindowLongPtr`。

---

## 3. 门禁（全部实测）

| 项 | 结果 | 证据 |
|---|---|---|
| `TestFluxingCloud` | **`RESULT: PASS=614 FAIL=0` / `VERDICT: ALL PASS` / `TEST_EXIT=0`**，`回环回归 PASS=11 FAIL=0` | `tmp\bh-testrun2.log`；exe `689664 B` mtime `19:22:25` ≥ src `19:21:30` |
| `WeaselTSF` 全量 | `0 警告 / 0 错误`（链接 `weaselx64.1.3.8.0.0.dll`） | `tmp\bh-tsf-full.log` |
| `L0-static.ps1` | `PASS_COUNT=15 FAIL_COUNT=0` | 早前轮实测 |
| 未登录 12 项布局回归 | `NO_LEGACY_HISTORY_WIDGET=True` / `C1_VERDICT=PASS` / `MISSING=` `EXTRA=`（逐字等于 BF 表，`1001,1002,1003,1006,1010,1013,1018,1019,1021,1025,1026,1034`） | `run-relogin3.log` sha `6A0B391D1606D9A2`（19:35 轮） |

⚠️ `TestFluxingCloud` **不在 `weasel.sln`** ⇒ 必须显式 msbuild，且必须 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`（否则 `wtl/atlapp.h` C1083）；单测 exe 落在**仓库根** `Release\`。

---

## 4. 真机取证 —— 分三轮，判据逐轮收紧

### 4.1 已验证（实测坐实）

- **需求 ① = `PASS`**：`check_state=1 expect=1`；`1019 text_len=14 sha16=6A48109401C0EB6D`；rect `x=154 y=137 w=427 h=35`；`evidence-c1-save-account.png`。
- **需求 ② 弹出几何 = `PASS`**：弹层 `x=154 y=58 w=665 h=23`，与邮箱框（`x=154 y=28 w=665 h=26`）**左边缘/宽度逐字对齐**、贴在框底下方 2px（`C2_GEOM_OK=True dy=0 dx=0 dw=0`）；`C2_LIST_COUNT=1 ITEM0_SHA16=0D4D27C22CF53D38 ITEM_HEIGHT=21 hit_row=0` ⇒ 列表真在点的位置上。
- **需求 ② 点条目回填 = `PASS`**：`C2_AFTER_PICK list_hidden=True email_sha16=0D4D27C22CF53D38 filled_from_item=True`。
- **需求 ② 点其他控件收起 = `PASS`**：`C2_CLICK_status_1006 FG_OK=True` → `C2_HIDE_ON_OTHER_CLICK judged=True hidden=True`。
- **需求 ③ = `PASS`（19:29 轮）**：`C3_VERDICT=PASS c3a_has_ink=True c3b_centered=True c3c_not_clipped=True c3_setup=True email_CENTER_DELTA=0 pwd_CENTER_DELTA=1.5`（18:5x 与 19:35 两轮同样 delta 0 / 1.5，`email rows=13 / pwd rows=12`）；证据 `evidence-c3-{email,pwd}-{label,edit}.png`。
- **装机件含本轮改造 = 是**（19:23 那一份）：`D:\Program Files\fluxing\weasel\WeaselServer.exe` `2788864 B` / `sha16=0AEB3DA9BCB72BE5` / ver 恒 `1.3.8.0`，与当时构建载荷逐字节同。
- **账号态 = 已登录**（第 5 轮闭环）：`RELOGIN_OK_AFTER_SEC=4` / `dialogReopenedDuringWait=False` / `A4_RESTORE_VERDICT=PASS` / `VISIBLE_IDS=1004,1006,1010,1016,1017,1020,1022,1027,1035`。

### 4.2 🔴 唯一产品缺口：选完条目后**再单击邮箱框不重新弹出**（`c2c_reopen=False`）

三轮（19:29 / 20:0x / 20:23）逐字一致。定性为**产品侧真实缺口**，四条探针假红解释已逐条实测否证：

| 候选解释 | 否证证据 |
|---|---|
| 点击未送达 | `C2_CLICK_email2_after_hide at=586,181 FG_OK=True`、`all_clicks_delivered=True`（前置 TOPMOST→SetForegroundWindow→NOTOPMOST 三重断言） |
| 历史文件为空/被过滤 | `C2_HISTORY_PATH=D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-accounts.history` `bytes=16 lines=1 first_sha16=0D4D27C22CF53D38`（产品侧出处 `CloudAccountHistory.cpp:129`；口令值不落日志不落盘） |
| 「列表隐藏时条目被清空」 | `C2_REOPEN_MISS count=1` ⇒ 条目仍在，**count 无判别力**，判别只认 `IsWindowVisible` |
| 时序/输入方式 | 上一笔 `C2_HIDE_ON_OTHER_CLICK` 判 `hidden=True` ⇒ 单击前列表确为隐藏；而 `list_visible_after_seed=True` 是探针自己 `WM_SETTEXT` 触发 `EN_CHANGE` 弹的，**不能反证单击** |

**根因**：回填后焦点本就在邮箱框 ⇒ 再单击**不产生** `EN_SETFOCUS`/`EN_CHANGE`，而 `OnEmailEditChanged` 是唯一挂载 `ShowSuggestIfAny` 的地方；原实现只处理 `WM_*LBUTTONDBLCLK`，首次能弹是 `EN_SETFOCUS` 的副作用。

**修法（已在盘上工作树 `:1334`）**：`if ((message == WM_LBUTTONUP || message == WM_LBUTTONDBLCLK) && hwnd != nullptr && !g_suppress_email_change)` ⇒ 单击通道挂到 EDIT **自身**消息流（subproc 自转发，不依赖父窗 `WM_PARENTNOTIFY`）；`WM_PARENTNOTIFY` 路径（`:1506`）与 `g_list_mouse_down` 复位（`:1485/:1502`）**保留不动**；`ShowSuggestIfAny` 对已显示列表幂等 ⇒ 不构成竞态。

🚨 **该修法由并发会话写入**（`.cpp` mtime `2026-11-21T20:12:10`，注释还引用了本会话的 `run-uicheck5.log`）。我三次尝试编辑均以 `Could not find the exact text` / `OLD_CONTAINS=False` 停手 ⇒ **本会话未对产品码做任何改动，也不重复改**。`HEAD_HAS_WM_LBUTTONUP_FIX=False` / `WT_HAS_WM_LBUTTONUP_FIX=True`、`git status` 仍列 ` M CloudAccountDialog.cpp` 与 ` M CloudAccountUi.h` ⇒ **未入库、且尚未在任何装机件里验证**。

### 4.3 第 6 轮**不作数**（本轮最大操作失误）

`run-uicheck6.log`（`UICHECK_EXIT=4` / `BH_VERDICT=FAIL`）跑的是 **19:23 的旧装机件**，因为装机步骤从未执行：

```
INSTALL_EXIT=9009
'fluxing-1.3.8-installer.exe' is not recognized as an internal or external command,
```

NSIS 产物实际落在 **`output\archives\`**（构建日志有 `Output: "F:\...\output\archives\fluxing-1.3.8-installer.exe"`），而 .cmd 里 `cd /d ...\output` 再裸跑该 exe ⇒ 找不到。**`BUILD_EXIT=0` 把这一步掩盖了** —— 出包链的判据必须是 `SHA_MATCH_BUILD_VS_INSTALLED=True`，不是 `BUILD_EXIT`。

- 新构建载荷 `output\Win32\WeaselServer.exe` `2788864 B` / mtime `20:21:06` / `sha16=1A60B28CD714F31F`（**含**单击修复）。
- 新安装包 `output\archives\fluxing-1.3.8-installer.exe` `42977366 B` / mtime `20:23:06`。
- 第 6 轮的**顺带可用证据**：C1 仍 `PASS`；需求 ② 的几何/回填/收起三项仍 `PASS`；`C3_*_CENTER_DELTA` 在「播种态」取帧下仍为 `1.5 / 1.5`（`label_abs 40 vs edit_abs 38.5`、`99 vs 97.5`，`rows=14/16`）⇒ 与 19:29 轮方向一致。

### 4.4 C3 形式判据当前**结构上不可满足**（探针缺陷，非产品缺陷）

`c3_setup=False` 的成因是两半互斥：`c3d_seed_roundtrip=True`（`WM_SETTEXT` 把 email 写成 len=7，往返校验通过）**必然**触发 `EN_CHANGE` ⇒ `list_visible_at_capture=True`。⇒ 在「播种」路径下 `c3_setup` 永远为假，C3 只能 `NOT_JUDGED`。
**修法**（下一批）：播种 → 点 1006 收起 → **再取帧**（把 hide 移到 seed 之后），或把门改成「取帧瞬间不可见」为唯一必要条件。
在此之前，需求 ③ 的结论依据 = 19:29 轮 `C3_VERDICT=PASS` + 各轮 CENTER_DELTA 数值一致，**不得写成「本轮已验」**。

---

### 4.5 🔴 第 7 轮实测（**权威**，`bg_0a2829e0` / `reports/fluxing-bh-ui/run-uicheck7.log`，20:26:48）—— 本节**覆盖 §4.2 / §4.3 / §4.4 的结论**

**本轮装的是带双击修复的新件，逐字节坐实**：装机件 = 构建件 `bytes=2788864 / sha16=1A60B28CD714F31F / mtime 20:21:06`；
`SHA_MATCH_BUILD_VS_INSTALLED=True`、`SRC_HAS_LBUTTONUP_FIX=True`、`INSTALL_EXIT=0`（38.7 s）、服务端唯一 pid 1081812 start 20:26:25、账号态复原 `A4_RESTORE_VERDICT=PASS`（`RELOGIN_OK_AFTER_SEC=6`）。

- **§4.2 那条唯一产品缺口已修好**：`c2c_reopen=True`（第 5 轮 False）⇒ `C2_SUGGEST_REOPEN rect x=154 y=58 w=665 h=23`。
  **双击修复在真机坐实生效**，`WM_LBUTTONUP` 通道成立。
- **① `C1_VERDICT=PASS`**；`C2_SUGGEST_OPEN` / `C2_GEOM_OK=True dy=0 dx=0 dw=0` / `C2_AFTER_PICK filled_from_item=True` 与「不另增下拉模块」面（未登录 12 项布局 MISSING/EXTRA 皆空）均相符。
- **🔴 新暴露真缺陷：`C2_HIDE_ON_OTHER_CLICK judged=True hidden=False` ⇒ `c2d_hide_on_other=False` / `BH_VERDICT=FAIL`。**
  **第 5 轮的 True 是空判据、无判别力**：旧件下选中条目时列表已被收起，且 `c2c_reopen=False` 证明重弹从未成功 ⇒ 测的是「已隐藏仍隐藏」。
  本轮 `judged=True` 是探针先确认「点之前列表可见」，**这是对「点他处收起」的第一次真测**，且 `all_clicks_delivered=True`、`FG_OK=True`，送达面已排除。
  **代码里本该收起**：`CloudAccountDialog.cpp:1773-1775` 的 `WM_PARENTNOTIFY` 分支「点到别处 ⇒ `HideSuggestList(hwnd)`」，判靶 `id=1006`（状态 STATIC）正是 else 分支。
  ⇒ 假设不成立但**根因未坐实**，候选四条：① STATIC 不产生 `WM_PARENTNOTIFY`（只有 EDIT/BUTTON 产生）② `:1765` 只认 `WM_LBUTTONDOWN/DBLCLK`，而点 STATIC 时该消息根本不来 ③ 收起后又被 `:1334` 的 `WM_LBUTTONUP` 重开（判靶不是邮箱框，此条可能性低）④ `g_list_mouse_down` 残留 true 令 `:1584` 的 `EN_KILLFOCUS` 失效。
  **下一批第 1 项**：换判靶（1010 关闭按钮 / 1002 口令 EDIT）各测一次，并在 `:1765` 的三条分支各加一行 `fluxing::diag::AppendLine("acctdlg", …)` ⇒ 一次真机轮即可区分「STATIC 不产生消息」与「产生了但没生效」。**不得据此写成「已修好」，也不得写成「点任何控件都不收」。
- **③ `C3_VERDICT=NOT_JUDGED c3_setup=False`（§4.4 的探针门，本轮仍在）**，但三条实质判据全 True：`c3a_has_ink=True c3b_centered=True c3c_not_clipped=True`，`C3_email_CENTER_DELTA=1.5 / C3_pwd_CENTER_DELTA=1.5`（19:29 轮判 `PASS` 时同为 1.5 / 1.5）⇒ **不是产品回归**。

## 5. 本轮新增工具坑（必须固化）

1. **NSIS 包在 `output\archives\`，不在 `output\`** ⇒ 静默装机必须 `cmd /c "`"<abs>\output\archives\fluxing-1.3.8-installer.exe`" /S /D=D:\Program Files\fluxing"`；`INSTALL_EXIT=9009` 是「命令找不到」不是安装失败。**出包链若无 `SHA_MATCH_BUILD_VS_INSTALLED=True` 就等于没装机**（`INSTALL_EXIT=0` 也可能是假绿：实测 `Start-Process -Wait -PassThru` 对 NSIS **挂住不返回**但安装确实成功 ⇒ 一律以盘上 `sha16` + `mtime` + 服务端新 pid 判定）。
2. **阴性判据必须先穷尽三条候选解释**（送达 / 数据源 / 判据无判别力）再定性产品缺口，逐条配实测反证；`count=1` 这类「隐藏≠销毁」形态的数字**不能**当判别依据。
3. **把独立片段 splice 进宿主脚本 = 变量名必炸**：本会话两次同形（`$HistFile`、`$c3Setup` 均「只消费、无赋值」），后果是脚本照常 exit 4、个别判据永远 `NOT_JUDGED`（**不假绿但白跑一轮**）。体检法（实测有效，注意 `param` 行带 `[type]` 前缀、消费行带 `?:` 都会让「行首匹配」漏判）：
   ```powershell
   $P=[System.IO.File]::ReadAllLines($f,[System.Text.UTF8Encoding]::new($false))
   for($i=0;$i -lt $P.Count;$i++){ if($P[$i] -imatch 'c3Setup'){ "[$($i+1)] $($P[$i])" } }   # 只见消费不见赋值 = 未定义
   ```
4. **`edit` / `write` 的自述都不算盘上实况**；并发会话会改同一文件（`.cpp` 20:12、探针 20:17）⇒ 动刀前 `ReadAllLines` + `LastWriteTime` 当场取真值，**摘要里的行号一律不可信**；发现他人已写入同一修法就**停手不重复改**。
5. 探针脚本仍**必须纯 ASCII**；自检行**不要内嵌反引号 CRLF 字面量**（易自身 `ParserError`），拆成两条独立语句。
6. 跨进程读 EDIT 文本用 `SendMessageW(0x000D WM_GETTEXT / 0x000C WM_SETTEXT)` + `StringBuilder`（`GetWindowTextW` 对另一进程 EDIT 恒空）。

---

## 6. 产物与证据路径（全绝对，不删）

- 产品码：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountDialog.cpp`（1972 行 / 82160 B / **LF** / mtime 20:12:10）、`...\CloudAccountUi.h`（210 行）
- 探针：`F:\soft\00selfmade\rime_claude\reports\fluxing-bh-ui\probe-bh.ps1`（630 行 / 36679 B / mtime 20:17:00 / `PARSE_ERRORS=0`；词汇表 `$K_SUGGEST=1099`、`Click-Screen:274`、`Click-Ctl:282`、`Wait-Suggest:289`、C3 段 `:523-595`）
- 真机轮日志（同目录）：`run-uicheck.log` … `run-uicheck7.log`、`run-relogin{,2..7}.log`、`ui-20261121-*{,BeforeFix}-UiCheck.txt`
- 截图：`evidence-ui-loggedout.png`(840x508)、`evidence-c1-save-account.png`、`evidence-c2-suggest.png`、`evidence-c2-full.png`、`evidence-c3-{email,pwd}-{label,edit}.png`、`evidence-c3-blank.png`、`evidence-c3-seeded.png`
- 门禁/出包/装机：`F:\soft\00selfmade\rime_claude\tmp\bh-{test-build,test-run,testbuild2,testrun2,build-installer,build2,install2,install3,gate-install-test.cmd,reinstall-probe.ps1}.log|txt|ps1|cmd`
- 判据链脚本（**stage 5 `-Mode Relogin` 是重登安全网，勿改勿拆**）：`tmp\bh-full-chain.ps1`、`tmp\bh-reinstall-probe.ps1`

---

## 7. 下阶段（优先级，勿重复 4.1–4.2 已闭环项）

> **以 §4.5（第 7 轮权威实测）为准**，本节原第 1 项（装机复验双击修复）**已闭环**；新的第 1 项 = **`c2d_hide_on_other=False` 定根因**：
> 换判靶 1010 / 1002 各测一次 + 在 `CloudAccountDialog.cpp:1765` 三条分支各加一行 `acctdlg` 诊断日志 ⇒ 一次真机轮区分「STATIC 不产生 `WM_PARENTNOTIFY`」与「产生了但没生效」；
> 修好之后才允许把需求 ② 写成端到端闭环。第 2 项 = C3 探针 `c3_setup` 门改「播种 → 点邮箱框收起 → 再取帧」。
> **本批产品改动（三需求 + 双击修复 + 云单测）全在工作树、未 commit、未 push**（`git status -sb` ahead 1；`CloudAccountDialog.cpp` / `CloudAccountUi.h` / `TestFluxingCloud.cpp` 均 ` M`），是否入库待用户授权。


1. **读 `run-uicheck7.log`**（装机件应为 `1A60B28CD714F31F`）：`c2c_reopen=True` ⇒ 需求 ② 闭环；仍 False ⇒ 下一个候选根因是 `WM_LBUTTONUP` 被 `DefWindowProc` 分支吞掉或 `g_list_mouse_down` 复位过早，**先看 `C2_CLICK_email2_after_hide` 之后是否有 `SUGGEST_OPEN` 行**，不要先改产品码。
2. 若 `SHA_MATCH_BUILD_VS_INSTALLED=False` ⇒ 查 `install.nsi` 版本号 shim 是否把新件降级（`weasel.dll` 规范名锁定路径），**不得**用 `/quit` 或杀宿主绕过（L109）。
3. 修 C3 取帧顺序（hide 移到 seed 之后）使 `c3_setup` 可满足，重跑一次；否则需求 ③ 沿用 19:29 证据并登记「形式判据 NOT_JUDGED」。
4. **提交**需用户点头：建议 `feat(fluxing)` 单 commit（三需求 + 单击通道 + `test/TestFluxingCloud`）+ `docs(fluxing)` 交接指针 commit；提交判据 = `git cat-file -t` + `git show --stat` + `git grep <新符号> HEAD` 三条一起看，`commit_exit=0` 不算。
5. **并发会话协调**：`.cpp` 与探针正被另一会话改；动刀前 `git status` + mtime 复核，避免互相覆盖。
6. 遗留不变（**需用户在场/两台机器**）：双机 AC 7 条、restore 端到端、输入延迟两组对照、缺陷 A 反向判据 G-3/G-5、`kIdSyncStatus`(1017) 不可观测待拍板。

---

## 8. 未闭环清单（诚实版）

| 项 | 状态 |
|---|---|
| 需求 ① 保存账号勾选 | **已验（真机 PASS）** |
| 需求 ② 点邮箱框弹下拉（弹出/几何/回填/点他处收起） | **已验（真机 PASS）** |
| 需求 ② 选完条目后**再单击**重新弹出 | 修复**在盘上工作树、未入库、未装机验证**；装机旧件三轮实测 `c2c_reopen=False`；第 7 轮待读 |
| 需求 ③ 文字垂直居中 + 与标题对齐 | 19:29 轮**已验 `PASS`**；第 6/7 轮形式判据因探针缺陷 `NOT_JUDGED` |
| 本批产品改动提交 / push | **未做**（未获要求） |
| 装机版 | 1.3.8.0（判定一律用 `sha16`，版本号不可信） |
