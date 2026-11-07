# Handoff 2026-11-07（批次 G）：全半角按钮图标重做为「按钮内对角线」矢量布局（v7）

> **状态：代码 / 单测 / 全量构建 / NSIS 打包 / 静默装机 / 真机像素验收全部走完并通过**；L0 静态门禁 PASS。**唯一未完成的是提交与推送**（见 §五 执行序第 1 步）。
> 本文是**新建**文件（未改写 `handoff-2026-11-07-quickpanel-v6-punct-icon-and-shrink.md` 等更早文档）。下一会话请按 §五 执行序接手，**勿重做已完成的取证与构建**，**勿改写本文以外的历史 handoff / prompt**。
>
> **分支 / 提交**：分支 `Fluxing`，本地 **ahead 3**（`38b4c73` / `1df3fad` / `5b1a46f` 均为批次 F，**尚未推送**，`kizemo/Fluxing` 仍停在 `d4aa63a`）；本文对应的工作区改动**尚未提交**。工作区除本文 §四 所列文件外仅剩 ` m librime` 子模块假脏（**不得提交**）。

---

## 一、当前状态（一段话）

批次 G 只有一条用户指令：「设置栏图标中，全半角符号太小，不具有可视度。可以调整排版，参考搜狗输入法，按钮内对角线布局，从而可以放大。」**已按搜狗的「。」+「,」对角构型重做为纯 GDI 矢量图元**：全角 = 右上空心环（14×14 raw px、2px 描边）+ 左下大逗号尾；半角 = 右上实心点（10×10）+ 左下小逗号尾。两者以「空心 vs 实心 + 尺寸差」双重编码当前态，旧实现的 `，。` / `,.` **字体字形组合（v6）已弃用**——实测 CJK 标点墨迹只占 em 盒 ≈0.2，-15px 下全角仅 **18×4 raw px**、半角仅 **8×5 raw px**，放大字号长不大且会撑破 28px 键帽。v7 真机实测：全角墨迹 **25×25 raw px**、半角 **21×23 raw px**，双轴居中偏差 ≤1px，点击可双向切换、图标随 `ascii_mode` 自动变化，7 条断言全 True。**未做**：提交与推送、人眼观感确认（§八）、Bug #4 真机复现（继承批次 F）。

---

## 二、必读文档（全绝对路径，按此顺序）

| # | 文档 | 为什么必读 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 进站导航：强制项、分流规则、专项规则表 |
| 2 | `F:\soft\00selfmade\rime_claude\AGENTS.md` | 操作手册（§1 首屏 / §4 危险区 / §5 checklist / §9 交接双产物） |
| 3 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` | 身份 / 模块 / 词汇 / 不变量 |
| 4 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 法则层 R1–R9 / P1–P8（P2 禁 TSF 阻塞 I/O、P4 commit 规范、P5 中文化） |
| 5 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` | §A 部署、§11 NSIS 配方 |
| 6 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` | 工程技能消费契约 |
| 7 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v6-punct-icon-and-shrink.md` | 上一批（批次 F）完成态；本批的基线与「未完成项」来源；**本文不重复其内容** |
| 8 | `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\spec.md` | 本面板的需求基线（含「显示路径零 I/O」约束） |
| 9 | `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\tasks.md` | 阶段任务与验收判据（本批新增 `P0-8`） |
| 10 | `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` | 事故溯源；v4 logo 黑框（禁 `AlphaBlend`）的前科 |
| 11 | `F:\soft\00selfmade\rime_claude\docs\adr\0037-*.md`、`0038-*.md` | 面板几何 / 图标层决策（改几何前必读） |
| 12 | `F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md` | 仅当要改 `output/data/**/*.yaml` 时读 §5–§7、§9、§10 |

---

## 三、本批进度（逐项，含验证等级）
> **复验记录（批次 G 收尾）**：`RoundRect` 修正后跑了一条「build → 静默装机 → 探针 → 裁片 → 逐像素 ASCII」的后台链（`tmp\chain-batch-h.log`），**首轮探针 VERDICT=FAIL**；**判定为「装机后立刻探针」的时序假失败**，不是代码缺陷 —— 该轮 `WHOLE_PANEL_DIFF A_vs_B=0`（点击根本没送达按钮），且 slot2 顶部多出一条贯穿整按钮宽的横线（`INK bbox` 被它撑成 42×24），说明抓到的是刚重启 `WeaselServer` 的非常规态。**同一二进制、不重装、不重启，重跑探针即 VERDICT=PASS（7/7 全 True）**：`A_vs_B=165`、`B_vs_C=165`、全角墨迹 **22×23**、半角 **18×22**、双轴居中 (20.5,21)/(20.5,20.5)、`B_vs_E=0`、`C_vs_F=0`。**教训：装机后必须先让 `WeaselServer` 稳定驻留再跑探针，且「整面板 diff=0」要优先怀疑点击未送达而非图标逻辑。**

