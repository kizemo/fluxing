# 0022. 快捷面板「常驻」改为默认态 + 静止态整块 85% 不透明 + 「重启输入法服务」走脱离式 helper

- **状态**: Accepted
- **实现修订**: 见文末「实现落地情况修订（2026-10-30 收尾，随 v1.1.5 发布）」—— D1 / D2 的实际改法与下文不同，D3–D7 本轮延后。
- **日期**: 2026-10-30
- **关联**: ADR 0013 / 0018 D1-D2 / 0019 / 0020 / 0021 §2 §D6 · spec 055（启动不主动弹面板）· spec 087 · spec 089 · 交接卡 §24 第 1/2/6/7 条
- **相关**: `include/QuickPanelAlpha.h` · `include/FluxingQuickPanelState.h`（新） · `WeaselServer/QuickPanelDialog.{h,cpp}` · `WeaselServer/WeaselServerApp.{h,cpp}` · `WeaselServer/WeaselServer.rc` · `WeaselServer/WeaselTrayIcon.cpp` · `WeaselTSF/LanguageBar.cpp` · `WeaselTSF/WeaselTSF.rc`

## 背景（Context）

v1.1.4 真机反馈 4 条中，与面板 / 菜单 / 服务重启相关的部分。

### 1. 「勾选态不回显」的报错点是**语言栏菜单**，不是托盘菜单

用户原话：「勾选『快速面板常驻』后，右键菜单这个选项状态没有变化，但确实快捷面板常住了。」

取证：托盘菜单**已经有**回显 —— `WeaselServer/WeaselTrayIcon.cpp:32`
`CheckMenuItem(hMenu, id, MF_BYCOMMAND | MF_CHECKED)`，勾选态由
`WeaselServer/WeaselServerApp.cpp:27-30` 的 `SetMenuItemCheckedCallback` 提供。
语言栏菜单**没有** —— `WeaselTSF/LanguageBar.cpp:186-189` 在 `TrackPopupMenuEx` 之前
没有任何 `CheckMenuItem`；全仓 `CheckMenuItem` 只命中 `WeaselTrayIcon.cpp:32` 一处。

⇒ 用户点的是任务栏输入法图标（语言栏）菜单。这也**修正** ADR 0021 §2 的推论
（当时据「`quickpanel.state` 四处均不存在」判定用户从未勾选过）：本轮用户确实勾了，
且是从语言栏菜单勾的 —— 恰好是没有回显的那一个。

### 2. 「希望取消这个选项」的正确读法是「取消**默认勾选**语义」，不是「删掉菜单项」

用户原话后半段：「……如果用户不希望显示，就通过快捷键 `Alt+,` 或者右键菜单取消。」
⇒ 明确要求**保留**右键菜单作为关闭入口。前半句「希望取消这个选项」只能读作
「取消它的默认勾选语义」，即默认就常驻。删掉菜单项会与后半句自相矛盾。

### 3. 1.5 秒自动消失是**裸字面量**

`WeaselServer/QuickPanelDialog.cpp:742-747`（100 ms 轮询器内）：

```cpp
if (!AlwaysShowMode() && hit == -1 && !s_dragging && !inGrace) {
  s_outsideMs += 100;
  if (s_outsideMs >= 1500) { s_outsideMs = 0; Hide(); }
}
```

`kShowGraceMs = 2000`（`QuickPanelDialog.h:219`）有常量，1500 没有。
`RestoreAlwaysShowMode()`（`:1556-1559`）读不到状态即 `kHidden`
（`LoadPersistedAlwaysShowModeFrom`，`:1533-1543`，读不到 `return false`）；
`SetAlwaysShowMode`（`:1565-1577`）同值早退**不写盘** ⇒ 从未点过菜单的用户磁盘上根本没有状态文件。

### 4. 静止态「透明度过高」是**两层相乘**的结果，不是单一因子

面板是 `UpdateLayeredWindow` 逐像素 alpha。`ApplyAlphaGradient()`
（`QuickPanelDialog.cpp:1101-1149`）先按 y 插值出 `aPanel`
（`kAlphaPanelTop = 220` / `kAlphaPanelBot = 80`，`QuickPanelDialog.h:258-259`），
再乘 `s_alphaFactor`（静止态 `kAlphaFactorIdle = 153`，`QuickPanelAlpha.h:41`）：

| 像素类型 | aPanel | × factor | 有效不透明度 |
|---|---|---|---|
| 彩色（图标 / 文字） | 255 | 153/255 | **60.0%** |
| 背景·顶 | 220 | 153/255 | **51.8%** |
| 背景·底 | 80 | 153/255 | **18.8%** |

