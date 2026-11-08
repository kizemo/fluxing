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

## P0-8 · 全半角图标按钮内对角线重做（v7）

- [x] 取证：旧图标真机墨迹过小（用户否决）
  - v6（字形组合 `，。` / `,.`）：全角墨迹仅 **18×4 raw px**、半角仅 **8×5**，浅灰 `RGB(130,130,140)` 压在浅蓝底 `RGB(200,225,250)` 上几乎不可见 ⇒ 用户原话「全半角符号太小，不具有可视度」
  - 根因：CJK 标点墨迹只占 em 盒约 0.2 ⇒ 字号调大也长不大，且会撑破 28 键帽
  - 证据：`tmp/crop-batchf5-slot2-B.png`（旧图标真机裁片）
- [x] 参考物采集：搜狗输入法状态栏槽 3 =「。」+「,」对角排布
  - 证据：`docs/research/sogou-reverse-2026-11/evidence/quickpanel/sogou-status-zoom.png`
- [x] 设计定稿（v7）：**按钮内对角线布局**的 GDI 矢量图元
  - 全角 = 右上**空心环**（宋体式「。」，路径直径 14 raw px、2px 描边）+ 左下**逗号尾** ⇒ 声明墨迹盒 **25×25** raw px
  - 半角 = 右上**实心点**（ASCII「.」，路径直径 10 raw px）+ 左下**小逗号尾** ⇒ 声明墨迹盒 **21×23** raw px
  - 「空心 vs 实心 + 尺寸差」双重编码当前态；形状随 `s_inputState.fullwidth` 切换的既有语义不得退化
  - 弃用记录：v5 宽 / 窄圆角框、v6 字体字形组合 —— **两版均已作废**
- [x] 实现：图元表 + 居中公式下沉 `include/FluxingPanelFwIcon.h`（namespace `fluxing::panel_fwicon`，纯函数，生产与单测同源 / R7）
  - `DrawIconFullwidth()` 改为纯 GDI 矢量：空心环配 `NULL_BRUSH`、实心件配 `NULL_PEN`，**不走 `AlphaBlend`**（v4 黑框前科）
  - 居中按「墨迹盒并集 + 就近舍入」⇒ 中心恰为 viewBox 中心 (15,15)，与另外 5 个线描图标对齐
  - `s_hFontPunct` 整条生命周期（声明 / 定义 / 创建 / 释放）已删除
- [x] 单测：T33a–T33i 共 9 条断言
  - 墨迹盒 25×25 / 21×23、两态居中中心 == (15,15)、全角 1 空心 + 2 实心 / 半角 0 空心 + 3 实心、空心环直径 ≥ 14 raw px、墨迹盒落在 viewBox 内且不超 28 键帽、两态对角方位
  - 验收输出：`Release\TestQuickPanelDialog.exe` exit=0，**240 / 240**（原 231 + 新 9）；`TestQuickPanelRefactor` **40 / 40**
- [x] 真机像素验收：装机后裁 slot 2 交用户目视（需人眼确认「够大够清楚」）

- [x] 追补 **v8**：句号收小 + 中文逗号重画（用户指令：句号都偏大、中文逗号偏小且变形）
  - 全角空心环路径直径 14 → **10** raw px（配 2px 笔）、半角实心点 10 → **7** raw px
  - 逗号弃 v7「圆头 + 尾巴三角」两块拼接，改为**单一 10 点多边形**凹形轮廓（全角 11×13、半角 8×12）；`Icon.shapes` 3 → 2 槽
  - 墨迹盒改由 `ShapeInkBox()` 从几何表推导（全角 23×25、半角 17×23）
  - 单测 T33a–T33l 共 12 条 ⇒ Dialog **243 / 243**、Refactor **40 / 40**
