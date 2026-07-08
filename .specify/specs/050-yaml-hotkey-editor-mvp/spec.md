# 050 - yaml 快捷键可视化编辑器（v0.18.30 F3 MVP）

> **MVP 切片**：spec 007 完整 yaml 配置 UI 有 4 页（快捷键 / 样式 / 方案 / 用户词典），本 spec 只做第 1 页（快捷键）。剩余 3 页拆到 spec 051/052/053。
>
> 理由：4 页全部做完需要 2-3 周（每个 page 1 个 WTL 对话框 + IPC + 测试）。MVP 缩短到 3-5 天即可 ship v0.18.30，把"用户改快捷键不再手写 yaml"这个最高价值的需求交付出去。

## 0. 上下文

- spec 007 已 ship design（4 页 + ConflictChecker + ConfigEditorApp），全部 14 tasks 未实施。
- YamlRoundTrip 模块已 ship（spec 024），能读 / 写 yaml 保留 key order。
- 当前用户改 key_binder 流程：手编辑 `output\data\default.yaml` → 重新 deploy → 重启 WeaselServer.exe。
- L19 教训：单键 Shift_L binding 容易与 TSF release event 误匹配，UI 编辑器需要在保存前做基本冲突检测。
- L40 教训：default.yaml 直接编辑后被升级覆盖；**必须写到 user_data_dir/default.custom.yaml**（per project-knowledge §6.2 path 2）。

## 1. 产品视角

### 1.1 目标

让用户通过图形界面查看 / 添加 / 修改 / 删除 key_binder/bindings 条目，**不再手写 yaml**。改完点保存，立即生效（不需要重启）。

### 1.2 用户故事

- US050-A [P1]: 打开快捷键编辑器 → 看到当前 `key_binder/bindings` 全部条目（按 when 字段分组显示：composing / has_menu / always）。
- US050-B [P1]: 点"添加" → 弹出按键录制器 → 按 `Ctrl+Shift+F1` → 自动解析为 `Control+Shift+F1` → 输入动作类型（send / toggle / select）+ 动作值 → 保存 → 写入 `default.custom.yaml`。
- US050-C [P1]: 选某条 → 点"删除" → 该条从列表消失（仍写入 custom.yaml，等同覆盖 default.yaml 的同 accept 条目）。
- US050-D [P1]: 选某条 → 点"修改 accept" → 按键录制器 → 按新键 → 保存。
- US050-E [P1]: 关闭编辑器 → 触发 WeaselDeployer.exe /deploy → 新 key_binder 立即生效。
- US050-F [P2]: 检测到与现有 binding 冲突（相同 accept + when）→ 提示"已存在，是否覆盖？"
- US050-G [P2]: 检测到与 librime 内置 binding 冲突（如 Control+1 默认选候选 1）→ 警告"覆盖默认行为"。

### 1.3 验收

- Given 全新安装 Fluxing v0.18.30，default.yaml 有 10+ bindings，
- When 用户打开"快捷键编辑器"（通过托盘面板入口 / 未来 Alt+, 更多 → 偏好），
- Then 弹出独立窗口，列表显示所有当前 bindings。
- And 用户添加 / 删除 / 修改条目 → 点保存 → 写入 `<user_data_dir>/default.custom.yaml`。
- And 关闭编辑器 → 触发 `/deploy` → 新 bindings 立即生效。
- And 升级到下一版本时，用户的 custom.yaml 不会被覆盖。

### 1.4 不在范围（MVP 推迟）

- ❌ StylePage（配色 / 透明度 / 字体）→ spec 051
- ❌ SchemaPage（方案启用 / 拖拽）→ spec 052
- ❌ UserDictPage（用户词典搜索 / 编辑）→ spec 053
- ❌ ConflictChecker 完整静态 db（JSON 列表）→ spec 054（v0.19+ 优化）
- ❌ FluxingPanelHost 独立进程（spec 006 路线图）→ 现有 WeaselDeployer.exe 启动
- ❌ 暗色主题 UI 细节 → 复用 FluxingComponents 默认主题
- ❌ 导入/导出配置包 → spec 055（v2.1+）

## 2. 范围

### 2.1 改动文件

**新建**：
- `FluxingConfigEditor/HotkeyEditorDialog.{h,cpp}` - 主对话框（~500 行 WTL/ATL）
- `FluxingConfigEditor/KeyRecorder.{h,cpp}` - 按键录制器（~150 行，windowless 子控件）
- `FluxingConfigEditor/HotkeyBinding.h` - 数据结构 + 序列化（~80 行）
- `test/TestHotkeyEditor/` - 行为级测试项目（3-5 个 test case）