用户要「不透明度 85%」= factor `217/255`。**只改 factor 不改基数**的话，背景底部仍是
`80 × 217 / 255 = 68%` ⇒ 用户看到的仍是「上深下浅、底部发虚」，会继续投诉。

### 5. 「重启算法服务」在语言栏侧是一份**独立实现**，而且它并不「重启」

`WeaselTSF/LanguageBar.cpp:334-358` 的 `RERUN_SERVICE` / `INSTALLDIR` 共用分支自己
`ShellExecuteW` 启动 `WeaselServiceLaunchTarget(root)` —— **只 start，不 stop**。
托盘菜单里**根本没有**这一项：`include/resource.h:28` 早已定义
`ID_WEASELTRAY_RERUN_SERVICE 40015`，但 `WeaselServer/WeaselServer.rc` 的
简中（`:157-180`）/ 繁中（`:251-274`）/ 英文（`:345-368`）三块都没有它。

⇒ 同一个「重启服务」语义在两处各写一份，且都不是「重启」。

## 决策（Decision）

### D1 「常驻」的默认值用**三态**表达，不动既有 `false` 语义

新增 header-only 的 `include/FluxingQuickPanelState.h`（ADR 0020 / 0021 同款范式：
server 与 test 两侧都能直接调用，测试无需改工程链接），承载
`kAlwaysShowDefault = true`、`kAutoHideMs = 1500`、状态文件名等常量，
以及一个返回 `std::optional<bool>` 的只读查询（**缺失 ≠ false**，用类型强制区分）。

`RestoreAlwaysShowMode()` 改为 `found ? value : kAlwaysShowDefault`。
`LoadPersistedAlwaysShowMode()` / `LoadPersistedAlwaysShowModeFrom()` 的
「读不到 ⇒ `false`」语义**逐字保留**，实现改为薄封装 `found ? value : false`。

理由：`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp:289-325` 的 T13a–T13h
断言了「文件缺失 ⇒ false」，那是 ADR 0019 / 0020 的既有契约。直接改兜底值会让
这 8 条断言全红，并把「持久化机制坏了」与「默认值变了」两件事混在一个失败信号里。

**被否决**：把 `LoadPersistedAlwaysShowMode()` 的兜底直接改成 `true` ——
除上述回归面外，还会让函数名与语义不符（"Load" 却返回一个从未加载过的默认值）。

### D1b 「默认常驻」≠「登录后自动弹出面板」

spec 055 的不变量（启动时不主动弹面板）**保留**。常驻只影响
「已经显示出来的面板会不会自己消失」。用户的工作流是 `Alt+,` 唤出后即长期停留，
与该不变量不冲突。

**残留风险（已披露）**：若用户真实期望是「登录后面板就在屏幕上」，本决策不满足，
需要单独一条 spec 增加「开机自动显示」开关。已写入 spec 089 US1 的边界说明与交接卡。

### D2 静止态**有意牺牲渐变**，换「整块一致 85%」

`kAlphaFactorIdle` 153 → **217**；`kAlphaPanelTop`(220) 与 `kAlphaPanelBot`(80)
**双双**改为新常量 `fluxing::kAlphaPanelBase`(255，置于 `QuickPanelAlpha.h:94` 附近)。
改后三类像素有效不透明度**全部** = `217/255` = **85.1%**。
`ScaleAlpha(base, factor)`（`:51-61`）在 `factor >= 255` 时直接返回 base；
217 < 255 ⇒ 走乘法分支，`255 × 217 / 255 = 217` ✓。

理由：用户给出的是一个**标量**诉求（85%）；标量与「背景自上而下渐隐」不可同时满足，
而 18.8% 的底部在真实桌面上等于看不见 —— 正是本次投诉的成因。

hover 态仍走 `kAlphaFactorFull = 255`（`:29`）⇒ 鼠标移上去仍然「变清晰」。
梯度从**空间渐变**转移到**交互态渐变**，ADR 0018 D2 的不变式
「静止态仍可辨地退在 hover 态之后」保留（85% < 100%）。

同步改 `TestQuickPanelDialog.cpp:243-244` 的 T12d / T12e 硬编码断言
（`ScaleAlpha(255, kAlphaFactorIdle)` 153 → **217**；`ScaleAlpha(220, kAlphaFactorIdle)` 132 → **187**）。

### D3 语言栏菜单补勾选态回显，回显源与托盘**同源**

