# 批次 BA · §6 第 1 项闭环：换钥提示折行宽度**实测取到真值**（A-1/A-2/A-3 PASS）+ 新发现 DPI 余量为 0

> 接力自 [`handoff-2026-11-20-batch-az-install-1-3-6.md`](handoff-2026-11-20-batch-az-install-1-3-6.md) §6 未闭环清单**第 1 项**。
> 判据表：[`docs/release/restart-checklist-1.3.6.md`](../../release/restart-checklist-1.3.6.md) §1（A-1–A-5）。

---

## 1. 本批性质

**纯取证批次**。未改任何产品代码、未改 `librime/`、未改 IPC 协议、未新增依赖、**未提交**。
磁盘新增只有：探针与证据 `reports/fluxing-ba-a1-note-wrap/`（7 个文件）+ 本 handoff + 接力 prompt +
`CLAUDE.md` 指针一行。**装机版仍是 1.3.6.0，未重装、未重启电脑、未杀任何进程**（守住 L109）。

## 2. 结论（一句话）

**A-1 / A-2 / A-3 = PASS**：提示行 37 个全角字符在 150% 缩放下确实**折成两行**，两行**都完整画在 `h=36` 的框里**，
但**余量恰好 0 px** —— AZ 的「需 ≥2 行、疑被裁」方向对、数值错（AZ 猜行高 22 ⇒ 需 44–48 px；实测行高 **18 px** ⇒ 需 **36 px**）。
代价是量出一条**新缺陷 C：`h=36` 只在 ≤150% DPI 成立，任何 >150% 的机器必裁**（见 §4）。

## 3. 实测数据（全部来自 `report.json`，非推断）

真机环境：`WeaselServer.exe` **pid=173132 / start=11-20 16:42:20 / FileVersion=1.3.6.0**（AZ 装机的那个实例，未动）；
桌面守卫 `thread desktop = Default` PASS；主屏 2560×1440；账号窗 **`GetDpiForWindow = 144`（150%）**。

开窗方式沿用 AX 已实测姿势：向**服务自身非托盘顶层窗** post `WM_COMMAND(40024)`
（`hwnd=4391456 class='ATL:004A8DE0'`；发 `TrayIconClass` 无效）。窗 rect `76,76,578,422`，客户区 **`480×290`**（逐字等于 `AccountClientSize()`）。

控件 `id=1035`（`kIdKeyRotationNote`）真机属性：

| 项 | 实测值 | 对照 |
| --- | --- | --- |
| 窗口类 | `STATIC`，`vis=True` | 坐实「真子窗、非自绘」 |
| 尺寸 | **`456 × 36`** | **逐字等于 `CloudAccountUi.h:159` 字面量 ⇒ 布局未随 DPI 缩放** |
| 文本 | `len=37`，见 `note-text.txt` | 与清单 A-1 期望**逐字相同** |
| 字体 | `WM_GETFONT` 有句柄，`GetTextFaceW` = **宋体**，dpi=144 | `DefaultUiFont()` = `DEFAULT_GUI_FONT` |

`DT_CALCRECT`（用**控件自身字体**，不是猜的字号）：

| 测量 | 结果 | 读法 |
| --- | --- | --- |
| 单行 `DT_SINGLELINE` | **w=666 px / h=18 px** | 666 > 456 ⇒ **必须折行**；行高 **18**（AZ 猜 22 ⇒ 错） |
| 折进真实框 `MeasureInBox(456)` | **w=450 px / h=36 px**，行数 **2** | 最宽行 450 ≤ 456；两行 = 2×18 = **36** ⇒ 刚好填满 |
| 判据 | `deficit = 36 − 36 = 0` | `A1_NOT_CLIPPED`，**HEADROOM 0 px** |

像素剖面（`PrintWindow(hwnd,hdc,2)` 裁 456×36 后逐行扫描墨迹）：

- 背景 `240,240,240`（= 对话框底色）；墨迹带 **`0–16`** 与 **`18–34`** ⇒ **bandCount = 2**（两行都画出来了）
- **`lastBandTouchesBottom = False`**（第 35 行无墨）⇒ 末行下方仍有空间，**没有被截断**
- 两条**互相独立**的路径（CALCRECT 算术 / 像素带数与带位置）同时给出「两行、都完整」⇒ 判据有牙

