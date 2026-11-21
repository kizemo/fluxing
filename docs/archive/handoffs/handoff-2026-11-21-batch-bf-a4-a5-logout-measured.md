# 批次 BF · 账号页 A-4/A-5 未登录态实测闭环 + A-5 探针重写（2026-11-21）

> **本批性质** = 纯取证 + 探针批次。
> **产品代码零改动**：未改 `librime/`、未改 IPC 协议消息类型、未改 `WeaselTSF/**` / `include/**` / `install.nsi` / 版本号、未新增依赖、**未出包**、**未装机**。
> 装机版仍 **1.3.8.0**，运行中 `WeaselServer` 唯一 pid **1025692**；本批全程**未 `/quit`、未杀任何 TSF 宿主、未改任何注册表持久项**。
> **接手请先读 §1**。

---

## 1. 本批做了什么（一句话）

把 BE 遗留的 §7 第 5 项「A-4/A-5 未登录态」从**未测**变**实测闭环**：
**A-4 = `PASS`（累计 4 个独立轮次结论一致）**；
**A-5 = 清单两条判据（两态同高 + 底部留白不裁）全部实测到手**，另有一条**自检项待定性已在本批定性为探针口径问题**。
顺带把 BB 需要的「175% DPI 真机像素证据」补齐了（本机自然处于 175%，未主动切档）。

顺序固定为：Phase 0（口令可重登判别性）→ Baseline（已登录态正控制）→ Logout（A-4）→ A-5 未登录态几何/留白/像素剖面 → Relogin（复原核验）。

---

## 2. A-4 · 未登录态不出现换钥提示行 = **实测 `PASS`**

**判据（本批定案口径，两条同时成立才算 PASS）**

1. 控件 `kIdKeyRotationNote`（**id=1035**）**存在但 `IsWindowVisible = false`**；
2. 可见 id 集合 **逐字等于**未登录态 12 项布局表（`MISSING_IDS` 与 `EXTRA_IDS` 均为空）。

实测：`A4_VISIBLE_IDS=1001,1002,1003,1006,1010,1013,1018,1019,1021,1025,1026,1034`（12 项）· `A4_1035_PRESENT_HIDDEN=True` · `A4_NOTE_HIDDEN_AFTER_SEC=0.5`。

**🔴 否证 BE handoff §6 第 5 项的假判据（下一批务必按本批更正，勿再照抄）**

> BE 写的「未登录态**枚举不到 1035** 即通过」是**错的**。
> 源码 `WeaselServer/Cloud/CloudAccountDialog.cpp:1221` `ApplyLayout(...)` 明确**不重建控件**，对不在当前角色布局表里的 id 只做 `ShowWindow(SW_HIDE)`。
> ⇒ 「枚举不到」与「存在但隐藏」**不可区分**；且「枚举不到」还会被**枚举本身失败**满足 ⇒ 该判据既能假绿又能假红。
> ⇒ 必须写成 **`IsWindow(1035)=真` 且 `IsWindowVisible(1035)=假`**，并**配合可见 id 集合逐字比对**。

**判别性（证明这条阴性判据有牙）** = Baseline（已登录态）正控制实测 `VISIBLE_IDS=1004,1006,1010,1016,1017,1020,1022,1027,1035` ⇒ harness **确实看得到** 1035；同一 harness 在未登录态看不到 ⇒ 阴性成立。

**链路证据**：默认桌面守卫 `DESKTOP=Default`（影刀虚拟桌面未进）→ `SetProcessDpiAwarenessContext(-4)` → `Find-Dialog` 按控件 id 定位（**不按文本**）→ 服务端唯一 + 1.3.8.0 守卫 → `BM_CLICK(kIdLogout=1004)`（`CloudAccountDialog.cpp:1405 case kIdLogout: controller.SubmitLogout()`，工作线程 `Logout→StopEngine→logged_in=false`）→ 枚举可见子窗。

---

## 3. A-5 · 未登录态底部留白 = **判据实测到手**

清单 A-5 原文（`docs/release/restart-checklist-1.3.6.md:33`）：「`AccountClientSize` 按窗口角色统一取高度 ⇒ 两态同高 290 px，未登录态会有留白（**设计取舍，非缺陷**）」。

