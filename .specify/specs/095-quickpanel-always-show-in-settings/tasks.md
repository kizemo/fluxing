# 095 · Tasks · 「快捷面板常驻」开关收进统一设置面板

## Phase 1 · 取证（已完成）

- [x] T101 读 `WeaselServer/QuickPanelDialog.{h,cpp}` 的常驻态全部读写点
      （`s_mode` / `SetAlwaysShowMode` / `RestoreAlwaysShowMode` /
      `LoadPersistedAlwaysShowMode*` / `WriteQuickPanelState` /
      `ReadAlwaysShowStateLine` / `QuickPanelStatePath`）。
- [x] T102 读 `WeaselServer/ShortcutSettings.{h,cpp}` 的窗口创建 / 几何 /
      绘制 / 消息分发，确认 ID 段（2100–2199）与 2130 空闲。
- [x] T103 确认 `WeaselServerApp.cpp:251` 在**服务器启动时**调用
      `RestoreAlwaysShowMode()` ⇒ 设置面板打开时运行期状态已是真实值。
- [x] T104 读 `test/TestQuickPanelDialog` 的 T12/T13/T16 全部断言，
      确认读盘语义基线（含 T13h「坏文件 ⇒ 关」与 T16a–e 主旧落点优先级）。
- [x] T105 读 `test/TestShortcutSettings.vcxproj`，发现它**从未进入套件**，
      确认 ID 段 / include 路径 / 平台配置可用。

## Phase 2 · 共享头（宪法 R7）

- [x] T201 新建 `include/FluxingAlwaysShow.h`（UTF-8 BOM + LF，header-only）：
      运行期状态、观察者、格式/解析、三态读、读盘策略纯函数、
      写盘回读自证、落点注入、置位入口。
- [x] T202 落点注入点唯一化：`StatePathOverrideRef()`，
      `QuickPanelDialog::SetStatePath` 与 `TestShortcutSettings` 共用。
- [x] T203 自检：header 带 BOM、无 CRLF、文件非空。

## Phase 3 · `QuickPanelDialog` 下沉

- [x] T301 删 `static Mode s_mode;`（头 + 定义），`CurrentMode()` /
      `AlwaysShowMode()` 改读共享头。
- [x] T302 删 `WriteQuickPanelState` / `ReadAlwaysShowStateLine` /
      `QuickPanelStatePath`，落盘与读盘并入共享头。
- [x] T303 `LoadPersistedAlwaysShowMode*` / `SetStatePath` /
      `RestoreAlwaysShowMode` / `SetAlwaysShowMode` 改走共享头。
- [x] T304 新增 `OnAlwaysShowModeChanged(bool)`（alpha 复位 + 重绘），
      文件作用域静态对象在进程启动时注册观察者。
- [x] T305 保留 legacy 落点（`SHGetFolderPathW`）在 `QuickPanelDialog`。

## Phase 4 · `ShortcutSettings` 增量

- [x] T401 窗口标题 / 面板标题改「设置」；副标题改中文。
- [x] T402 新增 `kPrefsH` / `kPrefsY`；`kTableH_phys` 与 `kBtnY_phys`
      两式同步调整。
- [x] T403 `OnCreate` 插入「界面」分组 + 「快捷面板常驻」复选框（ID 2130），
      初始勾选态取 `AlwaysShowChecked()`。
- [x] T404 `OnPaint` 自绘分组标题「界面」+ 分隔线。
- [x] T405 `OnCommand` 处理 `BN_CLICKED` ⇒ `SetAlwaysShowChecked(checked)`。
- [x] T406 `Hide()` 与 `OnDestroy()` 两处复位勾选框句柄。
- [x] T407 新增公开 API `AlwaysShowChecked()` / `SetAlwaysShowChecked(bool)`。

## Phase 5 · 编码事故修复（L162）

- [x] T501 逐字节 `\xNN` 中文 → 真实 UTF-8 字符（脚本化，幂等）。
- [x] T502 手工修正 2 处源字节本身有损的字（`⟳ 恢多默` / `恢多默`）。
- [x] T503 **组级**判别性校验：旧字节转义按 UTF-8 解码 == 新实际文本，
      且校验器自带「篡改意图文本必须报不一致」自测 ⇒ `VERIFY-PASS`。

## Phase 6 · 判别性回归测试

- [x] T601 T-A1 状态行格式与解析（含「不认识 ⇒ nullopt ≠ 缺失」）。
- [x] T602 T-A2 三态读（缺失 / 空文件 / 读到）。
- [x] T603 T-A3 写盘 + 回读自证（内容逐字节断言）。
- [x] T604 T-A4 主 / 旧落点优先级（含「读到但不认识 ⇒ 不回落 legacy」）。
- [x] T605 T-B 生产 API 往返（`AlwaysShowChecked` / `SetAlwaysShowChecked`），
      落点注入到临时目录，绝不写真实用户目录。
- [x] T606 `TestShortcutSettings` 接入套件的 build 列表（:107）与 run 列表（:119）。
- [x] T607 既有 `TestQuickPanelDialog` 断言**零改动**（等价性的机器证据）。

## Phase 7 · 文档与交付

- [x] T701 `docs/adr/0026-quickpanel-always-show-in-settings-panel.md`（MADR 四段式）。
- [x] T702 `docs/adr/README.md` 补 0026 索引行。
- [x] T703 `CHANGELOG.md` `[Unreleased]` 补条目（含编码事故修复）。
- [x] T704 `.specify/memory/lessons-learned.md` 补 L162。
- [x] T705 spec 三件套（spec.md / plan.md / tasks.md）。
- [x] T706 构建（0 错误）+ 全量套件全绿。
- [x] T707 分模块小提交（Conventional Commits + 白名单 scope）。

## Phase 8 · 人工验证（无法自动化，见交付报告）

- [ ] M701 打开「设置」弹窗 ⇒ 标题显示「设置」，工具栏与状态行之间有
      「界面」分组和「快捷面板常驻」复选框。
- [ ] M702 勾选 ⇒ 复选框保持勾选；关闭弹窗；重启 `WeaselServer.exe`
      ⇒ 勾选仍在，面板常驻右下角。
- [ ] M703 取消勾选 ⇒ 重启后面板回到瞬时模式（调出几秒后自动收起）。
- [ ] M704 设置弹窗里的中文全部正常显示（编码事故已修）。