- [x] 追补 **v8.1**：空心环描边调细（用户指令：线需要调细，才能更清楚分辨）
  - `kStrokeWidth` 2 → **1**；环路径框同步外扩 1px `{17,4,27,14}` → `{16,3,28,15}` ⇒ GDI 实测墨迹 10×10 / 50 px → **12×12 / 32 px**
  - `ShapeInkBox()` 去掉 `stroke/2` 外扩：实测 GDI `Ellipse` 笔**不向外溢出**，墨迹盒 == 路径框减右 / 下边界（`tmp/probe-pen1.ps1`）
  - 单测新增 T33m（笔宽 == 1）/ T33n（墨迹盒 == 路径框减边界）⇒ Dialog **245 / 245**、Refactor **40 / 40**
  - 取证限制：`full_shape` 无外部翻转入口 + 合成点击与真实 `mouse_event` 均不生效 ⇒ 全角态 1px 效果只能靠单测 + 人眼确认

- [x] 追补 **v9 / 批次 H**：图标分离 + 半角=英文标点 + Shift 中英联动标点（用户三条指令）
  - 图标 v9：全角环 `RECT{16,2,28,14}` + 12 点中文逗号、半角点 `RECT{17,4,24,11}` + 10 点小逗号；并集墨迹盒 全角 **25×27** / 半角 **19×25**（奇×奇 ⇒ 居中中心仍 (15,15)）
  - 判据：两态两图元 **≥2 空列 + ≥2 空行**，x/y 皆不相交（像素探针 `tmp/probe-fwicon-v9.ps1`）
  - 状态口径：新增 `include/FluxingPanelStatus.h` 的 `fluxing::panel_status::ShapeIsFullwidth(ascii_mode, ascii_punct)`；面板不再读裸 `full_shape`
  - 服务端新增 `SetPunctuationWidth(bool halfwidth, WeaselSessionId)`；设置栏「全/半角」改为驱动 `ascii_punct`（为真 ⇒ punctuator `kNoop` ⇒ 英文标点）
  - Shift 联动：`ShiftUp` 的 `kToggleAsciiMode` 分支追加 `_ApplyPunctWidth`；`GetPanelInputState` 改 live 读取（零 `set_option`、零文件 I/O）；新增「最近焦点会话」回退 `m_last_focus_session`
  - yaml：`melt_eng` / `radical_pinyin` 补 `ascii_punct` 声明（不写 `reset` ⇒ 默认 ¥ = 中文标点）
  - 单测新增 T33o / T33p / T33q / T34a–T34d ⇒ Dialog **252/252**、Refactor **40/40**
  - 真机隔离实验 `Ctrl+.` 六段全绿（`tmp/probe-ctrl-punct.ps1` → `tmp/probe-ctrl-punct.txt`）
  - 打包链路：改 `output/data/*.yaml` 后必须**重打安装包 + 静默重装**（数据落在 `$INSTDIR\weasel\data`），本轮已重打（NSIS_RC=0）并重装（四二进制 MATCH=True）

