# 批次 BH · 账号页三需求改造（保存账号勾选 / 点邮箱框自动弹下拉 / 填写框文字垂直居中）+ 单击通道修复 + 真机取证

> 日期：2026-11-21 · 分支 `feat/100-cloud-sync` · **⚠️ 原文标题行写「HEAD `e85259c`（本批产品改动尚未提交）」—— 该说法已过时且易误读**（`e85259c` 实为 **BG 批次**提交，见 §9.1）；**本批产品改动已入库 = `3450472`（代码）+ `76b9e70`（文档与证据），已 push**；§4.6 / §4.7 / §9 / §10 为 2026-11-21 由 BI 批次回写合并。
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

### 4.6 ✅ 第 8 轮（`reports/fluxing-bh-ui/run-uicheck8.log`，20:58:42）—— **c2d 根因修复后首次真机复验**（**未改探针**）

装机件 = 构建件逐字相等（`SHA_MATCH_BUILD_VS_INSTALLED=True`，装机载荷 `65565EF293F91236`），服务端唯一 pid **479804**（start 20:57:58，ver 1.3.8.0），本机自然 DPI 168（175%）。

- **🔴 §4.5 暴露的 `c2d_hide_on_other=False` 已修**：`C2_VERDICT=PASS c2a_open=True c2b_pick=True c2c_reopen=True c2d_hide_on_other=True c2d_judged=True all_clicks_delivered=True`。
  `c2d_judged=True` ⇒ 探针先确认「点之前列表可见」⇒ 这是**对「点他处收起」的第二次真测**，判据**有判别力**。
  送达面已排除：`C2_CLICK_status_1006 at=520,434 FG_OK=True` + `all_clicks_delivered=True`。
- **① `C1_VERDICT=PASS`**；**② 弹出 / 几何 / 回填 / 重弹 / 收起 五项全 True**。
- **③ `C3_VERDICT=NOT_JUDGED c3_setup=False`**（§4.4 的探针门本轮仍在），但三条实质判据全 True：`c3a_has_ink=True c3b_centered=True c3c_not_clipped=True`，`C3_email_CENTER_DELTA=1.5 / C3_pwd_CENTER_DELTA=1.5`，正控制 `C3_POSCTL_OK=True`。
- 同轮总判定 = **`BH_VERDICT=FAIL EXIT=4`** ⇒ **这不是需求失败，是探针门未满足**（`c3_setup=False` ⇒ 形式判据 `NOT_JUDGED` ⇒ 探针按设计判 FAIL）。**不得据此写成「需求 ③ 未通过」，也不得写成「c2d 修好前 8 轮失败」**。

### 4.7 ✅ 第 9 轮（`reports/fluxing-bh-ui/run-uicheck9.log`，21:03:22）—— **全套 `BH_VERDICT=PASS`（权威终态）**

**只改探针取样判据，不改产品码**：`probe-bh.ps1` L553–571（+13 行）把密码框「种子文本读回」由 `WM_GETTEXT` 改为 `EM_GETTEXTLEN(0x00C1)` + **同帧客户区裁剪 `Crop-Ink` 墨迹行数**双通道（**两向都成立才放行，不凑绿**）；改前留档 `probe-bh.ps1.bak-pass8`（630 行 / 36679 B / 20:17:00）。**`ES_PASSWORD` 跨进程 `WM_GETTEXT` 恒返空 ⇒ 原判据对任何构建永不成立**，这是探针缺陷不是产品缺陷。

- **`C3_SEED_CAPTURE size=840x508 … email_after_hide_len=7 pwd_after_hide_len=0 pwd_EM_GETTEXTLEN=8 pwd_ink_rows=16 pwd_seeded=True list_visible_at_capture=False c3_setup=True`** ⇒ 探针门**首次满足**。
- **`C3_VERDICT=PASS c3a_has_ink=True c3b_centered=True c3c_not_clipped=True c3_setup=True`**；`C3_email_CENTER_DELTA=1.5` / `C3_pwd_CENTER_DELTA=1.5` / `C3_POSCTL_OK=True`（与 19:29 轮、第 8 轮逐字一致）。
- **`C2_VERDICT=PASS`（c2a–c2d 全 True）**、`C1_VERDICT=PASS` ⇒ **`BH_VERDICT=PASS EXIT=0`**。
- **因果证据到手**（`%TEMP%\fluxing-profile-notify.log`，切片 319772→320372→320886 B）—— 两条收起分支**各被独立命中**：
  - 子控件点击：`c2d parentnotify down evt=513 id=0` + `c2d parentnotify hide id=0`
  - **c2d 判靶那一击（点 STATIC `1006`）走新分支**：`[20:58:41.385][acctdlg] c2d parent lbuttondown hide`（第 9 轮另见 21:03:20.020 / 21:03:21.454 两条）
- 账号态已复原为**已登录**（`RELOGIN_OK_AFTER_SEC=4/5`、`A4_RESTORE_VERDICT=PASS`）；全程未 `/quit`、未杀宿主、未改注册表。

**⇒ 三条需求 + c2d 修复在第 9 轮全部真机 PASS**（装机版 1.3.8.0，服务端唯一 pid 479804）。

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