| 子判据 | 实测值 | 结论 |
| --- | --- | --- |
| 两态同高 | 未登录客户区 `840x508`；base 高 `290 × 1.75 = 507.5 → MulDiv96 → 508` | **`MATCH=True`** ⇒ 「统一取高度」坐实，未登录态未被另外裁小 |
| 留白段 1 | `1006` 状态行底 `329` → `1010` 关闭钮顶 `438` = **109 px**（base 62.3） | 实测 |
| 留白段 2 | `1010` 关闭钮底 `487` → 客户区底 `508` = **21 px**（base 12） | 实测 |
| 末行文字不触底 | 逐行墨迹剖面 `FIRST_INK_ROW=439` / `LAST_INK_ROW=485` / `CLIENT_H=508` ⇒ 末行墨迹后留白 **22 行** | **无裁切** |
| 逐控件几何 | 未登录 12 项中 **11 项实测 rect 与 `MulDiv96(base,168)` 逐字相符** | 缩放链路正确 |

**⚠️ 唯一不符项 `A5_ID1018`（本批已定性 = 探针期望表口径，不是产品缺陷，不改产品代码）**

- 实测 `578,18,242,29`，期望 `578,18,242,46`（base `1018(330,10,138,26)`）⇒ `A5_GEOMETRY_MISMATCH=1 / FAIL_COUNT_FINAL=1 / A5_EXIT=1`。
- x / y / w **三项相符**，只有 height 不符。
- 源码：`WeaselServer/Cloud/CloudAccountDialog.cpp:1014-1015` → `case AccountWidgetClass::kCombo: return CBS_DROPDOWNLIST | WS_VSCROLL;`
  ⇒ **Win32 combo 固有行为**：`CBS_*DROPDOWN*` 的**可见高度由系统按字体自行决定**，创建 / `MoveWindow` 传入的 height **不被遵守**（它只影响下拉展开部分的高度）。
- ⇒ 本批探针把**布局请求高**当**实测期望**是错的。该值登记为**信息项**，**不作缺陷、不进判据**。
- ⇒ **推广**：任何 DPI 缩放几何自检，combo 的 height 必须从等值判据里摘出来，否则在完全健康的实现上会**长期假红**。

**信息项（未定性为缺陷）**：`SELF_CTX_IS_PIV2=False` 系**探针瑕疵** —— 写法拿 `GetWindowDpiAwarenessContext(GetShellWindow())` 比 `$PM_V1=[IntPtr](-1)`（`DPI_AWARENESS_CONTEXT_UNAWARE`），比错对象**又**比错常量（PerMonitorV2 应为 `-4`）。与 BB 已登记的「`WeaselServer.exe` = PerMonitorV2」**不冲突**。

**账号态已复原（第 5 次实测这条链路有效）**：`SAFETY_TRIGGERED=1` → `probe-a4-a5.ps1 -Mode Relogin` → `RELOGIN_OK_AFTER_SEC=4` / `VISIBLE_IDS=1004,1006,1010,1016,1017,1020,1022,1027,1035` / `A4_RESTORE_VERDICT=PASS` ⇒ **当前账号态 = 已登录**。

---

## 4. 本批新增认知（可迁移）

1. **隐藏 ≠ 销毁**：凡自绘对话框布局走 `SW_HIDE` 而非销毁，所有「控件不见了」类判据都**必须**成对报 `IsWindow` + `IsWindowVisible`，单报其一必失判别力。
2. **阴性判据必须配正控制**：A-4 若没有同 harness 的已登录态 Baseline（能看见 1035），「看不见 1035」什么都不能证明。
3. **留白 / 几何只能用实测 rect**，不得用「base 常量 × DPI」反推冒充实测（本批第一版 A-5 探针就是这么写的，整段重写后弃用）。
4. **combo 高度是系统管的**，不在缩放等值判据范围内（见 §3）。
5. **重登安全网必须和无条件串联放在同一个调用里**：任何会登出的探针，其重登步骤要么排在崩溃点之前，要么整段 `try/finally` 兜住；实操口径 = 同一 bash 调用里 `;`/`if` 串 `probe-a4-a5.ps1 -Mode Relogin`，判据 = 结果文件里没有 `A5_RESTORE_VERDICT=PASS`。本批该安全网**触发 1 次并成功**。
6. **工具自述不是证据**（第 3 次实证）：上一轮声称的补丁在盘上根本不存在；本轮起一律「写 → 同轮 `ReadAllLines` 回读行数 + 字节 + `Test-Path`」，并加 `[Parser]::ParseFile` 静态自检 `PARSE_ERRORS=0` 才允许跑。
7. **宿主预算告警时立刻落盘结论**，不要继续优化非判据自检项 —— 本批 `A5_ID1018` 的定性只需一次源码取证，不需要再跑探针。

