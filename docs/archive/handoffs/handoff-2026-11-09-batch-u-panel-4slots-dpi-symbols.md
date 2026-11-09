# 批次 T 收尾 · 批次 U —— 快捷面板 4 槽化(P1) + 键帽符号/笔宽严格同比(P0)

- 日期: 2026-11-09(构建与装机落在 2026-11-10 00:32)
- 上游交接: [`handoff-2026-11-09-batch-t-sogou-dpi.md`](handoff-2026-11-09-batch-t-sogou-dpi.md) §8(P0 键帽符号 + 线宽 DPI 收尾、P1 摘两个键帽)
- 分支: `Fluxing`(推 `kizemo/Fluxing`,未推 origin)
- ADR: [`docs/adr/0050-quickpanel-4-slots-and-dpi-proportional-symbols.md`](../../adr/0050-quickpanel-4-slots-and-dpi-proportional-symbols.md)

---

## 1. 目标

1. **P0**:键帽符号与线宽按 DPI **严格同比**缩放(150% ⇒ 符号 1.5×、线宽不再相对粗 33%)。
2. **P1**:摘掉「快捷键设置」与「用户登录(关于·账号说明)」两个键帽,6 槽 → 4 槽。

## 2. 根因(已坐实,非「没上机」)

- **装机二进制确实含上一轮改动**:`output\Win32\WeaselServer.exe` 与
  `D:\Program Files\fluxing\weasel\WeaselServer.exe` SHA256 相同
  (`47B00A2B…6E59`);时间戳链 `QuickPanelDialog.cpp (21:47:07) < .obj (22:03:23) < .exe (22:03:34)`
  ⇒ 83d1820 的图标缩放**在机**。
- **P0 真病灶 = 两处取整**:
  1. **笔宽就近舍入**:raw1 的空心环在 150%(系数 1.5023)⇒ `round(1.5)=2px`
     ⇒ 笔宽 2.0× 而几何 1.5× ⇒ **相对粗 33%**。
  2. **逗号字号二次取整**:目标墨迹高取自**已取整**的缩放墨迹盒(13→19)⇒
     `48×19/13 = 70px`(理想 72)⇒ 符号只有 **1.46×**。
- **附带**:tooltip 字体 `s_hFontTooltip` 只在首次显示时创建,DPI 变化后不重建。

## 3. 本轮改动

| 文件 | 改动 |
|---|---|
| `include/FluxingPanelIconGeom.h` | **新建**:符号几何/笔宽缩放口径的单一真相源(`ClampFactor` / `ScaleCoord` 就近舍入 / `ScaleStroke` **向下取整** / 5 个图标 raw bbox / `ScaledInkBox` / `ScaledInkSize`) |
| `include/FluxingPanelFwIcon.h` | `Icon::commaFontPx`(目标字号);`Build()` 置种子 48;`ScaleIcon()` 按系数缩放;本地 `ScaleCoord` 改为转发共享实现(消第二份真相源) |
| `WeaselServer/QuickPanelDialog.h` | `kPanelW 215→155`;`kButtonCount 6→4`;`kVisualToLogical {0,4,5,1,2,3}→{0,4,5,1}` |
| `WeaselServer/QuickPanelDialog.cpp` | 6 处笔宽改 `ScaleStroke`;`DrawInkPlacedChar(..., int fontPx=0)`,自校正仅在 `fontPx<=0` 时执行;调用点传 `ic.commaFontPx`;`ApplyPhysicalLayout` 内重建 tooltip 字体 |
| `test/TestQuickPanelDialog` | T25 尺寸断言改 155/233/310;槽位与 tooltip 断言改 4 槽;**新增 T31**(三档 DPI 符号同比/笔宽/字号断言) |
| `test/TestQuickPanelRefactor` | `kButtonCount == 4`;T12 改为「视觉 4/5 已摘除 ⇒ HitTest -1、s_onShortcut 不触发」 |

**口径决定(唯一理由,改它必须同步改单测)**:

- 笔宽 **向下取整(floor),下限 1px** —— GDI 笔宽只能是整数,1.5px 不可表达;用户明确抱怨偏粗 ⇒ 取细的一侧。raw≥2 的线在 100/150/200% 下正好 2/3/4px(严格同比)。
- 坐标口径**不动**(就近舍入 .5 向上,与 `panel_dpi::ScaleByFactor` 逐位一致)。
- 逗号目标字号**直接按系数给**(48 → 72 → 96),字体自校正退化为「目标机缺字体」兜底。

**P1 只摘面板键帽,不动功能入口**:托盘右键「快捷键设置」(`ID_WEASELTRAY_SHORTCUT_SETTINGS` → `ShortcutSettings::Show()`)与 Ctrl+Shift+K 热键**保留**;「关于/账号说明」页模块保留但面板侧不再有入口。

## 4. 验证状态

### 已验证(本机实测)