| # | 用户指令 | 状态 | 结论与落点 | 验证等级 |
|---|---|---|---|---|
| 1 | 全半角符号太小、不具可视度 | ✅ 完成 | 新增 `F:\soft\00selfmade\rime_claude\include\FluxingPanelFwIcon.h`（namespace `fluxing::panel_fwicon`）：`Build(bool fullwidth)` 返回 3 个图元（全角：空心环 + 逗号头 + 尾三角；半角：实心点 + 小逗号头 + 尾三角）+ 声明墨迹盒 `RECT`。`DrawIconFullwidth()`（`WeaselServer\QuickPanelDialog.cpp`）整段重写为矢量绘制：`CreatePen(PS_SOLID, 2, color)` + `NULL_BRUSH` 画空心环，`CreateSolidBrush` + `NULL_PEN` 画实心件。**全程零 `AlphaBlend`**。 | **verified**（真机像素：全角 25×25、半角 21×23，双轴居中） |
| 2 | 参考搜狗输入法 | ✅ 完成 | 形态照抄搜狗「。」+「,」对角（参考图 `docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-status-zoom.png`，槽 3）。全角=空心大环（直径 14 raw px）+ 大逗号；半角=实心点（10 raw px）+ 小逗号。**空心 vs 实心 + 尺寸差**双重编码当前态。 | **verified**（逐像素 ASCII 取证：`tmp\batchg-slot2-ascii.txt`，环右上、逗号左下，空心环轮廓规整） |
| 3 | 按钮内对角线布局，从而可以放大 | ✅ 完成 | 两态均按「右上 + 左下」对角排布；`CenteringOffset(ic, kIconViewBox)` 用**墨迹盒并集 + 就近舍入**（`(viewBox − w + 1)/2`）使中心恰为 (15,15)，与另外 5 个线描图标对齐。墨迹盒全角 `RECT{3,3,27,27}`（25×25）、半角 `RECT{4,5,24,27}`（21×23），均落在 30×30 viewBox 内、不超 28px 键帽。 | **verified**（T33a–T33i 9 条断言 + 真机 7 条断言全 True） |
| 4 | 「随 `s_inputState.fullwidth` 切换」语义不得退化 | ✅ 完成 | 状态源沿用批次 F 的 `GetPanelInputState()`（进程内 C API 读内存，符合 spec 099「显示路径零 I/O」），**未改 IPC 协议**（`include\WeaselIPCData.h` 未动 ⇒ 无需四边同步 + ADR + 新 IPC 测试）。 | **verified**（`ICON_FOLLOWS_ASCII=True`、`ASCII_TOGGLE_REVERSIBLE=True`、`FULLWIDTH_TOGGLE_REVERSIBLE=True`） |
| 5 | 清理 v6 遗留 | ✅ 完成 | `s_hFontPunct` 的**声明 / 定义 / 创建（`ApplyPhysicalLayout`）/ 释放（`OnDestroy`）整条生命周期已删**；`QuickPanelDialog.h` 中重复的 `panel_fwicon` 命名空间块（原 616–709 行）一并删除。 | **verified**（编译通过、`grep` 无残留、单测 240/240） |

---

## 四、改动文件清单（提交前对照）

