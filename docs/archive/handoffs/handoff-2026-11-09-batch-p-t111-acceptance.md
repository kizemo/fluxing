# 批次 P · spec 045 T111 真机验收闭环（run11 = 14/14 全绿）

> 日期: 2026-11-09 · 分支 `Fluxing`（ahead 19，含本批四笔提交）
> 上游: 批次 O（T111 范围定案 · 方案 A）→ 本批（实现 + 单测 + 构建 + 装机 + 真机验收 + 提交）

## 背景与目标

T111 缺陷：快捷键设置面板把「快捷静态 Alt+comma」「常用短语 Alt+period」两条**应用级**热键写成
`custom:` 动作落进 `key_binder/bindings:` 托管块，而 librime `key_binder.cc` 只认
`send/send_sequence/toggle/set_option/unset_option/select`，未知动作 `LOG(WARNING)` 跳过 ⇒
落盘是污染；应用级热键实际生效机制是 WeaselServer 级 `RegisterHotKey`。

批次 O 定案（方案 A，最小完备）：这两行在快捷键设置界面**只读**——不进捕获态、不显示「重置此行」
右键菜单、落盘永不写入托管块；点击时显示提示 popover 说明「此快捷键由输入法程序管理，在快捷设置栏
切换」。

## 本批完成（全部已提交）

1. **落盘语义**（提交① `bd3d941`）：`ShortcutSettings.cpp` 保存路径跳过 read_only 行，并逐条剔除
   已落进基线的污染旧行（`IsAppLevelAction` 标记）。
2. **UI 只读交互 + popover 修复 + 单测**（提交②，t111-a/b/c/d 四个用例随本笔入库，d 测 popover 定位，
   依赖 UI-only 符号故无法拆进①）：只读行点击 → 提示 popover（`kHintHideMs=3000` 自隐）；右键早退无菜单
   （read_only 行 `return 1` 路径）；修复真机缺陷：只读行 `SetPopoverHint` 因 popover 未创建而静默失效
   ——提取 `EnsurePopover`，单测 + 真机双证。单测全量 **PASS=186 FAIL=0**（`tmp/t111-test-run3.log`）。
3. **installer 脚本**（提交③，以 diff 实况为准）：`silent-install.ps1` 改为按版本号从
   `output\archives` 自动解析最新安装包（替换原硬编码 1.1.6）——**该文件不含任何「瞬态锁重试」代码**，
   勿再沿用旧表述；`acceptance-check.ps1` 的 C1 判据改为比对装机 `default.yaml` 与仓库源
   `output/data/default.yaml` 的 size+sha256。
4. **构建 / 装机坐实**：全量构建 + L0 门禁 14/14 + 打包 1.2.1 + 静默安装；装机
   `D:\Program Files\fluxing\weasel\WeaselServer.exe` = 2434048B
   sha256 `B7D04408650A71FC81D2AD45C15A44739C43FDF5E841184FE98D41ADF5A5F7DA` = 构建产物逐字节一致。
5. **真机验收 run11 = 14/14 全绿**（`tmp/acceptance-t111/probe-t111-v3-run11.log`）：
   - C2-hint / C3-hint 4ms 出现、nearRow=True；C2c / C3c autohide 2631/2617ms；
   - C2b / C7 `default.custom.yaml` sha256 全程不变（`50A46CC7…`）⇒「永不写托管块」真机证据；
   - C4-capture route=real 正常行进捕获 + C4b Esc 3ms；C5 popover 生命周期后复用；
   - **C6a 正控制真绿**（正常行右键 5ms 弹 `#32768`，menu=0x4E06FC）⇒ **C6b 只读行右键
     `NO #32768 window exists anywhere` 升格为有效验证**（memory #41 判据满足）。

## 已验证 vs 推断 vs 未验证

- **已验证**（证据在盘）：上述 run11 全部 14 判据；单测 186；装机二进制逐字节；红→绿单测流程
  （`tmp/t111-red.log` 7 条判据命中红后才恢复 A3 守卫）。
- **推断**（有代码路径 + 单测佐证，真机未单独测）：托管块对 read_only 行的跳过逻辑在「面板有自定义行」
  混合场景下的组合行为——由单测覆盖，真机 run11 未构造该组合。
- **未验证 / 永久缺席**：文案类判据（跨位数 `WM_GETTEXT` / `LVM_GETITEMTEXT` 对 listview 在本探针形态
  作废）⇒ hint 文本内容未做像素/文案断言，仅断言 popover 存在 + 位置 + 自隐时序。

## 本批探针关键坑（run8–run11 沉淀，已在探针注释留档）

- **`EnumWindows` 原生回调写不回函数局部变量**：run1–run10 期间 `Menu-Visible` 用函数局部 `$found`
  跨回调传状态 ⇒ 确定性返回 0 句柄，C6a 三 run 恒红被误归因「前台问题」；改成 `$script:` 作用域
  （与取证函数 `Dump-All-Menus` 同形态）当场 5ms 命中。**教训：原生回调闭包一律走显式 `$script:`。**
- **判据「0 命中」先自证可见性**（memory #41 再现）：本轮靠无条件全量枚举（pid/vis/rect 全打印）一次
  定位判据 bug，而不是继续猜时序。
- 轮询前 300ms 预睡会错过瞬态窗口（#35 秒杀场景）⇒ 缩到 30ms，由 10ms 轮询覆盖。
- `TrackPopupMenu(TPM_RETURNCMD)` 阻塞对话框线程：命中后必须立刻 `Dismiss-Menu`
  （PostMessage ESC 配方，#35），run11 顺序已验证安全。

## 勘误（推翻先前错误结论，勿再引用）

- 「C6a 曾经 PASS」不成立——run1 日志实证从未通过；批次 N 时代以 C6b PASS 佐证「只读行无菜单」同样无效。
- 「前台不对称」假设被 run10 取证证伪：`setFgOk=True fgPid=dlgPid=315996` 时菜单照样弹、判据照样瞎。
- 产品侧**无任何回归**：C6a 红纯粹是探针判据 bug。

## 下阶段（新会话入口）

1. 先读本文件 + `F:/soft/00selfmade/rime_claude/.specify/specs/045-shortcut-settings/tasks.md` +
   `F:/soft/00selfmade/rime_claude/CLAUDE.md` + `F:/soft/00selfmade/rime_claude/AGENTS.md`。
2. spec 045 剩余：T104（「立即验证」按钮，须先过 P2/AP-045-B 审查）、T105（自定义快捷键子 modal）、
   P2 界面补全 T201–T203。
3. 并行线 spec 100（云端账号与多端同步）见 CLAUDE.md 指针。
4. 真机探针资产可复用：`F:/soft/00selfmade/rime_claude/tmp/acceptance-t111/probe-t111-v3.ps1`
   （原生 Win32 配方、纯 ASCII、run 存档 run1–run11 同目录）。