在 `LanguageBar.cpp:186-189` 的 `TrackPopupMenuEx` **之前**，对
`ID_QUICKPANEL_ALWAYS_SHOW` 调 `CheckMenuItem`。勾选态**不在 TSF 进程内另存一份**：
`OnClick` 直接读 ADR 0020 已收敛到用户数据目录的那个状态文件。
⇒ 单一事实源仍是那个文件，托盘与语言栏读同一份（P1）。

`include/WeaselIPC.h:18-40` 的命令枚举**不新增**任何查询 / refresh 消息
⇒ 不触发 CLAUDE.md §2 的「IPC 四边同步」硬规则。

**被否决**：新增 `WEASEL_IPC_GET_QUICKPANEL_STATE` —— 四边同步（server / TSF / deployer
+ ADR + `test/TestWeaselIPC/*`）成本高，而状态文件本就在磁盘上、TSF 进程有读权限。

### D4 「重启输入法服务」= 脱离式 helper 批处理「停-等-起」，两条菜单路径**同源**

前端把 helper 批处理**生成到 `%TEMP%`**（不往 `output/` 加文件 ⇒ 不触碰
`output/install.nsi` 的仓外 Sandbox 门禁），内容为：短暂延迟 → 停服务
（等价 `output/stop_service.bat` 的 `weaselserver.exe /q`）→ 轮询进程消失 →
`start_service.bat` 拉起；用 `ShellExecuteW(NULL, L"open", L"cmd.exe", L"/c \"<helper>\"", NULL, SW_HIDE)`
启动 ⇒ helper 生命周期**脱离** WeaselServer 进程，服务自杀后仍能把服务拉回来。

托盘菜单**新增** `ID_WEASELTRAY_RERUN_SERVICE`（三语言块各一条）；
语言栏那条本地 `ShellExecuteW` 分支**删除**，改为落到已有的
`default: m_client.TrayCommand(wID)`（`:384-386`）⇒ 两条路径同一份实现。
摘出后该分支只剩 `ID_WEASELTRAY_INSTALLDIR`，`LooksLikeWeaselRoot` 校验保留。

文案统一为「重启输入法服务(&E)」（原语言栏简中「重启算法服务」/ 繁中「重啓算法服務」/
英文「Restart (&E)」；托盘此前无此项）。

**被否决**：起第二个 WeaselServer 实例顶掉第一个 ——
`WeaselServer/WeaselServer.cpp:145-163` 的单实例逻辑在 `retry >= 10` 时 `return 0`，
即第二个实例**静默退出**；若第一个此时已被停掉，结果是「服务没了且没有任何提示」，比现状更糟。

**失败可感知**：路径与启动目标校验在**启动 helper 之前**完成，失败弹简体中文
`MessageBoxW`，沿用 ADR 0021 D6 的两条文案。helper 自身的失败（服务没回来）
由前端下一次交互时的「服务未运行自动拉起」路径兜底（ADR 0021 已修好该路径的空串问题）。

### D5 语言栏 `OnClick` 读小状态文件**不违反** P2

P2 红线的字面范围是「`ITfTextInputProcessor::*` 回调内禁止 `CreateFile` /
同步注册表 / `WaitForSingleObject`」。`ITfLangBarItemButton::OnClick`
**不属于** `ITfTextInputProcessor` 接口族（它由语言栏在自己线程上回调，
且用户点击本身已是一次同步 UI 交互）。读的是一个数十字节的状态文件，
而 ADR 0019 / 0020 已让服务端在同一文件上做同步读写。

仍作两条自我约束：① 只读打开且带 `FILE_SHARE_READ | FILE_SHARE_WRITE`，
避免与服务端写入互锁；② 读失败一律按「缺失」处理（三态 ⇒ 常驻），
**不得**因读失败而弹框、重试或阻塞。

### D6 开发向菜单项收进「高级(&V)」子菜单

主菜单只留用户向项（具体三项见 spec 089 US4 / T5）。
`HMENU2ITfMenu`（`LanguageBar.cpp:18-41`）已支持子菜单 ⇒ 语言栏侧零新增机制。

**被否决**：直接删掉这些项 —— 「用户文件夹」是交接卡 §24.5 验收清单里用户自己要用的入口。

### D7 退出确认框落在**服务端 QUIT handler 一处**

`WeaselServerApp.cpp:297-298` 的 QUIT 分支（现为 `m_server.Stop() == 0`）前加确认框。
语言栏侧**不加**第二道确认 —— 否则经由 `TrayCommand` 转发会出现「弹两次」。
⇒ 托盘与语言栏两条退出路径行为一致。

## 后果（Consequences）

- (+) 新用户与升级用户**默认常驻**；不必先去发现那个藏在两个不同菜单里的开关
  （ADR 0021 §2 记录的「入口错位」从根上消失）。