**修改**：
- `WeaselDeployer/WeaselDeployer.rc` - 加 menu item "快捷键编辑器" 入口
- `WeaselDeployer/WeaselServerApp.cpp` 或 `WeaselDeployer.cpp` - 加 IPC handler 启动编辑器
- `weasel.sln` + `xmake.lua` + `scripts/test-infra/run-test-suite.bat` - 注册新模块 + 测试

### 2.2 不在范围
- WeaselServer 侧改动（编辑器作为独立 exe 启动，不需 IPC）
- librime 内置 binding 完整列表（仅做基本冲突检测，覆盖提示即可）
- YamlRoundTrip 升级（现有 API 够用）

## 3. 数据流

```
[打开编辑器]
  ├─ 读 <user_data_dir>/default.custom.yaml（如不存在，fallback 到 default.yaml）
  ├─ YamlRoundTrip::Load
  └─ 提取 key_binder.bindings → HotkeyBinding[] 列表

[用户编辑]
  ├─ 添加：按键录制 → 解析 accept (e.g. "Control+Shift+F1")
  ├─ 修改：选条目 → 同上
  └─ 删除：选条目 → 标记删除

[保存]
  ├─ 重建 key_binder.bindings 列表（保留 default.yaml 的非冲突条目 + 用户的 custom 条目）
  ├─ YamlRoundTrip::WriteString("key_binder.bindings", 序列化后的列表)
  ├─ 写回 default.custom.yaml
  └─ 触发 WeaselDeployer.exe /deploy
```

## 4. UI 设计

### 4.1 主对话框布局（~600×500）

```
┌─ Fluxing 快捷键编辑器 ─────────────────────────────┐
│                                                      │
│  ┌─ 过滤 ─────────────────────────────────────┐     │
│  │ 显示: [全部▾]  搜索: [____________] [清空] │     │
│  └────────────────────────────────────────────┘     │
│                                                      │
│  ┌─ 快捷键列表 ──────────────────────────────┐      │
│  │ ✓  when    accept           动作         │      │
│  │ ─────────────────────────────────────     │      │
│  │ 1  composing  Shift+Tab     → Shift+Left  │      │
│  │ 2  composing  Tab           → Shift+Right │      │
│  │ 3  has_menu  comma         → Page_Up     │      │
│  │ ... (滚动)                                │      │
│  │ 11 always   Shift+space    ⇄ ascii_mode  │      │
│  └────────────────────────────────────────────┘      │
│                                                      │
│  [添加]  [修改]  [删除]  [重置默认]                 │
│                                                      │
│  说明: 改完点保存触发 deploy，无需重启 WeaselServer。│
│                                                      │
│  [保存]  [取消]                                      │
└──────────────────────────────────────────────────────┘
```

### 4.2 按键录制器（弹出子窗口 ~400×200）

```
┌─ 按键录制 ──────────────────┐
│                              │
│  请按想要设置的组合键...    │
│                              │
│  [大输入框]                 │
│  Control+Shift+F1           │
│                              │
│  修饰键: ☐ Ctrl  ☐ Shift    │
│         ☐ Alt   ☐ Win       │
│  按键: [F1_____]            │
│                              │
│  [确定]  [取消]              │
└──────────────────────────────┘
```

按 ESC 取消，超时（10s 无输入）自动关闭。

## 5. 风险

| 风险 | 缓解 |
|---|---|
| YamlRoundTrip 写回后 key 顺序丢失 | YamlRoundTrip 已 ship 保留 key order（L27）|
| 用户改了 key_binder 触发 L18/L19 bug | 写一个简单的 `linter`：保存前检查是否有 `keycode=Shift_L` 单键 binding（L19 教训）|
| 触发 deploy 但 WeaselServer 没重启 → 旧 binary 还在 | 显示 "deploy 已触发，配置立即生效" 提示（同 spec 045 L51 NSIS post-install prompt 模式）|
| 编辑器自己的 IPC 路径被 taskkill 杀掉 | 编辑器作为独立 exe，无 IPC 状态（与 spec 042 不同场景）|
| `default.custom.yaml` 不存在 → 首次保存 | 首次保存时创建文件，写 minimal header |

## 6. 依赖

- spec 024 YamlRoundTrip（已 ship）— 读写 yaml
- spec 037 FluxingComponents（已 ship）— 控件可选用（按钮 / 列表）
- spec 041 DPI 修复（已 ship）— 144 DPI 下视觉正确
- WeaselDeployer.exe 启动 /deploy 的现有机制
- L17 / L18 / L19 / L21 教训（Shift key binding 风险）

## 7. 关联

- 父 spec 007 - 完整 yaml UI 蓝图（本 spec 是 MVP 子集）
- 后续 spec 051 - StylePage
- 后续 spec 052 - SchemaPage
- 后续 spec 053 - UserDictPage
- project-knowledge §6.2 yaml 编辑落地的两条路径（用 YamlRoundTrip + 写 custom.yaml）
- project-knowledge §3.2 librime 覆盖机制