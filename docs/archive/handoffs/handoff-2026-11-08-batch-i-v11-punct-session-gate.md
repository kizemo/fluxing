# 批次 I · 图标 v11 + 半角标点修复(ADR 0043) + 开机卡顿修复(ADR 0042)

- 日期:2026-11-08
- 分支:`Fluxing`
- 上游批次:批次 H2(快捷面板图标 v10 + 标点语义纠正,ADR 0041)· 见
  [`handoff-2026-11-08-quickpanel-v10-punct-semantics.md`](./handoff-2026-11-08-quickpanel-v10-punct-semantics.md)
- 本批新增 ADR:[`docs/adr/0042-session-gate-precedes-server-shutdown.md`](../../adr/0042-session-gate-precedes-server-shutdown.md) ·
  [`docs/adr/0043-punct-language-conversion-at-commit-boundary.md`](../../adr/0043-punct-language-conversion-at-commit-boundary.md)

---

## 1. 用户本轮 5 条检验项 + 1 个缺陷的处置结论

| # | 用户诉求 | 结论 | 证据 |
|---|---|---|---|
| 1 | 面板「中英文标点」图标:句号可以、逗号形态不好且中英文无法区分 | **已修(v11)**,待人眼终确认 | 真机像素(服务端运行时位图落盘):中文态 **ink=67 / bbox 23×26**、英文态 **ink=41 / bbox 18×20** ⇒ 形态 + 尺寸双重可区分;日志 `tmp/p4v5c.log` |
| 2 | 中文态裸 Shift 切英文,「中/英」标志跟随 | **PASS** | `tmp/batch-i2-P2-slot1-cn.png`(ink=198,bbox 20×24) / `...-en.png`(ink=274,bbox 22×24);`P2 slot1 DIFF=363` |
| 3 | 手点「中英文标点」切半角 → 中文态打 `,` 变半角;点回变 `，` | **原为真实缺陷 → 已修(ADR 0043) → 真机往返 PASS** | `tmp/p4v5c.log`:中文态 `，`(U+FF0C) → 点一次 `,`(U+002C) → 点回 `，`(U+FF0C);槽 2 图标同步翻转 ink 67↔41 |
| 4 | 中文标点态下数字与 `@#%` 恒半角 | **PASS** | `tmp/batch-i3.log`:`123{#}{%}{@}` → `123#%@`,码位全 ASCII |
| 5 | 点「中英文」/「中英文标点」按钮后不点宿主窗口直接打字能上屏 | **PASS**(焦点语义) | `GetGUIThreadInfo`:点按钮前后前台窗口不变(`fg=0x3132C`)、`hwndFocus` 恒为宿主 EDIT(`0x12131A`)、caret 也在该 EDIT ⇒ 键盘焦点没丢;探针 v3 的空文本是 `SendKeys` 自身问题,v4 改 `keybd_event` 后确实把键打进了宿主框 |
| 6 | 开机第一次切到火流猩,延时较久才能用(设置栏/短语不出、中文打不出) | **是 bug,已修(ADR 0042)** | 根因:门禁排在「杀旧实例」之后 ⇒ 远程会话实例先杀掉 console 会话健康服务端、自己被拒退场 ⇒ 服务真空。日志 `startup_refused allowed=0 reason=not_console_session current_session_id=2 console_session_id=1` 紧跟 `start_returned_false exit=-1` |

---

## 2. 本批改动清单(全部尚未提交,见 §6)

### 2.1 图标 v11 —— 逗号可区分(单一事实源 `include/FluxingPanelFwIcon.h`)

- 根因:v10 两态都用 Microsoft YaHei,`U+FF0C「，」` 与 `U+002C「,」` 是**同一个楔形**,48px 下墨迹盒同为 7×13 ⇒ 「无法区分」成立。
- v11 定稿:**句号图元不动**;中文态 **SimSun** `U+FF0C`(目标墨迹 8×13,origin=(6,11));英文态 **Times New Roman** `U+002C`(目标墨迹 6×9,origin=(2,3)),两态下沿同为 28。
- 落地文件:`include/FluxingPanelFwIcon.h`(新增 `kCommaInkWidthCn/kCommaInkHeightCn/kCommaInkWidthEn/kCommaInkHeightEn/kCommaFaceCn/kCommaFaceEn/kCommaBottom`,`Icon` 增 `commaFace`)、
  `WeaselServer/QuickPanelDialog.cpp`(`CreateCommaFont` / `DrawInkPlacedChar` 传 `ic.commaFace`)、
  `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`(T33k 按态断言 8×13 / 6×9;新增 **T33r** 两态逗号字体必须不同)。
- 单测:`tmp/qpanel-tests-v11.log` ⇒ `255/255 assertions passed`、`TEST_DIALOG_EXIT=0`、`TestQuickPanelRefactor 40/40`。