| 项 | 结果 |
|---|---|
| `TestQuickPanelDialog` | **269/269 assertions passed**(EXIT=0) |
| `TestQuickPanelRefactor` | **41/41 assertions passed**(EXIT=0) |
| 构建 | `tmp/build-installer.cmd` → `xbuild.bat weasel installer` ⇒ **XBUILD_EXIT=0**,产物 `output\archives\fluxing-1.2.1-installer.exe` |
| 装机 | 静默 `/S /D=D:\Program Files\fluxing` ⇒ **INSTALLER_EXIT=0**;装机 `WeaselServer.exe` SHA256 `A5F2424F88EE1174894007BE7FE033B44FE70AE27A017FD135D0AFEF8C94A7DC` = 构建产物(2468352 B @2026-11-10 00:32)⇒ **改动已上机** |
| 服务端 | 已重启,pid 157804 |
| 装机验收 C2/C3/C4 | PASS(无 stage2 标记;26 个版本化 shim;TIP 指向版本化 shim;HKLM Run 项存在) |
| L0 静态门禁 | `scripts/gate/L0-static.ps1 -Mode source` ⇒ **checks=14 pass=14 fail=0** |

### 未验证 / 待用户

1. **P0 真机 DPI 复验(必须用户人工做)**:改一次分辨率(建议 3840×2160@150% → 2560×1440@100% → 3840×2160@200%),
   打开快捷面板检查:① logo 完整;② 键帽符号与键帽**同比**(不再是「小一号」);③ 线条不再偏粗。
   - **自动化抓不到面板**:`FluxingQuickPanel_v3` 只在 IME 激活瞬间存在,跨进程 `FindWindow` 返回 0(见上游交接 §7.8 与记忆条目 #43)。
2. **P1 人眼确认**:面板只剩 4 个键帽,视觉顺序 = 切换输入法 / 中英文 / 全半角 / 用户短语;托盘右键「快捷键设置」仍能打开界面。

### 附带发现(与本轮改动无关,但会让装机验收报红)

- `tmp/acceptance-check.cmd` 的 **C1 判据已过期**:硬编码 `data\default.yaml` = 18217 B / sha `917509f6f2460230`,
  而仓库与装机的实际文件是 **18448 B / sha `05CC2DDB21A1ABF35DF0D98294C946F8B528D6B1C31596FFEFD2D1B2625B22E0`**(两者**逐字节相同**)。
  ⇒ C1 FAIL 是**判据过期**,不是本轮回退。**下一轮应把 C1 判据改为「与 `output\data\default.yaml` 比对」**(避免下次再误判)。

## 5. 未做 / 挂起

- 上游交接 §8.3 继续挂起:`double_pinyin_mspy.schema.yaml` 微软/搜狗键位标注不一致、`m_session_schema_ids` 未回写、按窗口独立选方案(需推翻 ADR 0048)、ADR 0047 空缺。
- x64 TSF(`weaselx64.dll`)本轮**未重建**:本轮改动全在 `WeaselServer`(x86)与共享头,x64 TSF 不引用它们 ⇒ 无需重建;若后续有人怀疑双拼,仍按 L128 顺序重建。

## 6. 本轮工具坑

- **`edit` 一次只能传一组 `old_string`/`new_string`**(JSON 重复键会被静默丢弃)——本轮改用 `.ps1` 补丁脚本统一落盘。
- **PowerShell 数组字面量会把嵌套数组展平**:`$pairs = @( @('a','b') )` 只有 1 个元素时会**拆成两个字符串** ⇒ `$p[0]` 变成 `[System.Char]`(报「不包含名为 Substring 的方法」)。必须写 `$pairs = ,@('a','b')`(一元逗号)。
- **补丁脚本的「幂等跳过」会把 old ⊂ new 的替换重复应用**(T31 插入块与 T20m 块的 old 是 new 的前缀)⇒ 补丁脚本应**只跑一次**,或在应用前 `git checkout` 目标文件。
- `xbuild.bat weasel installer` **只编 x86**(L210)——面板改动落在 x86 侧,不需要 x64 配方。
- 装机验收 C1 的过期判据(见 §4)会让人误判「装机坏了」。

## 7. 下一轮起点

1. 读本文件 §3/§4,确认用户对 P0(真机改分辨率)与 P1(4 键帽 + 托盘入口)的复验结论。
2. 若 P0 通过 ⇒ 更新 `CLAUDE.md`「本批交接指针」+ `CHANGELOG.md`,提交本轮改动(见 §8 提交边界)。
3. 若 P0 不通过 ⇒ 只查 `include/FluxingPanelIconGeom.h` 的三条口径与 `ScaleStroke` 取整方向,**不要再改几何系数**。
4. 顺手修 `tmp/acceptance-check.cmd` 的 C1 判据(改为与仓库 `output\data\default.yaml` 比对)。

## 8. 提交边界(本轮)

**应提交**:`include/FluxingPanelIconGeom.h`、`include/FluxingPanelFwIcon.h`、
`WeaselServer/QuickPanelDialog.{h,cpp}`、`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`、
`test/TestQuickPanelRefactor/TestQuickPanelRefactor.cpp`、`docs/adr/0050-*.md`、
本 handoff、`CHANGELOG.md`、`CLAUDE.md`。

**不得提交**(并发会话的改动):`RimeWithWeasel/RimeWithWeasel.cpp`、子模块 `librime`、`weasel.props`、`env.bat`、`*.obj`。