- [x] 追补 **v10 / 批次 H2**：字形逗号 + 「全角 = 中文标点」语义纠正 + 点按钮不抢焦点（用户五条指令）
  - **图标 v10**：逗号弃 `kSolidPolygon` 手绘多边形（GDI `Polygon` 无抗锯齿 ⇒ 锯齿不可消除），
    改为**字体字形**：`Icon` 新增 `commaInk` / `commaChar` 两字段，全角 `L'\xFF0C'`、半角 `L','`，
    字体 Microsoft YaHei `ANTIALIASED_QUALITY` `lfHeight=-48`；`Icon.shapeCount` 2 → **1**（环/点各 1）
  - 落笔用 `GetGlyphOutlineW(GGO_METRICS)` 的墨迹盒做**中心对齐**（`GlyphPenPointForInk`），
    新增纯函数 `GlyphInk` / `ScaleFontHeight` / `GlyphPenPointForInk`，生产与单测同源；
    `ScaleFontHeight` 一次自校正抗字体替换（正常机器是死路径）
  - 判据（v6 结论修正）：v6 写「字体方案不可行」是**错的**，真因是字号取小；本轮 seed=48 实测两态墨迹均 **7×13**
  - **语义（ADR 0041）**：全角 = **中文标点**，`full_shape` 永久关闭。`_ApplyPunctWidth` 只写 `ascii_punct`，
    且 `full_shape=0` 从「仅 halfwidth 分支」提到**两个分支都清**（清用户机器上的历史残留，
    否则「英文标点」态会打出全角空格）；`default.yaml` 的 `switcher/save_options` 移除 `full_shape`
  - **文案**：`panel_status::ShapeModeText` 改名 **`PunctModeText`**（返回「中文标点 / 英文标点」）；
    槽 5 名改「中英文标点切换」。`ShapeIsFullwidth` / `PanelState::fullwidth` / `onFullwidth` **保留旧名**只补注释
  - **焦点**：`WM_LBUTTONDOWN` 的 `CaptureKeyboardFocus` 加 `if (hit < 0)` 前置 ——
    点按钮不再抢焦点，点完可直接在宿主窗口继续输入
  - **Shift 同步**：`kToggleAsciiMode` 补写 `m_global_ascii_mode`（面板「中/英」标志与全半角同步失配的根因）
  - 单测：T33k/T33l/T33q 的**多边形断言已废**（`ptCount`/`pts[]`/`ShapeInkBox(shapes[1])` 在 v10 下是未初始化内存），
    改为 `commaInk` 目标矩形 7×13 + `commaChar` 码点（U+FF0C / U+002C）+ 字形落笔往返断言 T33q2 / T33q3；
    T26g/T26h/T26j、T34a–T34d、T28e2/T28i3/T28k2/T28o3 文案改新口径
    ⇒ Dialog **254 / 254**（原 252 + 净 2）、Refactor **40 / 40**
  - 构建：x64 TSF + x86 全解决方案 + NSIS 全绿（`ALL_STEPS_OK` / `NSIS_RC=0`），安装包 `fluxing-1.2.1-installer.exe` 已重打
  - **未做**：真机像素验收（图标墨迹 / Shift 中英同步 / 点按钮后焦点 / 中文标点语义）需装机后人眼 + 手点确认

## P1 · 声明式槽位表（不排进 1.1.x）

- [ ] ADR 草案：槽位表驱动（对齐 `UIPlugin.dll` 的 `AddUIFunction` 思路，自有实现，不抄代码）
- [ ] 迁移方案 + 兼容性测试

## 收尾（每片都要做）