### 2.2 P4 修复 —— 半角标点 ⇒ 英文标点(ADR 0043)

- 引擎侧事实(`librime/src/rime/gear/punctuator.cc`):标点表**只**由 `full_shape` 选项选(`:23-30`);`ascii_punct` 只让处理器短路(`:102`);翻译路径(`:329`)完全不看它;本 fork 两张表(`output/data/default.yaml:127`/`:161`)**都**映射中文标点 ⇒ 引擎永远吐中文标点。
- 不用 `full_shape` 选表的原因:`shape.cc:16-36` 的 `ShapeFormatter` 会把数字/`@#%` 全角化;`PunctTranslator` 自带 `formatter_`(`punctuator.h:88`)在 `punct_number` 分支(`:318-323`)生效 ⇒ 半角态下「3.14」会变「3．14」。
- 修法:**不改 librime、不碰 full_shape**,由 fork 在**提交边界**做标点语言转换 —— `RimeWithWeasel.cpp` `_Respond()`(全仓**唯一**一处 `get_commit`)在引擎 `ascii_punct` 为真时,把「纯标点」提交里的中文标点换成 ASCII。
- 新增 `include/FluxingPunctText.h`(纯函数,生产与单测同源 R7):`IsPunctuationOnly` / `ToAsciiPunctuation` / `ApplyAsciiPunctuation`。
- 单测:`test/TestWeaselIPC/TestWeaselIPC.cpp` §1c(12 条断言 + 反向自检)。

### 2.3 开机卡顿修复(ADR 0042)

- **D1 门禁前置**:`WeaselServer/WeaselServer.cpp` 在「杀旧实例」**之前**做 `EvaluateCurrent()` 门禁,拒时写 `startup_refused ... detail=pre_kill_gate` 并 `return -1`;新增纯函数 `fluxing::session::MayShutdownExisting(verdict, quit)`(`/quit` 运维语义仍放行)。
- **D2 前端补拉**:`WeaselTSF/WeaselTSF.cpp` 阈值 `retry >= 6` → **连续失败** `>= 2`(连上即清零),进程计数改**按会话**(`ProcessIdToSessionId` vs `CurrentSessionId`),留痕新增 `server_process_count_all=` / `server_session_id=`。
- **D3 诊断日志**:`include/FluxingDiagLog.h` 新增 `DiagTimestamp()`,行形状 `[YYYY-MM-DD HH:MM:SS.mmm][tag] message`。
- 单测:`test/TestWeaselIPC/TestWeaselIPC.cpp` §1b(`MayShutdownExisting` 真值表 + 反向自检)与 §5(时间戳前缀长度 25 + 分隔符断言)。

---

## 3. 构建 / 装机