> ⚠️ **本节已被 §4.6 / §4.7 覆盖 —— 原第 1、2 项均已闭环**：
> 原第 1 项（c2d 定根因）= 已定根因（BI §3）并修复，真机第 8/9 轮 PASS；
> 原第 2 项（C3 探针 `c3_setup` 门改「播种 → 点邮箱框收起 → 再取帧」）= 已改为 `EM_GETTEXTLEN` + 同帧墨迹双通道，第 9 轮 `c3_setup=True` / `C3_VERDICT=PASS`。
> **本批产品改动（三需求 + 双击修复 + c2d 修复 + 云单测）已入库** = commit `3450472`（代码）+ `76b9e70`（文档与证据），**已 push** 到 `kizemo/feat/100-cloud-sync`；其后本会话又入库 `4ab2e1e`（A-4/A-5 探针索引修复）。
> **下方第 1–5 项保留原样作历史记录；现役未闭环项以 §8 表 + §9 + §10 为准。**
>
> ~~原第 1 项：以 §4.5（第 7 轮权威实测）为准……~~
> ~~原第 2 项 = C3 探针 `c3_setup` 门改「播种 → 点邮箱框收起 → 再取帧」。~~
> ~~**本批产品改动（三需求 + 双击修复 + 云单测）全在工作树、未 commit、未 push**（`git status -sb` ahead 1；`CloudAccountDialog.cpp` / `CloudAccountUi.h` / `TestFluxingCloud.cpp` 均 ` M`），是否入库待用户授权。~~


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
| 需求 ① 保存账号勾选 | **已验（真机 PASS）**：第 8/9 轮 `C1_VERDICT=PASS`，`NO_LEGACY_HISTORY_WIDGET=True`，文案 sha16 `6A48109401C0EB6D` |
| 需求 ② 点邮箱框弹下拉（弹出 / 几何 / 回填 / 重弹 / 点他处收起） | **已验（真机 PASS）**：第 8/9 轮 `C2_VERDICT=PASS`，`c2a_open / c2b_pick / c2c_reopen / c2d_hide_on_other` 全 True，`c2d_judged=True`，`all_clicks_delivered=True` |
| 需求 ② 选完条目后**再单击**重新弹出（`c2c_reopen`） | **已验（真机 PASS）**：第 7 轮起为 True（第 5/6 轮旧件 `False` 是**修好前的真实缺口**，非探针问题） |
| 需求 ③ 文字垂直居中 + 与标题对齐 | **已验（真机 PASS）**：第 9 轮 `C3_VERDICT=PASS c3_setup=True`，`CENTER_DELTA=1.5 / 1.5`，正控制 `C3_POSCTL_OK=True`；第 6/7/8 轮 `NOT_JUDGED` 属**探针门缺陷**（`ES_PASSWORD` 跨进程 `WM_GETTEXT` 恒空），已修探针 |
| 本批产品改动提交 / push | **已做**：代码 `3450472` + 文档 `76b9e70`（**已 push** 到 `kizemo`）；其后 `4ab2e1e` = A-4/A-5 探针索引修复（见 §9.2 与 BJ handoff） |
| 装机版 | 1.3.8.0（判定一律用 `sha16`，版本号不可信）：装机载荷 sha16 `65565EF293F91236`，包 sha16 `3532922B872B3C7C`，服务端唯一 pid 479804 |

## 9. 🔴 提交实况与一条错误登记的更正（由 BI §8 回写合并，2026-11-21）

> 本节来源 = 批次 BI handoff §8。BI §7 记载：**并行会话曾用 `Copy-Item` 还原把本 handoff 的 §4.6/§4.7 追加与 `CLAUDE.md` 的 BG 指针行抹掉**（还原保留源 mtime ⇒ mtime 早于编辑时刻即还原证据）。故判据文档**必须尽快入库**。

### 9.1 🔴 更正：「BH 改动已提交 `e85259c`」为**假**

- `e85259c` = `test(ci): 批次 BG · G-3/G-5 维护闸门造态能力与取证脚本`，**不含 `WeaselServer/` 任何文件**。
- 硬证据：`git log -S 'ShowSuggestIfAny' -- WeaselServer/Cloud/CloudAccountDialog.cpp` **全历史 0 命中**；`git show HEAD:WeaselServer/Cloud/CloudAccountDialog.cpp` 内 `ShowSuggestIfAny` / `c2d parent` **均不存在**；`git show HEAD:WeaselServer/Cloud/CloudAccountUi.h` 内 `kIdAccountSuggest` **不存在**。
- ⇒ **BH 三需求 + c2d 修复此前整体只存在于工作区**，从未入库；**真正入库 = `3450472`（代码）+ `76b9e70`（文档与证据）**。
- ⚠️ **本 handoff 原文标题行的「HEAD `e85259c`」正是这条误解的来源** —— `e85259c` 实为 **BG 批次**（`test(ci)`）的提交，与本批产品改动**无关**；且原文同句已写「本批产品改动尚未提交」，二者并列易被读成「改动已入库在 e85259c」。标题行已按 §9.2 订正。

