# 099 · Tasks · 快捷设置栏对标搜狗

> 配套 `spec.md`。**一次只做一片**，每片做完贴测试输出再进下一片。
> 状态标记：`[ ]` 待办 / `[~]` 进行中 / `[x]` 完成（完成时必须把验收输出写进本文件对应行下）。

## P0-1 · 搜狗快捷设置栏黑盒行为采集

> 2026-11-05 完成。用户在场把活动输入法切到搜狗，运行时观察由**自建 Win32 观察链**采集
> （REA 的运行时原生 UI 家族在 Windows 上不可用，见 `quickpanel-behavior.md` §0.2）。
> 产出：`docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md`
> ＋ `evidence\quickpanel\`（8 张截图 + 6 份运行日志 + 2 个采集脚本，已入库可复核）。

- [ ] 记录搜狗 16.8.0.5415 状态栏**右键菜单**全项 → **未做到**：右键未成功触发 ⇒ `quickpanel-behavior.md` §6-1
- [ ] 打开其「快捷设置/属性配置」面板，截图首屏与每个分页 → **首屏已截**（`≡` 面板 = `CherryIMEWindowClass` 480×974，截图 `sogou-menu-panel.png`）；**分页未打开** ⇒ §6-9
- [ ] 逐项点击：记录「点了之后改变了什么」——是否即时生效、是否需要重启、是否有开关态 → **未做** ⇒ §6-3
- [ ] 中/英、全/半角切换时，面板与状态栏的回显变化 → **只观察到静态形态**（状态栏槽 2 = `中`、槽 3 = `。,`；`≡` 面板有「英 / 英文输入法」「半 / 全半角切换」对应开关）；**切换后的回显未验证** ⇒ §6-3。P0-4 规格不依赖此项（我方 P0-4 已实装状态回显）
- [ ] DPI 100% / 150% / 200% 下面板尺寸与清晰度；主/副屏拖拽后的表现 → **仅 150% 实测**（状态栏物理 353×66、候选条 713×122，与逻辑值 235×44 / 475×81 比值同为 1.5）；100% / 200% 需注销重登，多屏无设备 ⇒ §6-4、§6-5
- [x] 常驻 vs 临时出现的行为差异（对照我们的 `kAlwaysShow`）→ **已确认**：状态栏**常驻可见**（候选窗隐藏时仍在），候选条**仅组合时可见** ⇒ 两者是**两套独立生命周期**；拖拽落点持久化未验证（§6-6、§6-7）
- [x] 产出 `docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md`：每条观察 → 截图相对路径
- [x] 未观察到的项写「未验证」，**禁止用推测填充** → §6 逐条列出 **11 项**未验证

**验收输出（2026-11-05）**

```
# 窗口枚举（log-window-enum.txt）
  cls=SoPY_Status vis=True  rect=[3369,1964,3722,2030] 353x66   child=0
  cls=SoPY_Comp   vis=False rect=[611,378,1324,500]    713x122  child=0
# 采集（log-capture.txt）—— 打字 nihao 触发组合后
  COMP rect=[696,378,1409,500] 713x122   COMP_CHILD_COUNT=0
  STATUS rect=[3369,1964,3722,2030] 353x66
# ≡ 命中（log-menu-hit.txt）
  target hamb click=(1359,449) -> NEW cls='CherryIMEWindowClass' pid=75684 rect=[1382,381,1862,1355] 480x974
# 状态栏槽位（sogou-status-printwin.png 原生 353×66 与 sogou-status-zoom.png 3× 放大互证）
  S | 分隔线 | 中 | 。, | 🎤 | ⌨ | 彩色渐变块 | 2×2 四宫格   = 7 可点项
# ≡ 面板三段式（sogou-menu-panel.png）
  账号区(winerking / 今日输入277字) + 4 开关(简/半/英/👁 各带小字说明) + 10 项列表(3 条分隔线分组)
# UIA（log-uia-empty.txt）
  搜狗窗口 0 个 AX 节点 ⇒ 完全自绘、不可被辅助技术访问
