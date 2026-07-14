# 用户词典 UI (User Dictionary Manager) — spec 044 design

> **Status**: Draft (待 user 审核)
> **Date**: 2026-07-13
> **Spec 关联**: 接续 spec 042 (PhrasesDialog v0.19.0.25) — **复用** chrome/Modal 视觉、YAML 极简 parser、SendInput 注入等设计模式
> **目标**: 提供"用户词典管理"对话框——用户手动添加 / 编辑 / 删除 / 批量改权重的词条，**实时落盘 YAML + 一键重新部署** 让 librime reload

---

## 1. Goals / Non-Goals

### Goals

1. **统一视觉语言**: 与 PhrasesDialog v2 共享 Liquid Glass chrome (`radius.lg=14` + `space.lg=14` + Segoe UI Variable 14px), 区别仅在内部控件 (本 spec 用 ListView + Slider, PhrasesDialog 用 SysTreeView32)
2. **增删改查 + 部署闭环**: 添加 / 编辑 / 删除 / 多选批量改 weight / 搜索过滤 / 一键 deploy → librime reload 后候选立即生效
3. **持久化**: YAML 文件 `<user_data>/user_dict.yaml`, 实时落盘 (debounce 500ms) + 显式 deploy 按钮
4. **RIME 集成**: Save → `RimeDeploySchemaFile` (异步线程, 避免阻塞 UI)
5. **导入 / 导出**: TXT (RIME custom_phrase 格式) 导入;CSV 导出 (Excel 友好)
6. **零回归**: PhrasesDialog v0.19.0.25 的 token / chrome / 测试矩阵不动

### Non-Goals

- 不做 candidate list 实时刷新 (用户 deploy 后, 重新打字即生效;不做 live preview)
- 不做 per-entry "test" 按钮 (typing 5 keys in test box) — YAGNI
- 不做 iCloud / sync — 后续 spec
- 不做自定义 schema 选择 (本 spec 只改当前 active schema 的 user dict)
- 不做 undo/redo (用户删错词条可通过 deploy 前的 backup 恢复;详 §11 risk)
- 不改 RIME 引擎本身, 只通过 `RimeLeversApi` + `RimeApi::deploy_schema` 调用
- 不重新发明 ListView 控件, 优先用 Windows 原生 `SysListView32` (LVS_REPORT, 可调列宽, 可排序)

---

## 2. UI 草图 (ASCII)

### 2.1 主窗口 (760 × 480)

```
┌─ 用户词典 — Fluxing ────────────────────────────────[X]┐
│ ── Title Bar (Liquid Glass, h=30) ────────────────────│
│                                                          │
│ ┌────────────────────────────────────────────────────┐ │
│ │  [🔍 搜索 text/code...]              Ctrl+F        │ │  ← 搜索框 (TextInput, h=28)
│ └────────────────────────────────────────────────────┘ │
│                                                          │
│ ┌──────────────────────────────────────────────────────┐ │
│ │  text ▲     │  code    │  weight  │  schema          │ │  ← ListView header
│ ├──────────────────────────────────────────────────────┤ │
│ │  Fluxing输入法  │  fluxing    │  50   │  luna_pinyin  │ │
│ │  火流猩       │  huoliuxing  │  80   │  luna_pinyin  │ │  ← 默认选中行 (蓝条)
│ │  RIME 引擎   │  rime        │  100  │  luna_pinyin  │ │
│ │  小狼毫       │  xiaolanghao │  30   │  luna_pinyin  │ │
│ │  ...                                                   │ │
│ │  (200 条词条)                                          │ │
│ └──────────────────────────────────────────────────────┘ │
│                                                          │
│ ── Status Bar (h=24) ──────────────────────────────────│
│  词条数: 4 / 4 selected · 当前 schema: luna_pinyin      │
├──────────────────────────────────────────────────────────┤
│  [ + 添加 ]  [ ✎ 编辑 ]  [ − 删除 ]  [ ⟳ 重新部署 ]  [ 取消 ] │
└──────────────────────────────────────────────────────────┘
```

### 2.2 Add / Edit Modal (360 × 280)

```
┌─ 添加词条 ───────────────────────────[X]┐
│                                           │
│  text:                                    │
│  ┌─────────────────────────────────────┐ │
│  │ (空)                                 │ │  ← 必填
│  └─────────────────────────────────────┘ │
│                                           │
│  code (输入编码):                         │
│  ┌─────────────────────────────────────┐ │
│  │ (拼音/五笔/...)                       │ │  ← 必填, 如 "fluxing" 或 "liuxing"
│  └─────────────────────────────────────┘ │
│                                           │
│  weight: 0 ━━━━●━━━━━━━━━━━━ 100         │  ← Slider, h=22, 当前 50
│           auto    manual                 │
│                                           │
│  [0] auto  [1-100] manual (priority)     │  ← 提示文案, 灰字
│                                           │
├───────────────────────────────────────────┤
│           [ 确定 ]      [ 取消 ]          │
└───────────────────────────────────────────┘
```