- (+) 静止态整块 85%，用户给出的标量诉求被逐字满足，不再是「只有图标变亮」。
- (+) 「重启输入法服务」第一次真正是「停-等-起」，且两条菜单路径同源（P1）。
- (+) 语言栏菜单第一次有勾选态回显。
- (−) **有意牺牲**：ADR 0013 / 0018 建立的「背景自上而下渐隐」视觉被拉平；
  `kAlphaPanelTop` / `kAlphaPanelBot` 退化为同一个值（保留符号，避免其他引用点与测试断裂）。
- (−) 三态语义让「状态文件缺失」不再等价于「非常驻」。今后任何读该文件的代码
  必须区分「缺失」与「值为 false」，否则会把默认值读反 —— 已用 `std::optional<bool>`
  在类型层面强制，不允许再用裸 `bool` 返回。
- (−) helper 批处理是**运行期生成的第二份事实源**（内容硬编码在 C++ 里）。
  已用「单一生成函数 + 生成即覆写」收敛；但改 `stop_service.bat` / `start_service.bat`
  语义时必须同步该函数，否则会出现「菜单点了、服务没回来」。
- (−) D1b 的残留风险：默认常驻**不**保证登录后自动显示面板。
- 回归测试：T12d / T12e 断言值同步更新；**T13b / T16a / T16e 极性翻正（缺文件 ⇒ 常驻），T13a / T13h / T16c 未改**（详见文末 R1）；
  新增断言覆盖「缺失 ⇒ 常驻」兜底、`kAutoHideMs` 在常驻模式下**完全不累加**（不是把阈值调大）、
  `kAlphaPanelBase × kAlphaFactorIdle` 的乘积。

## 实现落地情况修订（2026-10-30 收尾，随 v1.1.5 发布）

本节由收尾复核补写，**凌驾于上文 D1–D7 中与之冲突的描述**。上文保留原始决策推演
（取证与备选方案）以便追溯；实际落进 v1.1.5 代码的是 D1b、D2（改法不同）、R4、R5，
D3 / D4 / D5 / D6 / D7 本轮全部延后。

### R1 D1 的三态方案**未采用**，改为「缺失 / 损坏 ⇒ true」

`include/FluxingQuickPanelState.h` **从未创建** —— 上文提到的该文件与
`std::optional<bool>` 查询属规划，未落地。实际改法：
`WeaselServer/QuickPanelDialog.cpp` 的 `LoadPersistedAlwaysShowModeFrom(primary, legacy)`
末尾 `return false;` 改为 `return true;`，即「两个落点都读不到 ⇒ 默认常驻」。
`LoadPersistedAlwaysShowMode()` 的函数名与签名未变。

**有意保留的边界语义（原 T13h，极性未翻）**：状态文件**存在**、但内容不是
`always_show=1` ⇒ 返回 `false`（关闭）。只有「文件缺失 / 无法读取」才回落到默认常驻。
⇒ 「默认值变了」与「持久化机制坏了」两个信号仍可区分 —— 这正是 D1 想用
`std::optional<bool>` 在类型层达成的目标，现由「读取成功与否」这条路径达成。

回归面与 D1 上文的预估**相反**：`TestQuickPanelDialog.cpp` 的 T13b / T16a / T16e
三条断言极性被翻正（缺失 ⇒ 常驻），T13a / T13h / T16c 未动。43 / 43 通过。

### R2 D2 的常量方案**未按原文落地**：没有新增 `fluxing::kAlphaPanelBase`

实际改法是把 `WeaselServer/QuickPanelDialog.h:258-259` 的 `kAlphaPanelTop` 与
`kAlphaPanelBot` **就地双双改为 255**，符号名保留（`ApplyAlphaGradient()` 与既有断言的
引用点因此不必改动）；`include/QuickPanelAlpha.h` 未新增任何常量。
`kAlphaFactorIdle` 153 → **217**（`QuickPanelAlpha.h:41`）与 D2 一致。

改后有效不透明度：彩色像素与背景顶 / 底**同为** `255 × 217 / 255 = 217` = **85.1%**
⇒ 整块一致，D2 的目标达成。hover 态 `factor = 255` 命中 `ScaleAlpha` 的
`factor >= kAlphaScaleNum ⇒ return base` 短路分支 ⇒ 100%。

**观感梯度并未全部丢失**：`ApplyAlphaGradient()` 的**颜色**梯度
（`QuickPanelDialog.cpp:66-68`，`kBgTop = (200,225,250)` → `kBgBot = (155,195,240)`）
与圆角区域外 `alpha = 0` 的掩码都保留；被拉平的只是 alpha 沿 y 的 220 → 80 递减。
上文「(−) 有意牺牲」一条据此收窄为「牺牲 alpha 空间渐变，保留颜色渐变与圆角掩码」。