对照与收尾：`id=1022`（24 字符 / 456×18 的单行说明行）实测宽度按 18 px/字推得 432 < 456 ⇒ 与头文件
`:156-158` 自述的「23 个中文字在 456 px 内不折行」**同向**，模型不失真。探针收尾 `WM_CLOSE` 后 `IsWindow=False`（未杀进程）。

**布局不重叠的算术证据**（同一次枚举）：1035 占 `y 277..313` → `id=1006`（状态行）`319..359` → `id=1010`（关闭钮）`371..399` ≤ 窗底 422。
⇒ 提示行与上下控件各有 **6 px** 间隙，**A-2 的「不重叠」+ A-3 的「关闭按钮在提示行下方且完整可见」= PASS**。
A-3 的「可点」沿用 AX 实测（账号窗 3 轮开/关，关闭后 `IsWindow=FALSE`），本批未做合成点击。

## 4. 新发现：缺陷 C（DPI 余量为 0，>150% 必裁）

本批最大产出不是「PASS」，而是把 AZ 的「保守取值」这句模糊话换成了一条**可预测失效边界**：

- 布局是**物理像素字面量**（§3 已实测坐实 456×36 未缩放），而 `DEFAULT_GUI_FONT` **随 DPI 放大**
  ⇒ 需要的高度 ∝ DPI，给的高度恒定。
- 144 dpi：行高 18 ⇒ 两行需 36 = 给 36 ⇒ **余量 0 px**（本机侥幸刚好）。
- **>144 dpi 一律裁**：168 dpi（175%）行高 21 ⇒ 两行需 **42 > 36** ⇒ 末行被截；192 dpi（200%）行高 24 ⇒ 需 **48 > 36**。
  120 dpi（125%）需 30、96 dpi（100%）单行即可 ⇒ 均安全。
- 口径：**144 dpi 这条边界 = 实测**；168/192 dpi 的数值 = **由实测行高线性推得的推断，未在那两种 DPI 真机验过**。

修法候选（**本批未动代码**）：把 1035 的高度从字面量 36 改成按 DPI 计算
（`2 * lineHeight + 余量`，或直接 `MulDiv(36, dpi, 96)` 并把窗高同步放大），代价是要重排 `kIdStatus` / `kIdClose` 的 y。
是否进下一版（1.3.7）待用户拍板。

## 5. 判据口径（本批固化，后续别再犯）

1. **`h=36` 够不够**不许按「8pt 猜行高」，只认**控件自身 `WM_GETFONT` + `DT_CALCRECT` + 像素墨迹带数**三向一致。
   字体句柄、face 名、`GetDpiForWindow` 要一起打印，否则数值不可复现。
2. 判「裁没裁」的**像素判据**是「**末带是否贴底边**」，不是「有没有字看得见」——两行都在框内时末带 `18–34`，
   第 35 行留空；若被裁，末带会顶到 `35` 且带数只剩 1。
3. **清单编号 ≠ 探针 verdict 名**。探针自报 `A1_NOT_CLIPPED`，对应的是清单 **A-2（不被裁切）**；
   清单 **A-1（已登录态出现该行且文案逐字正确）** 由 `note-text.txt` 逐字比对判 PASS。别把两件事混着登记。
4. 账号窗类名 `FluxingCloudAccountDialog_v1`；`EnumChildWindows + GetDlgCtrlID` 取控件，**按 id 不按文本**。
   1.3.6 布局里**没有 `id=1032`** ⇒ 拿它当「单行不折」正控制的设想落空，改用 `id=1022` 做同向对照。

## 6. 未闭环

1. **缺陷 C（§4）修法待拍板** —— 唯一新增的产品决策项。
2. **A-4 / A-5 未测**：需要**未登录态**，而登出属用户账号状态变更，探针不自行发起。
   A-4 目前只有代码级证据（`CloudAccountUi.h:70-72` 注释 + `AccountVisibleIds` 按登录态过滤，1035 不在未登录集合）。