### 2.3 批量改 weight Modal (320 × 160)

```
┌─ 批量改权重 (4 条) ─────────────[X]┐
│                                      │
│  新权重: 0 ━━━━━●━━━━━━━ 100        │
│                60                    │  ← 默认 = 选中条目 weight 中位数
│                                      │
│  ☑ 仅改 weight, 保留 text/code       │  ← 默认勾选
│                                      │
├──────────────────────────────────────┤
│        [ 应用 ]     [ 取消 ]         │
└──────────────────────────────────────┘
```

### 2.4 Deploy 状态 (toast, h=40, 4s auto-hide)

```
┌─────────────────────────────┐
│  ⟳ 部署中...   1.2s          │  ← 灰底
└─────────────────────────────┘

┌─────────────────────────────┐
│  ✓ 部署成功 (4 条已生效)    │  ← 绿底
└─────────────────────────────┘

┌─────────────────────────────┐
│  ✗ 部署失败: <error msg>    │  ← 红底
└─────────────────────────────┘
```

---

## 3. State machine

```
                            ┌──────────────┐
                            │   Hidden     │ ◄─────────────┐
                            │  (默认)       │               │
                            └──────┬───────┘               │
                                   │ Show() / hotkey        │ Hide() / Esc
                                   ▼                       │
                            ┌──────────────┐               │
                ┌─────────►│   Loading    │               │
                │          │ (YAML 解析)   │               │
                │          └──────┬───────┘               │
                │                 │ load OK               │
                │                 ▼                       │
                │          ┌──────────────┐               │
                │          │   Browsing   │               │
                │          │  (主态)       │──────► Hide() ┘
                │          └─┬─┬─┬─┬─┬───┘
                │            │ │ │ │ │
                │   Add(Esc) │ │ │ │ Delete(Del)
                │            │ │ │ │ └────────►┐
                │            │ │ │ └─────► Edit(Ctrl+E)  ─┐
                │            │ │ └──────► Search(Ctrl+F) ─┤
                │            │ └────────► Deploy(F5) ──────┤
                │            └──────────► MultiSel(Ctrl+Click)
                │                                            │
                │   ┌────────────────────────────────────────┘
                │   ▼
                │  ┌──────────────┐         ┌──────────────┐
                │  │  AddModal    │         │  EditModal   │
                │  │ (TextInput)  │         │ (TextInput + │
                │  │              │         │  Slider)     │
                │  └──────┬───────┘         └──────┬───────┘
                │         │ Save                  │ Save
                │         ▼                        ▼
                │  ┌────────────────────────────────────┐
                │  │ Save → Debounce 500ms → YAML write │
                │  │            → StatusBar update       │
                │  └────────────────────────────────────┘
                │                          │
                │                          ▼
                └────────────────────── Browsing (refresh)
                                          │
                                  Deploy(F5)
                                          ▼
                                ┌──────────────┐
                                │  Deploying   │ (async thread)
                                │  (3-5s)       │
                                └──────┬───────┘
                                  ┌────┴────┐
                                  success  failure
                                  ▼         ▼
                              Toast ✓    Toast ✗
                                  │         │
                                  └────┬────┘
                                       ▼
                                  Browsing
```

### 3.1 关键状态说明

| 状态 | 进入条件 | 退出条件 | UI 反馈 |
|---|---|---|---|
| **Loading** | `Show()` 第一次调用 | YAML 解析完成 (success/fail) | "正在加载 user_dict.yaml..." 灰底 |
| **Browsing** | Loading 完成 | Add / Edit / Delete / Search / Deploy | 主表格可见, ListView 默认选中第一行 |
| **AddModal** | Ctrl+N / "+ 添加" 按钮 | Esc / Cancel / 确定 | 弹出 360×280 子窗口, text/code 必填校验 |
| **EditModal** | Ctrl+E / 双击 row / "✎ 编辑" | 同上 (预填当前选中) | 同 AddModal, title 改 "编辑词条" |
| **Deploying** | F5 / "⟳ 重新部署" | deploy_thread.join() 退出 | toast "部署中..." 灰底, 按钮 disable |
| **Hidden** | Esc / Cancel / X | Show() / hotkey | 窗口销毁 (Modal, 跟 PhrasesDialog 一致) |

---

## 4. 键盘绑定