| 文件 | 动作 | 说明 |
|---|---|---|
| `F:\soft\00selfmade\rime_claude\include\FluxingPanelFwIcon.h` | **新增** | 纯逻辑几何表（`ShapeKind` / `Shape` / `Icon` / `Build()` / `CenteringOffset()`）；`constexpr int kStrokeWidth = 2`；**无 BOM / LF / UTF-8** |
| `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.h` | 改 | 加 `#include <FluxingPanelFwIcon.h>`（line 51）；删 `static HFONT s_hFontPunct;` 及注释；删文件尾部重复命名空间块 |
| `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.cpp` | 改 | `DrawIconFullwidth()` 整段重写为矢量实现；删 `s_hFontPunct` 定义/创建/释放；`case 5:` 注释改写为 v7 |
| `F:\soft\00selfmade\rime_claude\test\TestQuickPanelDialog\TestQuickPanelDialog.cpp` | 改 | 加 `#include "../../include/FluxingPanelFwIcon.h"`（line 42 后）；新增 **T33a–T33i 共 9 条断言**（墨迹盒 25×25 / 21×23、两态居中中心 == (15,15)、全角 1 空心 + 2 实心 / 半角 0 空心 + 3 实心、空心环直径 ≥ 14、墨迹盒落在 viewBox 内且不超 28 键帽、两态对角方位） |
| `F:\soft\00selfmade\rime_claude\CHANGELOG.md` | 改 | `## [Unreleased]` 顶部新增 v7 条目（用户原话、形态、弃用 v6 的原因与 18×4 / 8×5 实测、实现落点、`s_hFontPunct` 已删、未改 IPC/install.nsi/librime、验收指向 T33 + 真机像素）。保持 CRLF / 无 BOM |
| `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\tasks.md` | 改 | 在 `## P1 · 声明式槽位表` 前插入 `## P0-8 · 全半角图标按钮内对角线重做（v7）`（取证/参考/设计定稿/实现/单测 `[x]`，真机像素验收 `[ ]`）。保持 LF / 无 BOM |
| `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md` | **新增** | 本文 |
| `F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-07-quickpanel-v7-fwicon-diagonal.md` | **新增** | 接力 prompt（单段落 + 全路径指针，不复述本文内容） |
| `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 改 | §2 加本文指针 |

> **不得提交**：` m librime`（子模块假脏）、`weasel.props`、`env.bat`、`*.obj`、`*.log`。

---

## 五、下一阶段执行序（按此顺序，理由随附）

1. **提交 + 推送批次 F 与 G** —— 理由：本地 ahead 3（批次 F）且批次 G 工作区改动未提交，远端仍停在 `d4aa63a`；先对齐远端，下一会话才有一致基线。**必须显式按路径 `git add`（禁止 `git add .`）**，commit 用 Conventional Commits + **单一 scope**，例如 `feat(WeaselServer): 全半角按钮图标改为按钮内对角线矢量布局`。命令 `git push origin Fluxing`，推送前再核一次 `git status -sb`（只应剩 ` m librime`）。
2. **把 §八 的「人眼确认」交给用户** —— 理由：数值上墨迹 25×25 / 21×23 raw px、居中偏差 ≤1px 已达标，但「看着够不够大够不够清楚」只能由人判断；本会话模型读图能力有限。
3. **Bug #4（切输入法后一小段时间只能打英文）真机复现** —— 理由：根因已在 `lessons-learned.md` L196 坐实但**缺运行时佐证**；需用户在场配合「切换后立即打字」，探针开工前先 `Get-Process LogonUI` 判锁屏。
4. **清理旧探针坐标** —— `tmp\qpanel-analyze.ps1`、`tmp\probe-qpanel-menu.ps1` 内仍按旧的 239×36 写，直接跑会裁错区域；要么改 215×32，要么删。理由：留着会制造假失败。
5. **x86 TSF shim 增量链接遗留** —— `D:\Program Files\fluxing\weasel\weasel.dll` 仍是旧件（批次 E/F/G 未改 WeaselTSF，增量跳过链接）；下次动 TSF 时必须确认它真的被重链接并装进安装目录。理由：静默跳过的增量链接会让真机验错对象。

---

## 六、证据路径（全部绝对路径）
| **最终复验探针日志（首选证据，VERDICT=PASS）** | `F:\soft\00selfmade\rime_claude\tmp\batchi.log` |
| 最终复验帧位图 / 裁片 | `F:\soft\00selfmade\rime_claude\tmp\batchi-{A,B,C,E,F}.bmp`、`tmp\crop-batchi-slot2-{A,B,C,E,F}.png`、`tmp\crop-batchi-panel-B.png` |
| 装机后立刻探针的**假失败**对照（勿当成缺陷） | `F:\soft\00selfmade\rime_claude\tmp\chain-batch-h.log`、`tmp\batchh.log` |

| 用途 | 路径 |
|---|---|
| 逐像素 ASCII 取证（**v7 对角布局的直接证据**） | `F:\soft\00selfmade\rime_claude\tmp\batchg-slot2-ascii.txt` |
| 真机帧位图（32bpp top-down BMP） | `F:\soft\00selfmade\rime_claude\tmp\batchg-{A,B,C,E,F}.bmp` |
| 放大裁片（供人眼复查） | `F:\soft\00selfmade\rime_claude\tmp\crop-batchg-slot2-{A,B,C,E,F}.png`、`tmp\crop-batchg-panel-B.png` |
| 真机探针日志（**首选证据**，VERDICT=PASS） | `F:\soft\00selfmade\rime_claude\tmp\probe-batchg-console.log` |
| GDI 形状实测（`Ellipse` vs `RoundRect` 光栅化） | `F:\soft\00selfmade\rime_claude\tmp\gdi-shape2.txt`、`tmp\gdi-ellipse-shape.txt` |
| RoundRect 修正后复验链日志 | `F:\soft\00selfmade\rime_claude\tmp\chain-batch-h.log`（含 build→install→probe→crop→ascii） |
| 构建 / 打包日志 | `F:\soft\00selfmade\rime_claude\tmp\build-batch-h.log` |
| 静默装机日志 | `F:\soft\00selfmade\rime_claude\tmp\reinstall3-batch-e.log` |
| 旧（v6）图标裁片（对比用） | `F:\soft\00selfmade\rime_claude\tmp\crop-batchf5-slot2-B.png` |
| 搜狗参考图 | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-status-zoom.png` |
| 探针 / 裁片 / ASCII / GDI 实测脚本 | `F:\soft\00selfmade\rime_claude\tmp\probe-batchf-state.ps1`、`tmp\crop-batchf5.ps1`、`tmp\dump-slot2-ascii.ps1`、`tmp\gdi-ellipse-shape.ps1`、`tmp\gdi-shape2.ps1` |
| 本批自建落盘脚本 | `tmp\fix-fwicon-header.ps1`、`tmp\add-t33-tests.ps1`、`tmp\update-docs-v7.ps1`、`tmp\run-batch-h.ps1` |