3. **175%/200% 缩放真机未验**（§4 的推断部分）—— 需要那两种 DPI 的机器或改缩放后重跑探针。
4. **AZ §6 第 2 项**（B-1/B-2/B-3 孤儿会话回收端到端）—— 本批未触碰，**这是下一批的第 1 项**。
5. **AZ §6 第 3 项**（已运行 TSF 宿主未逐 pid 枚举模块验证）。
6. **AZ §6 第 4 项**（`TestWeaselIPC.exe /verify-client-pid` 需先 `WeaselServer.exe /quit` 抢管道）。
7. 双机 AC 7 条 + restore + 延迟对照 + 缺陷 A 反向判据 G-3/G-5（AW/AX 遗留）。
8. **本批与 AZ 的文档改动均未提交、未 push**（提交/push 需用户授权）。

## 7. 证据路径（全路径）

- 探针（纯 ASCII）`F:\soft\00selfmade\rime_claude\reports\fluxing-ba-a1-note-wrap\probe-a1-note-wrap.ps1`
  ⇒ 复跑：`pwsh -NoProfile -ExecutionPolicy Bypass -File <该路径>`（自包含：找 pid→开窗→枚举 1035→CALCRECT→截帧→剖面→判据→关窗）
- 判据 JSON `…\reports\fluxing-ba-a1-note-wrap\report.json`（含 steps / dpi / face / 三向数值 / 墨迹带）
- 真机文案 `…\reports\fluxing-ba-a1-note-wrap\note-text.txt`（UTF-8，37 字符，逐字等于清单 A-1 期望）
- 提示行裁图 `…\reports\fluxing-ba-a1-note-wrap\note-1035.png`（456×36，A-2 目视件）
- 整窗截图 `…\reports\fluxing-ba-a1-note-wrap\account-dialog.png`（A-3 目视件）
- 真相源代码 `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountUi.h:153-162`（几何）
  · `WeaselServer\Cloud\CloudAccountDialog.cpp:953-957`（文案）`:986-991`（`WinClassFor`→`STATIC`）`:994`（`StyleFor`）`:1118-1125`（`_CreateControls`）
  · `WeaselServer\Cloud\CloudAccountWidgets.h:62-92`（`DefaultUiFont`/`ApplyFont`/`AddControl` = `WS_CHILD|WS_VISIBLE|style`）
- DPI 真相源 `F:\soft\00selfmade\rime_claude\PerMonitorHighDPIAware.manifest:5` + `WeaselServer\WeaselServer.vcxproj:180,265,350,394,438`
- 沿用姿势来源 `F:\soft\00selfmade\rime_claude\scripts\verify\verify-cloud-account-window.ps1`（`Assert-DefaultDesktop:252-265` / `Save-Shot` / `Open-AccountWindow`）

## 8. 本批工具坑（新增，别再踩）

1. **探针里的桌面守卫必须是真的**：初版写了个 `GetThreadDesktopNameChecks` 占位 P/Invoke（**该导出根本不存在**）
   ⇒ `Add-Type` 期不报错、运行时才炸，而且就算不炸也**没有判定作用**（假守卫比无守卫更坏，会让人误以为已过桌面检查）。
   正确写法 = `GetThreadDesktop(GetCurrentThreadId())` + `GetUserObjectInformationW(h, 2 /*UOI_NAME*/, …)` 取名字符串比对 `'Default'`。
2. **`DT_CALCRECT | DT_WORDBREAK` 与 `DT_EDITCONTROL` 两个变体在 456×36 的 RECT 上返回 `0×0`**（本探针未修，已登记）。
   ⇒ 别把「折行高度 = 0」读成「不需要高度」。判据必须走「单行宽 + 逐行贪心装箱」那条路（`MeasureInBox`），
   并**必须**用像素墨迹带数交叉验证。本批的 verdict 只建立在单行 666、装箱 450×36、带数 2 三者上，这三者自洽。
3. **DPI 感知判据要先问「谁画的这个窗」**：账号窗由**已装机的 1.3.6.0 `WeaselServer`** 绘制 ⇒ 这类 UI 取证**不需要重启电脑/重开应用**
   （推翻上一轮摘要里「须用户在场」的前提）；但「已运行的 TSF 宿主吃没吃新 shim」仍是另一回事（见 AZ）。
4. 控件矩形打印的是**屏幕坐标**（1035 = `99,277`），`CloudAccountUi.h` 的字面量是**客户区坐标**（`12,156`）⇒ 比对前要先减窗口偏移，
   别看到数字不同就判「布局漂移」。