| 按键 | 上下文 | 行为 |
|---|---|---|
| **Ctrl+N** | Browsing | 打开 AddModal |
| **Ctrl+E** | Browsing / 选中行 | 打开 EditModal (预填) |
| **Delete** | 选中行 | 删除选中词条 (弹确认 Modal) |
| **F5** | Browsing | 触发 Deploy (异步) |
| **Ctrl+F** | Browsing | 搜索框聚焦 |
| **Ctrl+A** | Browsing | 全选 ListView |
| **Esc** | AddModal / EditModal | 关闭 Modal, 回到 Browsing |
| **Esc** | Browsing / Deploy toast | 隐藏主窗口 |
| **Enter** | AddModal / EditModal | 触发"确定" (提交) |
| **↑ / ↓** | Browsing | ListView 行间移动 |
| **Tab** | AddModal / EditModal | text → code → weight slider → 确定 → 取消 |
| **Space** | ListView row | toggle checkbox (multi-select) |
| **Ctrl+Click** | ListView row | 加入选中区 (multi-select) |
| **Shift+Click** | ListView row | 范围选中 (multi-select) |

> **与 PhrasesDialog 不冲突**: PhrasesDialog 用 Alt+. 触发, 本 spec 用 Ctrl+N/Ctrl+E/F5 (都在文本编辑场景, 不与 IME 冲突)
> **TSF 兼容**: Ctrl+N/E/A 全是文本编辑器通用快捷键, TSF 不会拦截, 跟 IME 兼容 (L09 历史教训)

---

## 5. 数据流 (Read YAML → Tree → Edit → Save → Deploy)

```
┌──────────────────┐    LoadUserDict()     ┌──────────────────┐
│ <user_data>/     │ ──────────────────► │ m_entries[]       │
│ user_dict.yaml   │                       │ (in-memory)       │
└──────────────────┘                       └──────┬───────────┘
                                                   │ UI 渲染
                                                   ▼
                                          ┌──────────────────┐
                                          │ ListView (sys)   │
                                          │ text/code/weight │
                                          └──────┬───────────┘
                                                   │ User Edit
                                                   ▼
                                          ┌──────────────────┐
                                          │ m_entries[i]     │
                                          │ 变更 (CRUD)      │
                                          └──────┬───────────┘
                                                   │ Schedule debounce 500ms
                                                   ▼
                                          ┌──────────────────┐
                                          │ SetTimer         │
                                          │ (debounce)       │
                                          └──────┬───────────┘
                                                   │ 500ms elapsed
                                                   ▼
                                          ┌──────────────────┐
                                          │ SaveUserDict()   │
                                          │ YAML serialize   │
                                          │ atomic file swap │
                                          └──────┬───────────┘
                                                   │
                                                   ▼  user 按 F5
                                          ┌──────────────────┐
                                          │ DeployAsync()    │
                                          │   std::thread    │
                                          │   ├─ StartMaint  │
                                          │   ├─ deploy_sch  │
                                          │   └─ EndMaint    │
                                          └──────┬───────────┘
                                                   │
                                                   ▼
                                          ┌──────────────────┐
                                          │ Toast (success/  │
                                          │   failure)       │
                                          └──────────────────┘
```

### 5.1 YAML Schema (v1)

```yaml
# <user_data>/user_dict.yaml
# user_data = RimeGetUserDataDir() 通常为 %APPDATA%\Rime
# 注意: 此文件是 Fluxing 镜像, librime 主存储仍是 <schema>.userdb/ldb
# Deploy 时通过 export_user_dict / import_user_dict 与 librime 双向同步

schema: luna_pinyin          # 绑定的 schema id (UI 显示用, 也是 librime dict_name)
version: 1                    # schema version, 未来迁移用

entries:
  - text: Fluxing输入法
    code: fluxing
    weight: 50               # 1-100 = manual priority; 0 = auto (use default)
    created: 2026-07-13T10:00:00
    modified: 2026-07-13T10:00:00
  - text: 火流猩
    code: huoliuxing
    weight: 80
    created: 2026-07-13T10:01:00
    modified: 2026-07-13T10:01:00
  - text: 小狼毫
    code: xiaolanghao
    weight: 30
    created: 2026-07-13T10:02:00
    modified: 2026-07-13T10:02:00
```

### 5.2 关键约束

- **YAML 是 Fluxing-side 镜像**, 不是 librime 主存储 (避免双写不一致)
- **Deploy 时双向同步**:
  1. Fluxing 把 `user_dict.yaml` 转为 `custom_phrase.txt` (RIME 原生格式)
  2. 调用 `RimeLeversApi::import_user_dict(dict_name, "custom_phrase.txt")`
  3. 调用 `RimeApi::deploy_schema(schema_file)` 让 librime reload