- 构建脚本:`tmp/build-batch-i.cmd`(RimeWithWeasel x64 → TSF x64 → RimeWithWeasel Win32 → TSF Win32 → `weasel.sln` Win32 → makensis),日志 `tmp/build-batch-i2.log` ⇒ `ALL_STEPS_OK`、`NSIS_RC=0`。
- 产物:`output/Win32/WeaselServer.exe`(11:16)、`output/archives/fluxing-1.2.1-installer.exe`(11:18)。
- **注意(本轮新发现)**:TSF 两个 DLL(`output/weaselx64.dll`、`output/weasel.dll`)**没有**在 `AdditionalDependencies` 里链 `RimeWithWeasel.lib` ⇒ **引擎跑在服务端**,客户端 DLL 不含引擎代码 ⇒ TSF DLL 的 mtime 不变是**正常**的,不是「链接了陈旧 lib」。P4 修复在 `WeaselServer.exe` 里。
- 静默重装:`tmp/reinstall3-batch-e.ps1`(`INSTALL_EXIT=0`,四二进制 src/dst SHA256+size+mtime 全 `MATCH=True`)。只装 `D:\Program Files\fluxing`。
- 装机路径校正:`D:\Program Files\fluxing\weasel\WeaselServer.exe`(不是 `D:\Program Files\fluxing\WeaselServer.exe`);`engine-nested-fluxing-bak-20261103\weasel\` 与 `weasel\fluxing\weasel\` 是**旧备份**,勿误启动。

---

## 4. 真机验收

- 探针:`tmp/probe-batch-i3.ps1`(v4,含 P4+P6)、`tmp/probe-p4-v5.ps1`(P4 往返专用诊断),日志 `tmp/batch-i3.log` / `tmp/p4v5c.log`。
- 面板像素取证一律走**服务端运行时位图落盘**(`FLUXING_QP_DIAG_DUMP=1` ⇒ `qp-dump.bmp` / `qp-logo.bmp` / `qp-state.txt`),**不抓屏**(屏幕抓面板会被遮挡,本轮已实证一次)。落盘文件已从仓库根目录移入 `tmp/diag-residue/`。
- 面板唤出走**全局热键 `Alt+,`**(手工拉起的服务端没有托盘窗口,`WM_COMMAND 40018` 失效)。
- **P4 真机复验结果(本轮末尾取得)**:见 §7「本轮末尾状态」。

---

## 5. 未验证 / 已知缺口

1. **开机卡顿缺现场 A/B**:本机无法复现「RDP/远程会话拉起实例杀 console 服务端」的完整路径(历史日志无毫秒时间戳);已从根上断掉杀伤路径,但复验仍缺。
2. **图标 v11 需人眼终确认**:宋体逗号在浅蓝面板上的观感与重心(像素已取证,形态与尺寸与英文态可区分)。
3. **`t9.schema.yaml` 没有 `switches:` 段** ⇒ `ascii_punct` 未声明 ⇒ `set_option("ascii_punct")` 静默失败 ⇒ **t9 方案下 P4 机制不生效**(既有缺口,批次 H 已在 `radical_pinyin.schema.yaml:15-21` 记过同类坑)。
4. **ADR 0042 后续风险**:`/quit` 运维语义仍放行「杀旧实例」;「杀旧实例改成只杀同会话」本期未做。
5. **面板 tooltip 窗口会浮在槽位上方**:探针点击时 `WindowFromPoint` 命中 `FluxingQuickPanel_Tooltip` ⇒ 点击被吞。**探针已规避**(先把鼠标移出面板再点),但**用户手点时是否会被 tooltip 遮挡仍需人眼确认**;若复现,需让 tooltip 避开面板矩形或置 `WS_EX_TRANSPARENT`。

---

## 6. 提交状态

- 工作区含**批次 H2 + 批次 I** 两批未提交改动,**同一文件内两批改动交织**(如 `WeaselServer/QuickPanelDialog.cpp`、`include/FluxingPanelFwIcon.h`),**无法按文件拆分**,需按模块/职责拆成多个 Conventional Commit(每个文件只入一个 commit,禁 `git add .`)。
- 提交前必跑:`git status -sb` + `git diff --stat`;排除 `weasel.props` / `env.bat` / `*.obj` / `*.log` / `tmp/`;` m librime` 是假脏。

---

## 7. 本轮末尾状态

- **P4 往返复验 PASS**(`tmp/p4v5c.log`,服务端 pid 684476,二进制 = 装机版 11:16 构建):
  - state0:槽 2 ink=67 / bbox 23×26(中文态) → 中文态打 `,` 得 **U+FF0C**(全角逗号);
  - 点一次「中英文标点」⇒ state1:ink=41 / bbox 18×20(英文态)→ 同一输入得 **U+002C**(半角逗号);
  - 再点一次 ⇒ state2:ink=66 / bbox 23×26(中文态)→ **U+FF0C**。
  - ⇒ 面板图标与实际输出**同向翻转**,往返闭合。
- **前两轮「点回不生效」是探针假象**,已定位三条探针缺陷:① 面板枚举用 `FluxingQuickPanel*` 前缀 ⇒ 把 `FluxingQuickPanel_Tooltip` 当成主面板,第二次点击落在 tooltip 上;② 点击前鼠标就悬停在按钮上 ⇒ tooltip 弹出遮挡槽位,`WindowFromPoint` 命中 tooltip;③ `Find-Panel` 回调里给局部变量赋值未落到外层作用域 ⇒ 恒返回「找不到面板」。修正后立即 PASS。
- **`TestWeaselIPC` 跑通**:`TEST_EXIT=0`(此前 `-2 failed to connect to server` 是重装期间服务端已停,非断言失败)。
- **运行态已恢复干净**:手工实例(带 `FLUXING_QP_DIAG_DUMP`)已停,重新拉起干净服务端(pid 753164,session 1);仓库根目录的 `qp-dump.bmp` / `qp-logo.bmp` / `qp-state.txt` 已移入 `tmp/diag-residue/`。
- **仍未做**:开机卡顿的现场 A/B 复验、图标 v11 人眼终确认、`t9.schema.yaml` 补 `switches:`。

---

## 8. 下一阶段建议

1. 给 `t9.schema.yaml` 补 `switches:` 段(`ascii_punct`),让 P4 在九键方案下也生效(改 yaml 前先读 `Rime-设置说明书-重点快捷键.md` §5–§7/§9/§10)。
2. 复验开机卡顿(需远程会话场景或构造 console/非 console 双会话);ADR 0042 后续:「杀旧实例只杀同会话」。
3. 图标 v11 人眼终确认;若观感不足,下一版只调 `include/FluxingPanelFwIcon.h` 的逗号常量(单一事实源)。
4. 人眼确认 tooltip 是否会遮挡面板槽位(见 §5.5),必要时修 tooltip 定位/透明属性。