T12d / T12e 期望值同步为 `ScaleAlpha(255, kAlphaFactorIdle) == 217` 与
`ScaleAlpha(220, kAlphaFactorIdle) == 187`；后者仍用字面量 220 钉住纯数学，
**不代表面板还在用 220**。T12a–T12c 用 `kAlphaFactorFull` 触发短路，字面量断言数学上仍成立。

### R3 D3 / D4 / D5 / D6 / D7 本轮**全部延后**，v1.1.5 不含

| 决策 | 状态 | 原因 |
|---|---|---|
| D3 语言栏勾选态回显 | **未落地（结构性受限，且原取证有误）** | 语言栏菜单**不是** `TrackPopupMenuEx` 建的，D3 原文的「在 `TrackPopupMenuEx` 之前调 `CheckMenuItem`」无对应调用点，**作废**。真实链路是 `WeaselTSF/LanguageBar.cpp:18-41` 的 `HMENU2ITfMenu`：把 HMENU 逐项转成 `ITfMenu::AddMenuItem`，其 `mii.fMask` 只取 `MIIM_FTYPE｜MIIM_ID｜MIIM_STRING`，**不读 `MIIM_STATE`**，且恒以 `flags = 0` 调用，从不传 `TF_LBMENUF_CHECKED`。要回显必须改这条转换链；而勾选态活在 WeaselServer.exe 进程内，跨进程取值要么新增 IPC 查询命令（触发 CLAUDE.md §2 四边同步），要么在语言栏回调里读文件（撞 P2 红线论证）。**用户第 1 条反馈的真实诉求「默认就常驻、不必先去勾」已由 R1 覆盖** ⇒ 回显降级为受限项，写入交接卡 §25 待排期。托盘菜单的回显（`WeaselTrayIcon.cpp:23-35` + `WeaselServerApp.cpp:28-30` 谓词）本就正确，未改。 |
| D4 脱离式 helper「停-等-起」 | 未落地 | 需运行期生成批处理 + 三语言菜单块补项 + 文案统一，验收面独立；延后。 |
| D5 语言栏 `OnClick` 读状态文件 | 未落地 | 依附 D3，一并延后。P2 的论证本身未被推翻，只是没有落点。 |
| D6 「高级(&V)」子菜单 | 未落地 | 纯菜单重排，与本轮 4 条反馈无因果关系；延后。 |
| D7 退出确认框 | 未落地 | 延后。 |

⇒ 本 ADR 标题里的「脱离式 helper 服务重启」在 v1.1.5 中**没有对应实现**；
标题与文件名保留，以维持 ADR 编号与 `docs/adr/README.md` 索引稳定。

### R4 「去除 1.5 秒自动消失」无需新增代码

`QuickPanelDialog.cpp:741-744` 的 `s_outsideMs >= 1500` 分支本就被 `!AlwaysShowMode()`
守卫，`:684` 的 grace 路径同样带该守卫 ⇒ 常驻态下该计时器**完全不累加**
（不是把阈值调大）。R1 让常驻成为默认后，这条诉求自动成立。`kAutoHideMs` 常量**未新增**。

### R5 取消入口沿用既有两条，未新增

`Alt+,` → `ID_HOTKEY_QUICK_PANEL`（`resource.h:33`）；菜单项 `ID_QUICKPANEL_ALWAYS_SHOW`
（`resource.h:52`）→ `WeaselServerApp.cpp:374-384` 的 `AddMenuHandler` →
`ToggleAlwaysShowMode()`。spec 055 的不变量保留：`RestoreAlwaysShowMode()`（`:1556-1558`）
只回填 `s_mode`、**绝不 auto-show** ⇒ 登录后不会自己弹面板（D1b 逐字成立）。

### R6 本轮实际改动的文件面（权威清单）

`include/QuickPanelAlpha.h`（`kAlphaFactorIdle` 217 + 4 处过期注释）、
`WeaselServer/QuickPanelDialog.h`（`kAlphaPanelTop` / `kAlphaPanelBot` = 255 + 3 处注释）、
`WeaselServer/QuickPanelDialog.cpp`（`LoadPersistedAlwaysShowModeFrom` 返回 true + 4 处注释）、
`WeaselServer/resource.h`（1 处注释）、`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`
（T13b / T16a / T16e 极性 + T12d / T12e 期望值 + 消息文案）。
**未改** `WeaselTSF/*`、`WeaselServer.rc`、`install.nsi`、`librime/`；**未新增** IPC 命令。