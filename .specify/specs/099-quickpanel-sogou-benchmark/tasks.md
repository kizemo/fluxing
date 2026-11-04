# 099 · Tasks · 快捷设置栏对标搜狗

> 配套 `spec.md`。**一次只做一片**，每片做完贴测试输出再进下一片。
> 状态标记：`[ ]` 待办 / `[~]` 进行中 / `[x]` 完成（完成时必须把验收输出写进本文件对应行下）。

## P0-1 · 搜狗快捷设置栏黑盒行为采集 【需用户在场配合】

- [ ] 记录搜狗 16.8.0.5415 状态栏右键菜单全项（截图 + 逐项名称/图标含义）
- [ ] 打开其「快捷设置/属性配置」面板，截图首屏与每个分页
- [ ] 逐项点击：记录「点了之后改变了什么」——是否即时生效、是否需要重启、是否有开关态
- [ ] 中/英、全/半角切换时，面板与状态栏的回显变化（这决定 P0-4 的规格）
- [ ] DPI 100% / 150% / 200% 下面板尺寸与清晰度；主/副屏拖拽后的表现
- [ ] 常驻 vs 临时出现的行为差异（对照我们的 `kAlwaysShow`）
- [ ] 产出 `docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md`：每条观察 → 截图相对路径
- [ ] 未观察到的项写「未验证」，**禁止用推测填充**

## P0-2 · 差距矩阵与取舍

- [ ] 三列表：搜狗行为 × 我方现状（源码位置） × 设计意图（`docs\design\quickpanel-v{3,4,5}`）
- [ ] 每行判定「借鉴 / 改造 / 不抄」+ 理由
- [ ] 每行填「借鉴来源（证据文件全路径）」与「差异化实现」；填不出的行删除
- [ ] 产出 `.specify\specs\099-quickpanel-sogou-benchmark\gap-matrix.md`

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

- [ ] 确认状态来源：面板 `Show()` 现有 `currentFullwidth` 入参；中文态需从 server 侧取（**禁 TSF 阻塞 I/O，P2**）
- [ ] 先写失败测试：状态切换后面板回显跟随
- [ ] 实现绘制 + 命中区不变；不得改动 `quickpanel.state` 语义
- [ ] 贴 e2e 输出

## P0-5 · 键盘可达性 + 槽位 tooltip

- [ ] 失败测试先行：Tab / Shift+Tab / ← → 移焦、Enter|Space 激活、Esc 收起
- [ ] Esc 行为**必守 D0032**（只收面板、不得连带收起候选窗；用例 `test\regression\cases\D0032-esc-no-cascade.ps1`）
- [ ] tooltip：每槽「名称 + 当前态」，品牌 tooltip 现有实现可扩展
- [ ] 扩展 `scripts\gate\L1-quickpanel-hotkey.ps1` 并**造坏数据证明新断言会红**

## P0-6 · 装机版 DPI 复验（T5 收口）【可立即开工】

- [ ] 探测 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 版本（应为 1.1.9.0，本轮已证）
- [ ] 起 server（**先停计划任务 `FluxingDeployerMaintenance` 再 `Start-Process`**），在 150% DPI 下量面板物理尺寸
- [ ] 期望 368×72（修复后），若仍 245×48 ⇒ 登记 `test\regression\DEFECTS.yaml` 新 `D00xx` 并写 heavy 复现用例
- [ ] 100% / 200% 各测一次；多屏各测一次

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
