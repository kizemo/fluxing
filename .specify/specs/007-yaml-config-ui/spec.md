# 007 · 火流猩输入法 v2 · yaml 可视化设置 UI

> 元 spec 004 拆分。把 default.yaml / weasel.yaml / <schema>.schema.yaml / user.db 的配置暴露为 mac 风可视化编辑器。

## 0. 上下文

- 当前 `WeaselDeployer` 有 UIStyleSettingsDialog / SwitcherSettingsDialog / DictManagementDialog 三个旧式对话框。
- 4 类配置文件分散；用户痛点：不知道字段含义。
- v2 改造为左侧导航 + 右侧表单的 mac 风设置 UI（spec 006 的 FluxingPanelHost 承载）。

## 1. 产品视角

### 1.1 目标

让用户不读 yaml 文档、不手动编辑文件即可完成 4 类配置的查看 / 编辑 / 保存。

### 1.2 用户故事

- US3-A [P1]：进入设置 UI → 快捷键页 → 列表显示当前 default.yaml 全部 key_binder 条目 → 点"翻页（下一页）"项的按键录制器 → 按 `/` → 自动解析为 `slash` → 保存 → 立即生效。
- US3-B [P1]：界面外观页拖透明度滑块 50% → 候选窗实时半透明。
- US3-C [P1]：方案管理页拖"朙月拼音"到顶 → 重启输入法后默认方案改变。
- US3-D [P1]：用户词典页搜索"测试" → 显示 3 条 → 删除其中 1 条 → 关闭设置 → 重新打字"ceshi" → 该词条不再出现。

### 1.3 验收

- Given 全新安装 Fluxing v2.0.0，
- When 用户打开设置 UI（托盘面板的"偏好设置"或 `Alt+,` → ⚙ 更多 → 偏好），
- Then 弹出 mac 风独立窗口，左侧导航 + 右侧表单。
- And 快捷键页加载并显示 default.yaml 当前所有 key_binder 条目。
- And 修改任一条 → 保存 → 立即生效（无需重启）。

## 2. Out of scope

- 不重写 default.yaml 解析（复用 librime 内置 yaml-cpp）。
- 不做"导入/导出配置包"（P2 spec 010）。
- 暗色主题：详细规范见 spec 004 §9；本 spec 实施时引用之。
- 不实现 YamlRoundTrip 之外的编辑器（保留注释 + key 顺序由 YamlRoundTrip 单独 module 负责）。

## 3. 依赖

- spec 006 FluxingPanelHost + FluxingComponents 组件库。
- spec 009 PhrasesPage（不重复实现）。
- librime yaml-cpp（已 vendored）。
- 暗色主题：spec 004 §9.4 跨子 spec 集成点表。