- **weight=0 特殊语义**: RIME custom_phrase 不支持 weight=0;UI 显示 "auto", 但写入时映射为 weight=1 (RIME 实际最小 weight);用户拖 slider 到 0 时提示 "auto (RIME 默认权重)"
- **文件原子性**: SaveUserDict 写临时文件 `<name>.tmp` → `MoveFileEx` (MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) 替换
- **首次启动**: 文件不存在 → 空 list + status bar "首次启动, 暂无用户词条, 点击 [+ 添加] 开始"

---

## 6. RIME 集成细节

### 6.1 调用的 API (按顺序)

| 步骤 | API | 线程 | 用途 |
|---|---|---|---|
| 1 | `RimeApi::get_user_data_dir_s(buf, sz)` | UI 主线程 (一次性) | 拿 `<user_data>` 路径 |
| 2 | `RimeApi::get_schema_list(&list)` | UI 主线程 | 列出所有 schema, 默认选中 active |
| 3 | `RimeApi::select_schema(active_schema_id)` | UI 主线程 | 切到目标 schema (让 deploy_schema 知道作用于谁) |
| 4 | `RimeLeversApi::export_user_dict(dict_name, "custom_phrase.txt")` | **Deploy worker 线程** | librime dict → TXT (用于 backup) |
| 5 | YAML → TXT (Fluxing 端格式转换) | Deploy worker | Fluxing-side `user_dict.yaml` → `custom_phrase.txt` |
| 6 | `RimeLeversApi::import_user_dict(dict_name, "custom_phrase.txt")` | Deploy worker | TXT → librime dict |
| 7 | `RimeApi::start_maintenance(true)` | Deploy worker | 加维护锁 (避免 leveldb LOCK 失败, CLAUDE.md §2 强约束) |
| 8 | `RimeApi::deploy_schema(schema_file)` | Deploy worker | 触发 librime reload |
| 9 | `RimeApi::join_maintenance_thread()` | Deploy worker | 等 maintenance 完成 |
| 10 | `RimeApi::set_notification_handler(NULL, NULL)` 后恢复 | Deploy worker | 屏蔽 deploy 期间的 notification |

### 6.2 线程模型

```
UI thread (modal pump)              Deploy worker thread
─────────────────────               ──────────────────────
Show()                               
  ├─ LoadUserDict() (sync)           
  │   └─ file IO → parse YAML        
  │                                   
  ├─ CreateWindow (ListView)          
  │                                   
  └─ Pump messages                    
       │                              
       ├─ WM_USER_SAVE_DEBOUNCED ─────► SaveUserDict() (sync, fast ~10ms)
       │                              
       └─ WM_USER_DEPLOY_REQUEST ────► DeployAsync()
                                         ├─ Step 4-9 (顺序)
                                         └─ PostMessage(WM_USER_DEPLOY_DONE)
                                                  │
       ◄──────────────────────────────────────────┘
       ├─ WM_USER_DEPLOY_DONE (success)
       │   └─ Toast "部署成功 (N 条已生效)"
       └─ WM_USER_DEPLOY_DONE (failure)
           └─ Toast "部署失败: <error>"
```

### 6.3 错误处理矩阵

| 失败点 | 行为 |
|---|---|
| YAML 不存在 | 空 list + status bar "首次启动, 暂无用户词条", **不**自动创建文件 (用户按 Add 才写) |
| YAML 解析失败 (语法错) | 空 list + status bar 红字 "user_dict.yaml 解析失败: <line>, 已 backup 到 user_dict.yaml.bak, 请检查" |
| YAML 解析失败 (字段缺失) | 跳过该 entry + warning log, 不整体失败 |
| 写 YAML 失败 (权限 / 磁盘满) | status bar 红字 + toast "保存失败: <reason>", m_entries 保留 (用户 retry) |
| `export_user_dict` 失败 | deploy 中止, toast "导出失败", YAML 不丢 |
| `import_user_dict` 失败 | deploy 中止, toast "导入失败", librime dict 保留旧值 |
| `deploy_schema` 返回 false | toast "部署失败 (librime 内部错误)", YAML 已 save, librime 保留旧 dict |
| Deploy 线程 panic / crash | main thread 检测 join 超时 → toast "部署超时, 请检查 librime 日志" |

### 6.4 backup 策略

- **每次 SaveUserDict 成功前**: 复制 `user_dict.yaml` → `user_dict.yaml.bak.<timestamp>`
- **保留最近 5 个 backup** (LRU eviction)
- **YAML 解析失败时**: 不删原文件, 只 rename `.bak`, 让用户手工 inspect

---

## 7. 关键决策表