**可直接复用的命令（勿重造）**

```powershell
# 单测（改 UI 后必跑）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-qpanel-tests.cmd"

# 全量构建 + NSIS（>100s，必须后台跑）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-batch-e.cmd" 2>&1 | Tee-Object -FilePath "F:\soft\00selfmade\rime_claude\tmp\build-batch-h.log"

# 静默重装 + 哈希核对
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\tmp\reinstall3-batch-e.ps1"

# 真机探针（脚本跑一次，再单独读日志）
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\tmp\probe-batchf-state.ps1" -OutBase 'batchh'
Get-Content 'F:\soft\00selfmade\rime_claude\tmp\batchh.log' -Encoding utf8

# 裁片 / 逐像素 ASCII
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\tmp\crop-batchf5.ps1" -OutBase 'batchh'
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\tmp\dump-slot2-ascii.ps1" -OutBase 'batchh'

# 门禁
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\scripts\gate\L0-static.ps1" -Mode source -SkipNsisCompile -SkipGuardUnitTest
```

---

## 七、本批决策（改之前先读这里，别推翻重来）

| 编号 | 决策 | 依据 |
|---|---|---|
| D1 | **弃用 v6 字体字形组合，改用 GDI 矢量图元** | CJK 标点墨迹只占 em 盒 ≈0.2 ⇒ -15px 下全角仅 18×4 raw px、半角 8×5；放大字号长不大且会撑破 28px 键帽。矢量图元可精确控制直径 |
| D2 | 形态照抄搜狗「。」+「,」对角；**空心 vs 实心 + 尺寸差**双重编码状态 | 用户原话「参考搜狗输入法，按钮内对角线布局」；且不能退化批次 F 建立的「图标随 `ascii_mode` 变化」语义 |
| D3 | 纯逻辑下沉到 `include/FluxingPanelFwIcon.h`（header-only `inline`） | 对齐既有 `FluxingPanelDpi/Status/Keyboard/Tooltip/Logo/Position` 六个同类文件约定（无 BOM / LF / UTF-8）；生产与单测**同源**（宪法 R7 单一事实源） |
| D4 | 居中用「**墨迹盒并集 + 就近舍入**」`(viewBox − w + 1)/2` | 使中心恰为 (15,15)，与另外 5 个线描图标对齐；用 `ink` 而非各图元 bbox，避免偏心 |
| D5 | 图标仍按 **raw px 不随 dpr 缩放**（viewBox 30×30、键帽 28 logical） | 与既有 5 个线描图标同口径；`kIconViewBox` / `kIconBbox*Off` 不变 |
| D6 | **小尺寸实心圆点必须用 `RoundRect`（圆角半径 = 半边长），不得用 `Ellipse`** | 实测 GDI 把 ≤10px 实心椭圆光栅化成**四角星**（顶点仅 1px，不像点）：`Ellipse` box=10/12 顶点均 1px、box=14 才 3px 成正圆；`RoundRect` 半径=半边长在 box=10 即为干净 9×9 正圆。证据 `tmp\gdi-shape2.txt`。据此删除 `kSolidEllipse` 枚举值 → 改 `kSolidRoundRect`，三处实心件全走 `RoundRect`（几何/墨迹盒不变 ⇒ T33 全成立） |
| D7 | GDI 纪律：空心环配 `NULL_BRUSH`；实心件配 `NULL_PEN`；只 `DeleteObject` 自己 `Create` 的 pen/brush | `GetStockObject(NULL_BRUSH/NULL_PEN)` 是共享 stock 对象，`DeleteObject` 不可对其调用 |
| D8 | **不动** `include\WeaselIPCData.h`、`output\install.nsi`、`librime\`、面板几何常量（215×32）；**不改 status 源** | 改 IPC 要四边同步 + ADR + 新 IPC 测试（宪法硬约束）；状态源进程内读内存符合 spec 099「显示路径零 I/O」 |

---

## 八、需要用户到场的两件事

1. **人眼确认 v7 图标观感是否「够大够清楚」** —— 数值上全角 25×25、半角 21×23 raw px（旧版 18×4 / 8×5），居中偏差 ≤1px，颜色 `#82828C`（与同级图标一致）；但「看着够不够大」只能由人判断。放大裁片见 §六（`tmp\crop-batchg-slot2-{A,B}.png`）。**这是本批唯一未闭环的验收项。**
2. **Bug #4 复现配合** —— 切换输入法后**立即**打字，观察是否出中文；同时留意 `WeaselServer` 是否在跑。探针开工前先 `Get-Process LogonUI` 判锁屏。