```

**未验证 / 已知限制**

- 状态栏右键菜单全项、面板二级页、逐槽点击效果、100% / 200% DPI、多屏落点、拖拽持久化 —— 全部见 §6 逐条。
- REA `observe-native-ui` 在 Windows 上**必然失败**（`capability_unavailable` / `unsupported_provider`，provider 为 macOS-only）⇒ 勿再试探该命令族。

## P0-2 · 差距矩阵与取舍

- [x] 三列表：搜狗行为 × 我方现状（源码位置） × 设计意图（`docs\design\quickpanel-v{3,4,5}`）
- [x] 每行判定「借鉴 / 改造 / 不抄」+ 理由
- [x] 每行填「借鉴来源（证据文件全路径）」与「差异化实现」；填不出的行删除
- [x] 产出 `.specify\specs\099-quickpanel-sogou-benchmark\gap-matrix.md`

**验收输出（2026-11-05）**

```
矩阵 10 行，每行三列齐全 + 判定 + 借鉴来源（证据文件全路径）+ 差异化实现
  借鉴 3（行 1 常驻浮位 / 行 3 状态可见 / 行 10 DPI 口径）
  改造 2（行 2 胶囊一槽一图标 / 行 4 只借「开关带当前态」手法，不建二级面板）
  不抄 4（行 5 二级菜单 / 行 6 账号区 / 行 7 皮肤入口 / 行 9 候选条菜单）
  反向借鉴 1（行 8 搜狗完全自绘 ⇒ 不可访问；我方保持每槽独立命中区 + 键盘可达）
另列「不列入的候选项」7 项（无观察 ⇒ 不写行），不预判结论
```

**未验证 / 已知限制**

- 本轮**未改动任何生产代码** ⇒ 无单测 / 门禁需重跑；`kButtonCount = 4` 与 IPC 边界均未触碰。
- 行 7（皮肤入口）依赖 `REPORT.md §2` 的**静态导出名**（无运行时行为观察）⇒ 已在矩阵中标注证据分级。

## P0-3 · Account 槽实装（用户 2026-11-04 00:01 定稿：不摘除）

- [x] 先写判别性测试：断言「每个可见槽位点击必产生确定的用户可见效果」——当前必红（hit==3 为 no-op）
- [x] **决策（用户定）：实装** —— 第 4 槽（Account）点击打开 **「关于 / 账号说明」页**；不采用整槽摘除。UserDict 整槽删除先例（`QuickPanelDialog.h:132`）仅作实现参照，不适用于本槽
- [x] 先定「关于 / 账号说明」页的载体（**动手前必查图谱，不许新建平行实现**）：现状实测 = WeaselServer/WeaselUI/include 源码内**没有**任何现成 About 对话框（本轮 grep `关于|About|github\.com` 仅命中 `include\winsparkle.h` 与 `WeaselUI\GdiplusBlur.cpp`，均非 About UI）⇒ 新增 `WeaselServer/AboutDialog.{h,cpp}`（进程内、modeless、原生 caption 框 + `ModalChrome` 渐变底；**不新建 exe、不用 MessageBox**）
- [x] 内容规格（简体中文，P5）：产品名/版本号取自**宿主 exe 的 PE 版本资源字符串表**（`ProductName`/`ProductVersion`，语言块 zh-CN>zh-TW>en-US>en-GB，回退链 `ProductVersion`→`FileVersion`→`VS_FIXEDFILEINFO`），**不得硬编码第二处**；账号说明按事实写「本输入法不含云端账号。词库、配置与用户数据全部保存在本机，不上传、不同步。」；可点项 = 「打开数据目录」（本地 `explore`，不联网、不等待）。**不含「检查更新」**
- [x] **P2 红线**：版本/产品名/数据目录在**服务端启动期读一次并缓存**（`AboutDialog::Initialize()`，在 `WeaselServerApp` 构造函数内调用），窗口显示与按钮点击路径**零 I/O**；点击处理内不阻塞 I/O、不在 `OnLButtonUp` 里同步 `ShellExecute` 等待
- [x] 实现：新增 public `SlotRect()`（槽位布局**单一事实源**，`HitTest` 与单元测试共用）与 `DispatchSlotClick()`（两条抬起分支统一路由）；`WeaselServerApp.cpp` 的 `onSchema` → `SwitchToNextSchema()`（槽 0 改为「切换到下一个输入方案」）、`onLogin` → `AboutDialog::Show()`；方案循环抽成 header-only `include/FluxingSchemaCycle.h`（`fluxing::NextSchemaId`），生产与测试同源（R7）
- [x] IPC 四边同步**不触发**：本轮**未改** `include/WeaselIPCData.h`，方案切换走进程内 `rime_api->select_schema()`（Rime 会话宿主在 WeaselServer 进程内）⇒ 无需 server/TSF/deployer 三端同步；ADR 取新编号 `0034`（并补上 README 漏掉的 `0033` 索引行）
- [x] `kButtonCount` 保持 4（代码事实）⇒ 槽位数量断言**未被改动**。**注意**：099 spec §7 引用的 `scripts\gate\L1-quickpanel-{state,icon,hotkey}.ps1` 与 `L2-rime-quickpanel-e2e.ps1` 在仓内**尚不存在**（属后续切片待建），故本轮没有「槽位数量期望」文件可改，也无此类用例变红
- [x] 收尾（代码侧）：判别性测试转绿 + 贴输出 + 门禁全绿 + CHANGELOG `[Unreleased]`（P5 用户可见改动）
- [x] **顺带修掉一个既有缺陷**：`GetCursorPos` 失败时旧代码读未初始化 `POINT` 算位移 ⇒ 槽位点击被误判成「拖拽」而**随机吞掉回调**（真机表现：RDP 断开/会话锁定时「按钮点了没反应」）。已在 `WM_LBUTTONDOWN/MOVE/UP` 三处加失败兜底（详见 lessons-learned **L178**）
- [ ] 装机验收 `scripts\install\verify-installed.ps1` 与用户实际点击确认 —— **未做**（本轮未重新安装；槽 0 方案切换属 librime per-session 语义，端到端可见性需装机手点复验）

**验收输出（2026-11-04）**

```
# 造坏数据证明有牙：故意断开 DispatchSlotClick 的 case 0 / case 3 ⇒ 必红
BROKEN_BUILD_EXIT=0
BROKEN_RUN_EXIT=1
  FAIL: T20g: 槽 0 点击 → 方案切换回调恰好 1 次
  FAIL: T20j: 槽 3 点击 → 关于页回调恰好 1 次