---

## 5. 本批产物路径（全部为取证产物，无产品代码）

`reports/fluxing-bf-a4-a5/`

| 文件 | 说明 |
| --- | --- |
| `probe-a4-a5.ps1` | 主探针，448 行 / 27396 B，`Baseline` / `Logout` / `Relogin` 三模式（**Relogin 是安全网，勿改**） |
| `probe-a5.ps1` | A-5 专用探针，**重写定稿** 384 行 / 21615 B / `PARSE_ERRORS=0` |
| `probe-a5.ps1.bak-v1` | 重写前备份（含被弃用的「base×DPI 当实测」写法） |
| `baseline-20261121-145046.txt` 891 B | Baseline 正控制原始输出 |
| `evidence-baseline.png` 22571 B / `evidence-baseline-note.png` 6189 B | **已登录态整窗 + 提示行裁片** ⇒ 即 BB 所需 175% 真机像素证据（`SYS_DPI=168 / WIN_DPI=168 / SCALE_PCT=175`、客户区 `840x508`、`NOTE_PHYS=21,273,798,63`） |
| `a5-logout-20261121-151429.txt` 1549 B | **A-5 判据原始数据**（留白 / 剖面 / 逐 id 几何） |
| `evidence-logout-a5.png` 14857 B / `evidence-logout-a5-bottom.png` 1947 B | 未登录态整窗 + 底部留白带 |
| `a5-run.txt` / `a5-run2.txt` / `a5-run3.txt` | 三轮控制台副本（`run1/2` 为失败轮，留证不删） |
| `a5-safety-relogin*.txt` ×3 | 重登安全网触发记录 |
| `logout-run.txt` / `relogin-run.txt` / `baseline.log` | A-4 轮次原始输出 |
| `fix-docs.ps1` | 清单三行改写脚本（表格行特征锚定 + 命中数必须为 1 + 等行号赋值 + 行数不变断言），已执行：`CK_LINES_BEFORE=133 → CK_LINES_AFTER=133`、九个回读位全绿、`CRLF_COUNT=0`（未引入混行尾） |

**口令不在本文档、不在回答正文**。来源文件 `tmp/AN-register-qq.txt`（字段标记 `FLUXING_PROBE_PASSWORD`，账号 `25940969@qq.com`，`PW_LEN=22`，校验位见 `reports/fluxing-bf-a4-a5/relogin-run.txt`）。Phase 0 判别性实测 = 未登出前先 `node tmp/login-check-2026-11-19.mjs` → `LOGIN status=200` + `email_verified=true`。

---

## 6. 未闭环（下一批的活）

1. **双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3/G-5** —— 需**两台机器 + 用户在场**，本机不可做。
2. **200%（或其他缩放档）真机证据** —— 用户已拍板**不主动切档**；175% 已在本批顺带取到，**勿再为此重开档位**。
3. 本批文档改动 + BF 目录取证产物**已在收口 commit 内提交并 push**（`docs(fluxing)`）。⚠️ 本文档**不钉死提交哈希与 ahead/behind**（自引用必失真）⇒ 实况一律以 `git -C F:/soft/00selfmade/rime_claude status -sb` + `git log -1 --stat` 为准。

**已闭环、勿重跑**：B-4（用户 2026-11-21 拍板按「宿主退出触发源」口径闭环）· `/verify-client-pid`（BE 实测 `PASS`，`reported=self=1026796`）· B-1/B-2/B-3（BC）· B-5（BD `PASS_E2E`）· A-1/A-2/A-3（BA）· 缺陷 C 修法（BB，ADR 0068）。