---

## 九、本批工具坑（重蹈覆辙会白烧一轮构建）

1. **小尺寸实心圆用 `Ellipse` 会被光栅化成四角星** —— 见 §七 D6。若只看代码不看像素，会误判为「点太小」。
2. **`GetStockObject(NULL_BRUSH/NULL_PEN)` 不可 `DeleteObject`** —— stock 对象是共享的。
3. **`edit` 工具参数里写 `\uFF0C` / `\n` 会被 JSON 解析成真字符** ⇒ 匹配失败；含反斜杠的源码锚点必须改用脚本按边界正则替换。
4. **`edit` 工具偶发把空 `new_string` 报成「must be string」** —— 改用脚本或补一个非空最小差异。
5. **`bash` 里先 `cd` 再跑命令会被本地安全策略拦** ⇒ 一律用 `git -C` 或绝对路径。
6. **`GetDIBits` 必须传实际 bitmap 句柄**（传 `IntPtr.Zero` ⇒ 整块读成黑）；**`FillRect` 的刷子不应先 `SelectObject` 进 DC**。
7. **C# P/Invoke 形参重名**（如 `int h` 既是句柄又是高度）⇒ 编译报 CS0100。
8. **PowerShell `for` 循环初始化式写成比较表达式**（`for ($j = $y -lt $y + $h; ...)`）会静默走错 ⇒ 逐像素脚本改走 BMP 字节直读。
9. **BMP 高度为负（top-down）** —— 逐像素循环 0 次 ⇒ diff=0 假象。
10. **禁止 `git add .`**；`scripts\gate\` 内**没有** clang-format 自动检查（grep 0 命中），`.clang-format` = Chromium + `SortIncludes: false`（列宽 80、CJK 按显示宽度计），改完需人工保持 ≤80 列。
11. **本批中断原因** —— 上一轮工具调用累计 400+，上下文被压缩两次后终止。预防：每阶段结束立即落盘、单轮 ≤10 次调用。
12. **装机后立刻跑探针会假失败** —— 本批 chain-batch-h 首轮 VERDICT=FAIL（WHOLE_PANEL_DIFF=0、slot2 多一条贯穿横线），**同一二进制重跑即 PASS**。装机后先让 WeaselServer 稳定驻留再探；WHOLE_PANEL_DIFF=0 优先怀疑「点击未送达」而非图标逻辑。