=== 86 / 88 assertions passed ===
# 恢复（源文件字节级一致 RESTORED_BYTE_EQUAL=True）⇒ 必绿
RESTORE_BUILD_EXIT=0
RESTORED_RUN_EXIT=0
=== 88 / 88 assertions passed ===
# 连跑 9 轮零抖动
FIRST_EXIT=0 / RUNS_2..9=0,0,0,0,0,0,0,0

# 门禁（scripts\gate\run-all.ps1）
L0  PASS  exit=0  80.6s  静态门禁 checks=13 pass=13 fail=0 skip=0
L3  PASS  exit=0  79.4s  变异自检 mutations=7 killed=7 survived=0
L4  PASS  exit=0   1.4s  回归用例(fast) cases=1 pass=1 fail=0
```

**未验证 / 已知限制**

- 槽 0「切换到下一个输入方案」的端到端可见性：`select_schema` 作用于活动会话（librime per-session 语义），本会话无法验证 → 需装机手点。
- 关于页像素级版式（420×300 客户区 / 圆角 12）无视觉基准，需装机人眼确认。
- L5 sandbox-gate 本轮未跑（发版前必跑；沙箱无 `D:` 盘/外网时该项按设计固定红，属环境能力项）。

## P0-4 · 当前输入状态回显（中/英 + 全/半角）

- [x] 确认状态来源：面板 `Show()` 现有 `currentFullwidth` 入参；中文态需从 server 侧取（**禁 TSF 阻塞 I/O，P2**）
  - 落地：`include/FluxingPanelStatus.h` 的 `PanelState` 内存快照由 `StateProvider` 注入（`WeaselServerApp.cpp`）；面板在**已有的 100ms timer（id=2）**里拉取并比对 5 字段，变化才重绘 ⇒ 显示路径零 I/O（P2）
  - 预热：`WeaselServerApp::Run()` 在 `m_handler->Initialize();` 之后强制 `(void)m_handler->IsFullShape(); (void)m_handler->IsSimplification();` ⇒ 首次轮询不触发 `user_config_open`（lessons-learned **L180**）
  - 常驻开关唯一真相源 = `AlwaysShowMode()`（`RefreshInputState()` 强制覆盖 provider 的 `always_show`）⇒ R7 守住
- [x] 先写失败测试：状态切换后面板回显跟随
  - 验收输出：`test/TestQuickPanelDialog` T26（状态区几何 + 中/英/全/半 文字映射 + `SetInputState`/`CurrentInputState` 回读）—— **190/190 断言全绿**
- [x] 实现绘制 + 命中区不变；不得改动 `quickpanel.state` 语义
  - 状态区几何：左界 = 最后一个槽位右界 + 间隙（`kStatusGap=1`），右界 = 内容右界（面板宽 − padding）；100% ⇒ x=225..240 / 150% ⇒ x=341..360 / 200% ⇒ x=450..480 ⇒ 任意 DPI 与 4 槽位两两不相交
  - `quickpanel.state` 语义未动；`include/WeaselIPCData.h` **零改动**（IPC 边界未破）
- [x] 贴 e2e 输出
  - 本轮贴的是**单测输出**（`Release\TestQuickPanelDialog.exe` exit=0，190/190，无回归）；真机 e2e 需装机，随 P0-6 一并手点

## P0-5 · 键盘可达性 + 槽位 tooltip

- [x] 失败测试先行：Tab / Shift+Tab / ← → 移焦、Enter|Space 激活、Esc 收起
  - T27（纯逻辑状态机）+ T29（走真实 `WndProc` 端到端：Show → Tab 得焦点槽 0 + 进交互态 → Enter 触发槽 0 回调 → Esc 收起面板自身）
- [x] Esc 行为**必守 D0032**（只收面板、不得连带收起候选窗；用例 `test\regression\cases\D0032-esc-no-cascade.ps1`）
  - 路径纠正：tasks 里写的 `D0032-esc-no-cascade.ps1` **不存在**，实际用例是 `test\regression\cases\C0032-phrasesdialog-esc-panel.ps1`（heavy，需 `FLUXING_ALLOW_UI_AUTOMATION=1` + 装机版 SHA256 == `output\Win32\WeaselServer.exe`，用 `csc.exe` 编译 `test/regression/probes/ReproEsc.cs`）—— 本轮**未跑** heavy 用例
  - 语义调和（ADR 0036）：用**键盘交互态**这一可判定状态位分流 —— 未交互 ⇒ Esc 只吞掉、面板不隐藏（守 ADR 0019 D2）；已交互 ⇒ Esc 收起**面板自身**、不触碰任何其他窗口（守 D0032）
- [x] tooltip：每槽「名称 + 当前态」，品牌 tooltip 现有实现可扩展
  - 自绘轻量顶层窗（类名 `FluxingQuickPanel_Tooltip`，`WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT`），不引 comctl32、不改 `.vcxproj`；文本由 `include/FluxingPanelTooltip.h` 的 `BuildTooltip()` 生成（分隔符「 · 」）
  - hover 真相源 = **100ms 轮询**而非 `WM_MOUSEMOVE`（L84-fix 已证投递不可靠）；位置在面板上方并钳制在工作区内
  - 验收输出：T28（4 个槽名 / 越界空串 / `BuildTooltip` 全文 / 英文态显示「英」/ 缺方案名「未知」/ 生产 `TooltipText` 与纯逻辑一致 / 跟随 `AlwaysShowMode()`）
- [x] 扩展 `scripts\gate\L1-quickpanel-hotkey.ps1` 并**造坏数据证明新断言会红**
  - 该脚本本轮**新建**（15 项检查：A 资产存在 / B DPI 单一真相源 / C 状态回显接线 / D 键盘接线 / E tooltip 接线 / F 显示路径零 I/O / G 中文文案 / H IPC 边界未破），已接进 `scripts\gate\run-all.ps1`（新增 `-SkipL1`）
  - 造坏数据证明：`docs/archive/evidence/spec099-l1-gate-toothproof.txt` —— CONTROL 绿 / M1（删 `WM_TIMER` 里的 `RefreshInputState()`）红 / M2（本地抄一份 DPI 公式）红 / M3（`FluxingPanelStatus.h` 引入 `WeaselIPCData.h`）红 / ORIGINAL 绿 ⇒ `TOOTHPROOF=OK`
  - 单测侧另有 `docs/archive/evidence/spec099-mutation-proof.txt`（5 处变异全被抓住，还原后 SHA256 一致）

## P0-6 · 装机版 DPI 复验（T5 收口）【可立即开工】

- [x] 探测 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 版本（应为 1.1.9.0，本轮已证）
  - 实跑：ProductName `Fluxing` / ProductVersion `1.1.9.0` / FileVersion `1.1.9.0`，Length 2287104
- [x] 起 server（从源码构建 `output/Win32/WeaselServer.exe` 直接起，未动已安装版），在 150% DPI 下量面板物理尺寸
  - **本机真实缩放就是 150%**（前一版交接文档记的「100% / 虚拟屏 2560×1440」是**错的**：2560×1440 是 DPI 未感知进程看到的虚拟化逻辑分辨率，真实屏 3840×2160 @ 150%）
  - 三条独立来源一致：`GetDeviceCaps(LOGPIXELSX)=144` / `GetDpiForMonitor=144x144 hr=0` / `HKCU:\Control Panel\Desktop\LogPixels=144` + `Win8DpiScaling=1`
  - 实测：触发表 `WM_COMMAND 40020 → WeaselIPCWindow_1.0 (0x48040A)`；面板 `FluxingQuickPanel_v3`，`GetDpiForWindow=144`，`GetWindowRect=(1450,852,1818,924)` ⇒ **物理 368×72**
  - 证据：`docs/archive/evidence/spec099-p0-6-real-machine-dpi.txt` + `spec099-p0-6-measure-raw.txt` + `spec099-p0-6-panel-150dpi-368x72.png`（368×72 截图，隔像素去重 362 色 ⇒ 确已绘制非空窗）+ 可复跑探针 `spec099-p0-6-measure-panel.ps1`
- [x] 期望 368×72（修复后），若仍 245×48 ⇒ 登记 `test\regression\DEFECTS.yaml` 新 `D00xx` 并写 heavy 复现用例
  - **真机实测 368×72，与 `include/FluxingPanelDpi.h` 断言逐像素一致 ⇒ 不需要登记缺陷**
- [ ] 100% / 200% 各测一次；多屏各测一次
  - 断言侧三档齐备（245×48 / 368×72 / 490×96，`dpi<=0` 退化 96）
  - **仍需用户在场**：改系统缩放到 100% / 200% 必须注销重登，会话内无法完成；本机单屏，多屏落点无设备可测

## P0-7 · 面板位置持久化

- [x] 决策落 ADR：位置存在哪（**不得污染 `quickpanel.state` 的单一真相源语义，R7**）→ **ADR 0035**（`docs/adr/0035-quickpanel-position-persistence.md`）
- [x] 失败测试：跨重启保留位置；屏幕分辨率/ DPI 变化后钳制回可视区
- [x] 实现 + 多屏负坐标用例
  - 落点：`<用户数据目录>\quickpanel.position`（**独立文件**，与 `quickpanel.state` 同目录不同文件）⇒ R7 守住；格式 `pos_x=<int>` / `pos_y=<int>`，允许负坐标
  - 读：**服务端启动期**读一次并缓存 ⇒ `Show()` 显示路径零 I/O（P2）；恢复**只回填缓存、不 auto-show**（spec 055）
  - 写：拖动结束写**原始位置**（不在保存时钳制，避免松手瞬间跳动）；纯点击不写盘
  - 钳制三段：完整落在某工作区内⇒原样 / 否则吸附到「中心最近且容得下面板」的工作区 / 都容不下⇒(0,0)；工作区取 `EnumDisplayMonitors` + `GetMonitorInfoW` 的 `rcWork`
  - 共享纯函数下沉 `include/FluxingPanelPosition.h`（header-only，namespace `fluxing::panel_position`）⇒ 生产代码与测试同源
  - 验收输出：`test/TestQuickPanelDialog` T24 系列 **32 条断言全绿（120/120）**
  - **造坏数据（变异）证明有牙**：M1「破坏右边界上界 `best->right - w` → `best->right`」⇒ T24s + T24u 红（118/120）；M3「包含判据参数顺序写反 `RectContains(wa, panel)` → `RectContains(panel, wa)`」⇒ T24r2 红（119/120）；两次恢复后 **SHA256 字节级一致** + 120/120 全绿
  - 稳定性：先修掉槽位点击用例自身的**全局光标抖动**（lessons-learned **L179**；基线 30 轮 1 红 → 修复后 **30 轮 0 失败**）
  - 未验证：真实多屏拔插 + 100% / 150% / 200% DPI 下的实际落点（需装机手点复验）

## P1 · 声明式槽位表（不排进 1.1.x）

- [ ] ADR 草案：槽位表驱动（对齐 `UIPlugin.dll` 的 `AddUIFunction` 思路，自有实现，不抄代码）
- [ ] 迁移方案 + 兼容性测试

## 收尾（每片都要做）

- [ ] 用户可见改动写 `CHANGELOG.md` 未发布段（P5）
- [ ] 事故/坑追加 `.specify\memory\lessons-learned.md`（**BOM + CRLF**）
- [ ] 架构决策进 `docs\adr\`（下一个编号从 0034 起，先 `ls` 确认）
- [ ] push 前 `scripts\gate\run-all.ps1 -Level L0`；发版前 `-Level L0,L3,L4,L5 -Build`
- [ ] 提交用 Conventional Commits + 单一 scope；`git status -sb` 确认 ` m librime` 未被带进去