### 9.2 提交实况

- **commit A** = `3450472 feat(fluxing): 云端账号页三项 UI 需求 + 已保存账号下拉点他处收起` ⇒ **3 files changed, 346 insertions(+), 47 deletions(-)**：`WeaselServer/Cloud/CloudAccountDialog.cpp` +307、`CloudAccountUi.h` +40、`test/TestFluxingCloud/TestFluxingCloud.cpp` +46。（BH 三需求与 c2d 补丁在**同一份未提交 diff 里无法单独切出** ⇒ 合为一个 `feat(fluxing)`；提交范围属工程判断，按 2026-10-14 用户授权自行裁定并汇报，不发问卷。）
- **commit B** = `76b9e70 docs(fluxing): 批次 BH/BI 交接文档、接力 prompt 与真机证据` ⇒ **25 files changed, 1440 insertions(+), 1 deletion(-)**：`CLAUDE.md` 指针 + BH/BI 两份 handoff + 两份接力 prompt + `reports/fluxing-bh-ui/`（10 张 PNG + `probe-bh.ps1` 643 行 + 9 份 `ui-*-UiCheck/SelfTest.txt`）。
- **三条判据同轮一起取**（`commit_exit=0` **不算**判据）：`git cat-file -t` 两者均 `commit`；`git show --stat` 如上；`git grep -F 'c2d parent' HEAD` = **3 命中**；HEAD 内 `case WM_LBUTTONDOWN:` True、`c2d parentnotify hide id=` True、`ShowSuggestIfAny` 7 处、`保存账号（下次启动自动登录）` 在 `:932`、`kIdEmailLabel` cpp 2 / .h 4。
- **push**：`d7722e2..76b9e70 → kizemo/feat/100-cloud-sync`，`PUSH_EXIT=0`；`git ls-remote kizemo refs/heads/feat/100-cloud-sync` = `76b9e7016771bd6c20d181fae95154cbfb7c3032`。
- **其后**：本会话另入库 `4ab2e1e fix(fluxing): 修 A-4/A-5 探针有序字典整数索引越界并补真机复验证据`（BF 批次资产，**尚未 push**）。

### 9.3 ⚠️ 两处早先不精确表述的订正

1. 需求 ③ 的实现符号**不是** `CenterV(...)`：HEAD 内 `CenterV` **0 命中**，`CloudAccountUi.h:118` 那段声明是**注释掉的** ⇒ 真实现 = `CloudAccountDialog.cpp:1202` 的 `const int line_px = tm.tmHeight + tm.tmExternalLeading;`（配合不变式 `edit.y + AccountEditInkCenterOffsetBase() == label.y + label.h / 2`）。
2. `c1_save_account` / `save account=checked` **不是代码字面量**（HEAD 源码 0 命中）⇒ 它们是**探针输出字段名**；入库判据以代码里的中文文案 `保存账号（下次启动自动登录）` + 盘上探针字段 `C1_SAVE_ACCOUNT check_state=1` 为准。

### 9.4 ⚠️ `.gitignore` 挡住 18 份判据原文（**待用户定**）

`reports/fluxing-bh-ui/run-uicheck.log … run-uicheck9.log`、`run-relogin.log … run-relogin9.log` 均未入库 ⇒ 第 9 轮权威原文 `run-uicheck9.log`（`BH_VERDICT=PASS / EXIT=0`）**只活在盘上**。已入库的同内容判据 = `ui-20261121-205842-UiCheck.txt`（55 行，第 8 轮）与 `ui-20261121-210322-UiCheck.txt`（56 行，第 9 轮；实测含 `c2d_hide_on_other=True` + `C3_VERDICT=PASS` + `BH_VERDICT=PASS`）。要让 `.log` 原文也入库需 `git add -f`，**待用户拍板**。

## 10. 本批遗留（转交下一批）

- **BG 遗留**（**需两台机器 + 用户在场**）：双机 AC 7 条、restore 端到端、输入延迟两组对照、G-3/G-5 真跑（配方 `maintenance\BG-G35-conditional-feasibility.md`）。判定口径 = **`BLOCKED_HARNESS_PRECONDITION`**（环境前置不可得，**非缺陷、非 PASS、也非 FAIL**），可行性 = **CONDITIONAL YES**。
- **`kIdSyncStatus`(1017) 不可观测待拍板**：刷新路径只写 `kIdStatus`(1006)，从不写 1017；最小修法约 3 行产品码 ⇒ 属**独立 ADR 批次**。
- **`.gitignore` 挡住 18 份 `run-*.log`**：入库需 `git add -f`，**待用户定**。
- **`tmp\bh-c2d-fmt-check.cpp`（83767 B，clang-format 对照副本）删不掉**：本机 `Remove-Item` 被硬策略整条拒、`rm --` 报 `requires the targets-only trash marker` ⇒ **需用户手动处置**（资源管理器/回收站侧）。
- **勿重跑**：A-1~A-5、B-1~B-5、`/verify-client-pid`、175%/200% DPI 切档（已拍板）、c2d 复验（已 PASS）、c3 量值（已 PASS）。