- [ ] 用户可见改动写 `CHANGELOG.md` 未发布段（P5）
- [ ] 事故/坑追加 `.specify\memory\lessons-learned.md`（**BOM + CRLF**）
- [ ] 架构决策进 `docs\adr\`（下一个编号从 0034 起，先 `ls` 确认）
- [ ] push 前 `scripts\gate\run-all.ps1 -Level L0`；发版前 `-Level L0,L3,L4,L5 -Build`
- [ ] 提交用 Conventional Commits + 单一 scope；`git status -sb` 确认 ` m librime` 未被带进去

---

## 批次 J 追补 · 英文逗号辨识度 v12 + 语言栏项退出常驻托盘（2026-11-08）

> 本轮两条用户验收：①「中文标点辨识度已经很高，但英文的逗号辨识度仍然需要改进」；
> ②「去除任务栏托盘区的输入法『中英文』图标，这个图标不需要常驻」。
> 交接「下一阶段执行序」里的**补 `t9.schema.yaml` switches 经实测证伪，未做**（T-9J5）。

- [x] **T-9J1 v12 · 半角逗号墨迹盒 6×9 → 8×12** — `include/FluxingPanelFwIcon.h`
      `kCommaInkWidthEn=8` / `kCommaInkHeightEn=12`（中文态 8×13 不动）；字号仍由
      `ScaleFontHeight(48,11,12)=52` 反解，实际落点由 `GlyphPenPointForInk` 单调迭代命中目标盒。
      单测 `T33k(v12)` 契约 + `T33q2` 目标盒同步（`{5,17,12,28}` / `hw_pen(3,20)`）。
      代价如实记录：两态逗号同宽 8（高 13 vs 12）⇒ 辨识度改由「形状 + 句号图元
      （12px 空心环 vs 7px 实心点）」承担。
- [x] **T-9J2 语言栏项退出常驻托盘** — `WeaselTSF/LanguageBar.cpp` `GetInfo()` 摘掉
      `TF_LBI_STYLE_SHOWNINTRAY`，保留 `BTN_BUTTON | BTN_MENU`（左键开面板、状态文字「英/中」
      等能力零损失；明确不用会删掉整项的 `TF_LBI_STYLE_INVISIBLE`）；决策记录 **ADR 0044**。
- [x] **T-9J3 单测回归** — `TestQuickPanelDialog` 255/255 + `TestQuickPanelRefactor` 40/40。
      T12d/T12e 在中途一次运行里红过：插判别性诊断实测 `hit0..3 = 0..3`、回调计数与目标值全正常，
      摘掉诊断再跑即全绿 ⇒ **定性为间歇性 flaky（测试侧），非本批引入、非生产缺陷**；
      批次 I CHANGELOG 里「40/40」的声称与当前 HEAD 实跑存在同一条抖动，按 flaky 处理。
- [x] **T-9J4 全量构建 + NSIS + 静默重装** — `tmp/build-batch-j.log`；
      `tmp/reinstall*-*.log` 校验 `INSTALL_EXIT=0` 与已装二进制 SHA256 三方一致。
- [ ] **T-9J5 ~~给 `t9.schema.yaml` 补 `switches:` 段~~（证伪，不做）** — 两条反证：
      ① `output/data/t9.schema.yaml:98` 已有 `__include: rime_ice.schema.yaml:/`，
      **继承** rime_ice 的 `switches`（含 `ascii_punct`），显式补段反而覆盖继承结果；
      ② 活动用户目录 `build/` 下**没有 `t9.schema.yaml`** ⇒ t9 本机从未部署，
      「t9 下 ADR 0043 不生效」缺事实基础。若要终验：启用 t9 方案 + 重新部署，
      再看 `build/t9.schema.yaml` 的 `switches`，**而不是改源 yaml**。
- [ ] **T-9J6 真机终确认（需人眼 + 需用户侧动作）**
      ① 英文逗号辨识度 — 运行时像素已分离（`tmp/v12b-*`：`P4 VERDICT punct_follows_state = PASS`，state0 `，` U+FF0C / state1、state2 `,` U+002C；slot2 墨迹 state0 `67px 23×26` vs state1 `59px 18×24`；`CJK-CHECK text=[你 ] cjk=True`），**待用户目视** `tmp/v12b-P4-slot2-state0.png` vs `tmp/v12b-P4-slot2-state1.png`。`P6` 那一步 FAIL 与批次 I 基线逐字符相同 ⇒ 探针既有缺陷、非本批回归。
      ② 通知区域不再常驻「中/英」— **代码已落地、真机未生效**：装机副本 `weaselx64.dll` 被 17 个在跑 64 位进程占用、无法替换（x86 `weasel.dll` 已替换成功），需注销 / 重启 Windows 后重装。
      ★ **判据修正（勿再用字形计数）**：UIA（`tmp/v12-tray-j.txt`）显示通知区域**并存两个「任务栏输入指示」** —— 火流猩自己的（tooltip「左键切换模式，右键打开菜单」，显示方框「中」，即本批摘除对象）与 Windows 自带 IME 指示器（红猿头 logo，**本仓库代码改不掉**，需 Windows 设置 / 组策略）。我们的项在 UIA 中以 tooltip 文案出现、glyph 名为空 ⇒ 按「英/中」计数必然 0 命中 = 假阴性。正确判据 = 按该 tooltip 匹配，或数按钮个数（改前 2 → 改后应 1），并与抓屏 `tmp/v12b-tray-ime.png` 互证。
      ③ tooltip 是否遮挡面板槽位（批次 I 遗留，人眼项）。
- [ ] **T-9J7 开机首次切换卡顿复验**（ADR 0042 遗留）— 需重启 Windows，Agent 不可执行，交用户。