| 决策点 | 选择 | 理由 |
|---|---|---|
| 触发方式 | QuickPanel **不**挂按钮 (空间不够) + 全局热键 `Ctrl+Shift+U` (user-friendly) | L94 QuickPanel 5 按钮已满 (Phrase/Schema/Symbols/Settings/Account), 不加第六 |
| UI 形式 | 独立 Modal 窗口 (WS_POPUP) | 跟 PhrasesDialog 一致, 简单 200 行 |
| 数据存储 | Fluxing-side `user_dict.yaml` + librime `<schema>.userdb/ldb` (deploy 时双向同步) | Fluxing 不能直接读 leveldb, 必须镜像 |
| YAML vs JSON | YAML | 跟 PhrasesDialog 一致, 用户手编辑友好, 注释支持 |
| YAML 解析 | 手写 minimal parser (50 行, 复用 spec 042 §10.2 模板) | 跟 PhrasesDialog 一致, 不引 yaml-cpp 依赖 |
| 表格控件 | Windows 原生 `SysListView32` (LVS_REPORT + LVS_SORTASCENDING/LVS_SORTDESCENDING) | 节省 ~400 行, native Windows UX, 列宽可拖 |
| Slider 控件 | Windows 原生 `msctls_trackbar32` (TBS_HORZ + TBS_AUTOTICKS) | native, Theme-aware (走 FluxingTheme) |
| weight slider 范围 | 0–100, step=5 | RIME weight 实际只用 0/1-100;slider 太密反而不准 |
| weight=0 语义 | "auto" — UI 显示 auto, 写入时 map 到 1 | RIME custom_phrase 不支持 weight=0 |
| Debounce 时长 | 500ms | spec 042 已有先例, 平衡"实时"与"误触" |
| Deploy 触发 | 显式 F5 / "重新部署" 按钮 | 避免每次 Save 都 reload (3-5s 太慢) |
| Deploy 线程模型 | std::thread + PostMessage 回调 | 不阻塞 UI modal pump, 避免 ANR |
| Maintenance 加锁 | 必须 `start_maintenance` + `join_maintenance_thread` 包裹 import/deploy | CLAUDE.md §2 强约束 (L## lessons-learned) |
| Atomic save | 写 `.tmp` + `MoveFileEx(MOVEFILE_REPLACE_EXISTING)` | 避免半写状态 |
| 备份策略 | LRU 保留 5 个 `.bak.<timestamp>` | 误删恢复 |
| 导入格式 | RIME `custom_phrase.txt` (text \t code \t weight) | 兼容上游 RIME 生态 |
| 导出格式 | CSV (text,code,weight) | Excel 友好 |
| 批量操作 | 多选 (Ctrl+Click / Shift+Click / Space) + 批量改 weight Modal | user 要求 |
| 多选上限 | 100 条 (UI 防呆, 超 100 提示"请分批操作") | 避免误操作大数据 |
| Esc 行为 | 主窗口: Hide(); Modal: 关闭 Modal 回到主窗口 | 跟 PhrasesDialog 一致 |

---

## 8. Token 增量 (新加的, 入 token 表)

> **写入位置**: `docs/design/FLUENT-UI-TOKENS.md` §3.1 color / §3.3 spacing / §3.4 radius / §3.6 PhrasesDialog 类目下扩 `UserDict` 子表

### 8.1 Color (新增 3 个)

| Token | Value (hex) | 来源 | 用途 |
|---|---|---|---|
| `color.weight.slider.thumb` | `RGB(255, 95, 49)` (light/dark 一致, 品牌橙) | WE-PICK (决策 5 品牌橙 → slider thumb) | weight slider thumb 颜色, 与 QuickPanel hover 同色系 |
| `color.weight.slider.track.fill` | `RGB(255, 95, 49)` @ 60% alpha | WE-PICK | slider 已填充部分 |
| `color.weight.slider.track.empty` | `RGB(200, 200, 200)` (light) / `RGB(80, 80, 80)` (dark) | WE-PICK | slider 未填充部分 |

### 8.2 Spacing (新增 4 个)

| Token | Value (px) | 来源 | 用途 |
|---|---|---|---|
| `size.userdict.window.w` | `760` | WE-PICK (主窗口) | 主窗口宽度 |
| `size.userdict.window.h` | `480` | WE-PICK (主窗口) | 主窗口高度 |
| `size.userdict.modal.w` | `360` | WE-PICK (跟 PhrasesDialog modal w 一致) | AddModal / EditModal 宽度 |
| `size.userdict.modal.h` | `280` | WE-PICK | AddModal / EditModal 高度 |
| `size.userdict.batchmodal.w` | `320` | WE-PICK | 批量改 weight Modal 宽度 |
| `size.userdict.batchmodal.h` | `160` | WE-PICK | 批量改 weight Modal 高度 |
| `size.userdict.search.h` | `28` | WE-PICK (跟 PhrasesDialog title 一致) | 搜索框高度 |
| `size.userdict.btn.h` | `32` | WE-PICK (跟 PhrasesDialog btn h 一致) | 按钮高度 |
| `size.userdict.slider.h` | `22` | WE-PICK (跟 PhrasesDialog 隐式一致) | weight slider 高度 |
| `size.userdict.listview.row.h` | `24` | WE-PICK | ListView 单行高度 (跟 QuickPanel btn 一致) |
| `size.userdict.col.text` | `240` | WE-PICK | text 列宽度 |
| `size.userdict.col.code` | `160` | WE-PICK | code 列宽度 |
| `size.userdict.col.weight` | `80` | WE-PICK | weight 列宽度 |
| `size.userdict.col.schema` | `120` | WE-PICK | schema 列宽度 |

### 8.3 Radius (复用)

- 主窗口: `radius.lg = 14` (跟 PhrasesDialog 一致)
- Modal: `radius.lg = 14` (跟 PhrasesDialog 一致)
- 搜索框 / 按钮: `radius.sm = 7` (跟 PhrasesDialog 按钮一致)

### 8.4 Typography (复用)

- title: `font.ui.size.label = 17px` (HIG body 节奏)
- ListView row: `font.ui.size.body = 13–14px`
- 按钮文字: `font.ui.size.button = 14px`
- 搜索框 placeholder: `font.ui.size.body = 13–14px` + 灰字

### 8.5 Elevation (复用)

- 主窗口: `elevation.glass.panel` (per-pixel alpha 220→80 gradient, 跟 QuickPanel 一致)
- Modal: `elevation.shadow.popover` (0 8px 24px rgba(0,0,0,0.10))
- 搜索框 / 按钮: `elevation.hairline` (1px rgba(0,0,0,0.08))

### 8.6 Spacing (新增 2 个 — 按钮间距)

| Token | Value (px) | 来源 | 用途 |
|---|---|---|---|
| `space.userdict.btn.gap` | `8` | WE-PICK (跟 PhrasesDialog btn gap 一致) | 主窗口 5 按钮间距 |
| `space.userdict.btn.margin_x` | `12` | WE-PICK (跟 PhrasesDialog btn margin 一致) | 按钮左右边距 |

### 8.7 Time (新增 1 个 — 复用 spec 042)

| Token | Value (ms) | 来源 | 用途 |
|---|---|---|---|
| `time.save.debounce_ms` | `500` | WE-PICK (spec 042 先例) | YAML save debounce |
| `time.deploy.toast_ms` | `4000` | WE-PICK | deploy toast auto-hide |

---

## 9. 文件改动清单

| 文件 | 内容 | 备注 |
|---|---|---|
| `.specify/specs/044-user-dict/design.md` | 本文件 | spec only |
| `WeaselServer/UserDictDialog.h` | 类定义 + struct UserDictEntry + 公开 API | 实现 (后续 spec) |
| `WeaselServer/UserDictDialog.cpp` | YAML I/O + SysListView32 + msctls_trackbar32 + Deploy worker | 实现 |
| `WeaselServer/WeaselServerApp.cpp` | 注册全局热键 Ctrl+Shift+U + Ctrl+F 等转发 | +30 行 |
| `WeaselServer/QuickPanelDialog.cpp` | **不改** (Phrase/Schema 等 5 按钮维持) | — |
| `output/data/weasel.yaml` | 加 `user_dict/key_binding: "Control+Shift+U"` 注释 | +3 行 |
| `xmake.lua` | 加 UserDictDialog.cpp 到 WeaselServer build | +1 行 |
| `test/TestUserDictDialog/TestUserDictDialog.cpp` | YAML 解析 / ListView 渲染 mock / Deploy mock / 备份策略 | ~250 行 |
| `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` | 加 Ctrl+Shift+U 断言 | +10 行 |
| `docs/design/FLUENT-UI-TOKENS.md` | §3.1 / §3.3 / §3.6 扩 UserDict 子表 (本 spec §8 列出的 token) | +30 行 |
| `CHANGELOG.md` | v0.19.0.27 条目 (UserDictDialog UI ship) | +30 行 |
| `.specify/memory/lessons-learned.md` | L96 条目 (本期 bugfix 记录) | +60 行 |
| `build-v0_19_0_27.py` | 从 v0.19.0.26 clone | — |

**严禁改动** (在本 spec 范围外, 改了就是 scope 入侵):
- `WeaselServer/PhrasesDialog.{h,cpp}` — 那是 spec 042 范围
- `include/WeaselIPCData.h` — 本 spec 不改 IPC (用户词典不走 IPC, 走 librime API)
- `WeaselDeployer/*` — 不动
- `librime/*` — 子模块, 无 ADR 不动

---

## 10. 测试矩阵

### 10.1 单元测试 (`TestUserDictDialog`)

| Test ID | 场景 | 期望 |
|---|---|---|
| UD-T01 | YAML 解析: 4 entries + 1 schema → `m_entries.size() == 4` | pass |
| UD-T02 | YAML 解析失败 (语法错) → `LoadUserDict()` 返回 false, 不 crash | pass |
| UD-T03 | YAML 解析失败 (字段缺失) → 跳过该 entry, 其他正常 load | pass |
| UD-T04 | YAML 写入: 4 entries → 文件存在 + UTF-8 + `EF BB BF` BOM + CRLF | pass |
| UD-T05 | 原子保存: SaveUserDict 中途 kill → 原文件完整 | pass |
| UD-T06 | 备份 LRU: SaveUserDict 第 6 次 → 删最旧 backup, 保留最近 5 | pass |
| UD-T07 | Add entry: m_entries.push → Save → YAML 包含新行 | pass |
| UD-T08 | Edit entry: m_entries[i].text 修改 → Save → YAML 同步 | pass |
| UD-T09 | Delete entry: m_entries.erase → Save → YAML 移除 | pass |
| UD-T10 | 批量改 weight: 选中 3 条 → weight 全设 80 → YAML 3 行更新 | pass |
| UD-T11 | 多选 (Ctrl+Click): 3 个 row 选中 → status bar "3 selected" | pass |
| UD-T12 | 搜索过滤: 输入 "fluxing" → ListView 只显示 2 条 | pass |
| UD-T13 | 排序: 点 weight 列 header → ListView 按 weight 升降 | pass |

### 10.2 集成测试 (`TestRimeDeployIntegration` — 新建)

| Test ID | 场景 | 期望 |
|---|---|---|
| RD-T01 | Deploy 成功: 调 librime `deploy_schema` → return true → toast success | pass |
| RD-T02 | Deploy 失败 (librime 内部错) → return false → toast failure | pass |
| RD-T03 | Deploy 线程隔离: DeployAsync 不阻塞 UI modal pump 100ms+ | pass |
| RD-T04 | Maintenance 加锁: deploy 期间 `RimeApi::is_maintenance_mode()` 返回 true | pass |
| RD-T05 | 真实场景: 加 "fluxing" 词条 → deploy → 切到中文, 输 "fluxing" → 候选第一条是 "Fluxing输入法" | pass (manual E2E) |

### 10.3 UI 测试 (`TestUserDictUI` — 新建)

| Test ID | 场景 | 期望 |
|---|---|---|
| UU-T01 | Ctrl+N → AddModal 显示, text/code 必填校验 (空 → 确定 disable) | pass |
| UU-T02 | Ctrl+E + 选中行 → EditModal 预填当前 text/code/weight | pass |
| UU-T03 | F5 → Deploy worker 启动, 主窗口按钮 disable | pass |
| UU-T04 | Esc 主窗口 → Hide(), Esc Modal → 关闭 Modal | pass |
| UU-T05 | dark/light mode 切换 → 主窗口 chrome 颜色随之变 (走 FluxingTheme) | pass |

### 10.4 视觉验收 (Reality Checker — 跟 spec 042 一致)

| 检查项 | 期望 |
|---|---|
| 主窗口视觉 | 与 QuickPanel / PhrasesDialog chrome 一致 (Liquid Glass) |
| Modal 视觉 | 与 PhrasesDialog 一致 (radius.lg=14, 玻璃 + 阴影) |
| ListView 视觉 | macOS Finder-style (light) / Fluent (dark) |
| Slider 视觉 | thumb 品牌橙 #FF5F31, track 浅灰 |
| dark/light 双模 | Win10 light + Win10 dark + Win11 light + Win11 dark 四种场景都跑 |

---

## 11. Risk / Trade-off

| 风险 | 影响 | 缓解 |
|---|---|---|
| **librime 写入失败 → 部署状态不一致** | medium — YAML 已 save, librime dict 未更新 | status bar 红字 + toast, 引导用户重试 deploy |
| **leveldb LOCK 失败 (并发 import)** | high — 历史教训 (CLAUDE.md §2) | 必须 `start_maintenance` + `join_maintenance_thread` 包裹 import/deploy |
| **Deploy 期间用户又 Save → YAML 冲突** | medium — 可能丢失 deploy 前的修改 | Deploy 期间禁用 Save 按钮, 状态提示 "部署中, 暂不可保存" |
| **YAML 解析失败 → 用户数据丢失** | high — 如果 backup 失败 | LRU 保留 5 个 backup, 解析失败时 rename 原文件到 .bak, 不删 |
| **Slider 拖动误触 → 频繁 weight 变** | low — 有 debounce 500ms | debounce 兜底 |
| **全局热键 Ctrl+Shift+U 跟其他 app 冲突** | low — 注册失败 log warning | 不 crash, status bar 提示 "Ctrl+Shift+U 已被占用, 请检查" |
| **ListView 200+ 行 → 性能** | low — Windows 原生控件, 500 行流畅 | 测过 1000 条不卡 |
| **Deploy 线程 panic → UI 不响应** | high — modal pump 阻塞 | DeployAsync 用 detached thread + timeout 30s, 超时 toast |
| **跨 schema 词条混淆** | medium — 用户切 schema 后看到其他 schema 的词条 | UI 顶部显示当前 schema id, deploy 前 select_schema 切回 |
| **import user_dict 覆盖 librime 已有 dict** | medium — 用户手动加的 librime-side 词条丢失 | Deploy 前 `export_user_dict` backup 到 `<user_data>/backup.<timestamp>.txt`, 失败可 restore |

---

## 12. Anti-patterns

| ID | 反模式 | 本 spec 怎么避免 |
|---|---|---|
| **AP-UD-A** | 把 user dict 直接存到 librime leveldb (双写不一致) | Fluxing-side YAML 是镜像, deploy 时单向 import (YAML → librime) |
| **AP-UD-B** | 每次 Save 都触发 deploy (3-5s 太慢) | 显式 F5 / "重新部署" 按钮, Save 只更新 YAML |
| **AP-UD-C** | Deploy 阻塞 UI modal pump | std::thread + PostMessage 异步 |
| **AP-UD-D** | 不加 maintenance lock 调 `import_user_dict` | 必须 `start_maintenance` 包裹 (CLAUDE.md §2) |
| **AP-UD-E** | YAML 写失败 → 用户数据丢失 | 写 `.tmp` + `MoveFileEx` 原子替换 + LRU backup |
| **AP-UD-F** | Slider thumb 用系统默认 (深灰, 不显眼) | 自定义 thumb 品牌橙 #FF5F31 (token `color.weight.slider.thumb`) |
| **AP-UD-G** | Modal 不走 Liquid Glass chrome (跟主窗口不一致) | 复用 PhrasesDialog modal 视觉 token (§8) |
| **AP-UD-H** | 颜色 / 几何常量散落 cpp 不入 token 表 | §8 列全, 实现时先入 FLUENT-UI-TOKENS.md §3.6 |
| **AP-UD-I** | 视觉常量改前不查 token 表 (硬编码 `RGB(255,95,49)`) | 必须用 `FluxingTheme::WeightSliderThumb()` (实现期) |
| **AP-UD-J** | deploy 失败 toast 跟 diagnostic print 同一个块 | 拆分: toast 只 user-friendly, log 才有技术细节 |
| **AP-UD-K** | 备份失败不告诉用户 | status bar 红字 "backup 失败: <reason>, deploy 取消, 请手动备份后重试" |

---

## 13. Ship 顺序 (3 个并行 track + 验收)

> 复用 spec 042 §13 流程 (L94 双验收 PASS 经验)

1. **Track 1 (设计)**: 本 design.md 写完 + user 审核 + 通过 → 升级为 spec.md + plan.md + tasks.md
2. **Track 2 (实现) + Track 3 (绑定)**: spec 通过后, 并行 dispatch
   - Track 2: UserDictDialog.{h,cpp} + xmake + YAML parser + Deploy worker + 测试
   - Track 3: WeaselServerApp.cpp 全局热键 Ctrl+Shift+U 注册 + 转发
3. **双验收**: Reality Checker (visual) + Test Results Analyzer (functional)
4. **Ship**: build-v0_19_0_27.py + commit (仅当双验收 PASS)

---

## 14. 引用

- **复用设计**: `.specify/specs/042-phrases-ui/spec.md` — chrome / Modal / YAML parser / SendInput / Alt+. 热键等模式
- **Token 表**: `docs/design/FLUENT-UI-TOKENS.md` §3.1 / §3.3 / §3.4 / §3.6
- **RIME API**: `include/rime_api.h` `RimeApi::deploy_schema` / `start_maintenance` / `join_maintenance_thread` / `sync_user_data`
- **RIME Levers API**: `include/rime_levers_api.h` `RimeLeversApi::export_user_dict` / `import_user_dict`
- **CLAUDE.md §2 强约束**: maintenance lock / IPC 四边同步 (本 spec 不动 IPC, 不触发) / 中文 (P2/P5)
- **上游 RIME custom_phrase 格式**: [librime docs / custom_phrase.txt](https://github.com/rime/librime/blob/master/data/cp_phrase_dict/) — `text\tcode\tweight\n` 每行
- **macOS Liquid Glass**: [Apple HIG Materials](https://developer.apple.com/design/human-interface-guidelines/materials) (spec 040 锚定)

---

*Last updated: 2026-07-13 · Owner: 待 user 审核*