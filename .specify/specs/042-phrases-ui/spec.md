# 常用短语 UI (v0.19.0.25) — spec

> **Status**: Draft (待 user 审核)
> **日期**: 2026-07-13
> **Spec 关联**: 接续 spec 070 v0.19.0.24 (L94 ship)
> **目标**: 给 QuickPanel Phrase 按钮 + Alt+. 热键 提供 常用短语 Modal 对话框 + 树形分类 UX

## 1. Goals

1. **Phrase button 真正工作**: L86 至今 s_onPhrases callback 一直 no-op(spec 070 T007), v0.19.0.25 接通 callback → PhrasesDialog::Show()
2. **新热键 Alt+.**: WeaselServerApp 注册全局 hotkey `Alt+.` → PhrasesDialog::Show()
3. **树形分类 UX**: 每个 phrase 有可选 `category` 字段,顶层显示分类(文件夹图标),←/→ 展开折叠,未分类 phrase 直接展示在分类行下方
4. **完整增删改**: Add/Edit/Delete 按钮,Esc/X/Cancel 关闭,Enter 注入
5. **持久化**: YAML 文件 `<user_data>/phrases.yaml`,所有改动实时落盘
6. **零回归**: v0.19.0.24 L94 验收 (35/35 + 5/5 + 1/1 + Reality/Test 双 PASS) 必须保持

## 2. Non-Goals

- 不做多选 / 复制 / 拖拽排序 (YAGNI)
- 不做 per-phrase 图标/颜色 (YAGNI)
- 不做 iCloud/sync (后续 spec)
- 不改 RIME 引擎本身,只 SendInput 注入
- 不重新发明 ListBox 控件,优先用 Windows 原生 SysTreeView32 (节省 ~400 行)

## 3. 架构

```
┌─────────────────────────────────────────────────┐
│ WeaselServerApp                                 │
│   ├─ 注册全局 hotkey Alt+. → onAltDot()         │
│   └─ s_onPhrases = &PhrasesDialog::Show         │
│                                                  │
│ QuickPanel Phrase 按钮 (hit==1)                 │
│   └─ s_onPhrases()  (QuickPanelDialog::LButtonUp│
│                      click 分支)                 │
│                                                  │
│ PhrasesDialog::Show()                            │
│   ├─ Load YAML → m_phrases[] (in-memory)        │
│   ├─ CreateWindowExW (WS_POPUP | WS_EX_LAYERED) │
│   ├─ CreateWindowExW SysTreeView32 (WS_CHILD)    │
│   ├─ CreateWindowExW 4 个按钮 (WS_CHILD)         │
│   ├─ ShowWindow + 居中                           │
│   └─ WM_*: key handling + button click + paint   │
│                                                  │
│ On Enter / 双击 phrase:                          │
│   └─ InjectText(text) via SendInput              │
│      + Hide() 关闭 modal                        │
└─────────────────────────────────────────────────┘
```

## 4. 关键决策

| 决策点 | 选择 | 理由 |
|---|---|---|
| 触发 | QuickPanel Phrase 按钮 + **Alt+.** 热键 | user 要求 |
| UI 形式 | Modal 对话框 (独立 WS_POPUP 窗口) | user 要求,Modal 比 inline popup 简单 200 行 |
| 选中 UX | 默认第一条,↑↓ 切换,Enter 注入,Esc 关闭 | user 要求 |
| 存储 | YAML 文件 `<user_data>/phrases.yaml` | user 要求 |
| 功能范围 | Add / Edit / Delete / Cancel 按钮 | user 要求 |
| 注入方式 | `SendInput` with `KEYEVENTF_UNICODE` | 跨 IME 状态工作,无需 RIME commit API |
| **分类 UX** | **树形 ListBox + ←/→ 展开折叠** | **user 追加要求** |
| 分类为空 | 显示在 "(未分类)" 折叠行下,跟普通分类同级 | user 原话 |
| 排序 | 分类按 name 字母序,uncategorized 置**底**(最后),phrase 在分类内按 YAML 顺序(用户定义序) | 简单可预测 |
| 树控件 | Windows 原生 `SysTreeView32` (TVS_HASBUTTONS + TVS_LINESATROOT) | 节省 ~400 行,native Windows UX |
| Modal 视觉 | Liquid Glass chrome (WS_EX_LAYERED + per-pixel alpha,跟 QuickPanel 一致) | 视觉一致 |
| 失败 fallback | YAML parse 失败 → 空 list + log warning,不 crash | 防御性 |

## 5. UI 草图

```
┌─ 常用短语 ─────────────────────────────────────[X]┐
│                                                    │
│  ▼ 工作                                            │  ← 分类行(展开,▼ 图标)
│      你好                                          │
│      谢谢                                          │
│      期待合作                                      │
│  ▼ 日常                                            │
│      收到                                          │
│  ▸ 好的,收到                                       │  ← 默认选中(蓝条)
│  ▼ (未分类)                                        │
│      Hello                                         │
│      测试短语                                      │
│                                                    │
├────────────────────────────────────────────────────┤
│  [ + 添加 ]    [ ✎ 编辑 ]    [ − 删除 ]    [ 取消 ]│
└────────────────────────────────────────────────────┘
```

## 6. YAML Schema

```yaml
# <user_data>/phrases.yaml
# user_data = RimeGetUserDataDir() 通常为 %APPDATA%\Rime
phrases:
  - text: 你好
    category: 工作
  - text: 谢谢
    category: 工作
  - text: 期待合作
    category: 工作
  - text: 收到
    category: 日常
  - text: 好的,收到
    category: 日常
  - text: Hello
    category: ""           # 空字符串表示 uncategorized
  - text: 测试短语
    category: ""
```

YAML 解析: **不引入 yaml-cpp 依赖**,手写 minimal parser:
- 读行,trim 空白
- `- text:` 后是 phrase start,读直到下一行 `-` 开头
- `category:` 字段 optional,默认 ""
- 忽略空行和 `#` 注释
- 容错:任何 parse error → log + fallback to empty list

## 7. UI 树形数据流

```cpp
struct Phrase {
  std::wstring text;
  std::wstring category;  // "" = uncategorized
};

class PhrasesDialog {
  std::vector<Phrase> m_phrases;          // 从 YAML load,扁平
  std::set<std::wstring> m_categories;    // 从 m_phrases 推导
  bool m_expanded[每个分类];              // 折叠状态
  int m_selectedHTREEITEM;                // 当前选中节点
  // ...
};
```

显示逻辑:
```
1. 收集 m_phrases 中所有非空 category → m_categories (sorted)
2. 渲染顺序:
   - "▼ 工作" (展开时) + 它的 phrases
   - "▼ 日常" + 它的 phrases
   - "▼ (未分类)" + category=="" 的 phrases
3. 用户点击分类行 → toggle expand
4. 用户点击 phrase → select → Enter 注入
```

## 8. 键盘绑定

| 按键 | 选中项类型 | 行为 |
|---|---|---|
| ↑ | 任意 | 移到上一个可见节点 |
| ↓ | 任意 | 移到下一个可见节点 |
| ← | 展开的分类 | 折叠 |
| ← | 折叠的分类 | no-op (留原位) |
| ← | phrase | 跳到所属分类行 |
| → | 折叠的分类 | 展开 |
| → | 已展开的分类 | 跳到第一个 child phrase |
| → | phrase | no-op |
| Enter | phrase | 注入 text + Hide() |
| Enter | 分类 | toggle 折叠 |
| Esc | 任意 | Hide() |
| Tab | 任意 | 在按钮 (Add/Edit/Del/Cancel) 间循环 |

## 9. 组件 / 文件改动

| 文件 | 内容 | 行数估算 |
|---|---|---|
| `WeaselServer/PhrasesDialog.h` | 类定义,API,struct Phrase | ~120 |
| `WeaselServer/PhrasesDialog.cpp` | 实现,YAML I/O,SendInput,SysTreeView32 wiring | ~500 |
| `WeaselServer/QuickPanelDialog.cpp` | LButtonUp click 分支加 `if (s_onPhrases && hit==1) s_onPhrases();` | +2 |
| `WeaselServer/WeaselServerApp.cpp` | 注册 Alt+. hotkey + 转发到 PhrasesDialog | +20 |
| `output/data/weasel.yaml` | 加 `phrases/key_binding: "Alt+."` 注释 (实际靠 C++ RegisterHotKey,不靠 yaml) | +3 |
| `xmake.lua` | 加 PhrasesDialog.cpp 到 WeaselServer build | +1 |
| `test/TestPhrasesDialog/*` | 新建测试: YAML 解析、tree 渲染 mock、SendInput mock | ~200 |
| `.specify/specs/042-phrases-ui/spec.md` | 本文件 | — |
| `CHANGELOG.md` | v0.19.0.25 条目 | +30 |
| `.specify/memory/lessons-learned.md` | L95 条目 | +60 |
| `docs/design/FLUENT-UI-TOKENS.md` | 加 dialog-related tokens (e.g. `time.dialog.fade_ms`) | +20 |

## 10. 关键算法

### 10.1 SendInput 注入 unicode

```cpp
void InjectText(const std::wstring& text) {
  std::vector<INPUT> inputs;
  inputs.reserve(text.size() * 2);
  for (wchar_t c : text) {
    INPUT down = {};
    down.type = INPUT_KEYBOARD;
    down.ki.wScan = c;
    down.ki.dwFlags = KEYEVENTF_UNICODE;
    inputs.push_back(down);

    INPUT up = down;
    up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    inputs.push_back(up);
  }
  SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
}
```

### 10.2 YAML 极简 parser (300 行实现含 string→wstring 转换)

不引入 yaml-cpp。理由:用户数据 50 条左右时,JSON-like 序列化都 overkill。一个 `<vector<Phrase>` 跟一个手写 parser (50 行) 足够。

```cpp
bool LoadPhrases(const std::wstring& path, std::vector<Phrase>& out) {
  std::wifstream f(path);
  if (!f) return false;
  std::wstring line;
  Phrase cur;
  bool inPhrase = false;
  while (std::getline(f, line)) {
    auto trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == L'#') continue;
    if (trimmed[0] == L'-' && trimmed.size() > 2 && trimmed[1] == L' ') {
      // 新 phrase 起始
      if (inPhrase) out.push_back(cur);
      cur = Phrase();
      inPhrase = true;
      // 解析 `- text: <value>` (inline form)
      auto rest = trimmed.substr(2);
      if (rest.substr(0, 5) == L"text:") {
        cur.text = Trim(rest.substr(5));
        cur.text = Unquote(cur.text);
      }
    } else if (inPhrase) {
      // 解析 `category: <value>`
      if (line.substr(0, 9) == L"category:") {
        cur.category = Trim(line.substr(9));
        cur.category = Unquote(cur.category);
      }
    }
  }
  if (inPhrase) out.push_back(cur);
  return true;
}
```

### 10.3 Tree populate

```cpp
void PopulateTree(HWND hTree) {
  TreeView_DeleteAllItems(hTree);
  // 收集所有 category (sorted)
  std::set<std::wstring> cats;
  for (auto& p : m_phrases) cats.insert(p.category);
  bool hasUncat = cats.count(L"") > 0;
  cats.erase(L"");

  // 渲染分类
  HTREEITEM hFirstPhrase = nullptr;
  for (auto& cat : cats) {
    HTREEITEM hCat = InsertItem(hTree, L"📁 " + cat, /*icon=*/0);
    if (m_expanded[cat]) TreeView_Expand(hTree, hCat, TVE_EXPAND);
    for (auto& p : m_phrases) {
      if (p.category == cat) {
        HTREEITEM hPhrase = InsertItem(hTree, L"  " + p.text, /*icon=*/1);
        TreeView_SetItemData(hTree, hPhrase, &p - &m_phrases[0]);
        if (!hFirstPhrase) hFirstPhrase = hPhrase;
      }
    }
  }
  // 渲染 "(未分类)" 如果有
  if (hasUncat) {
    HTREEITEM hUncat = InsertItem(hTree, L"📁 (未分类)", 0);
    if (m_expanded[L""]) TreeView_Expand(hTree, hUncat, TVE_EXPAND);
    for (auto& p : m_phrases) {
      if (p.category == L"") {
        HTREEITEM hPhrase = InsertItem(hTree, L"  " + p.text, 1);
        TreeView_SetItemData(hTree, hPhrase, &p - &m_phrases[0]);
        if (!hFirstPhrase) hFirstPhrase = hPhrase;
      }
    }
  }
  // 默认选中第一个 phrase
  if (hFirstPhrase) {
    TreeView_SelectItem(hTree, hFirstPhrase);
    SetFocus(hTree);
  }
}
```

### 10.4 Alt+. hotkey 注册

```cpp
// WeaselServerApp.cpp 启动时
BOOL ok = RegisterHotKey(NULL, HOTKEY_ID_PHRASES, MOD_ALT, VK_OEM_PERIOD);  // Alt+.
// WM_HOTKEY handler:
case WM_HOTKEY:
  if (wParam == HOTKEY_ID_PHRASES) {
    PhrasesDialog::Show();
  }
  break;
```

## 11. 测试

- **TestPhrasesDialog** (新建):
  - YAML parser: 10 个 phrase + 3 个 category → m_phrases[] 正确
  - YAML 解析失败: invalid YAML → 返回 false,m_phrases 不变
  - 树形 populate: 5 phrases + 2 cats → 7 items (2 分类 + 3 phrases 在 cat1 + 2 phrases 在 cat2),默认选中第一个 phrase
  - 键盘模拟: WM_KEYDOWN VK_DOWN → hSelected 移到下一个 phrase
  - SendInput mock: 不真发,record 到 test log,验证 unicode codepoint 正确
  - Add phrase: 在 m_phrases 末尾 push,SavePhrases() 写盘
  - Edit phrase: 修改 m_phrases[i].text + .category,SavePhrases() 写盘
  - Delete phrase: m_phrases erase,SavePhrases() 写盘
- **TestDefaultHotkeys** (扩): 加 Alt+. hotkey 断言
- **TestQuickPanelDialog** (扩): Phrase 按钮 now-op 触发 s_onPhrases (用 mock callback 验证)

## 12. 风险与 trade-off

| 风险 | 缓解 |
|---|---|
| SendInput 可能被 active app anti-cheat 拦截 | 已选 SendInput;后续 spec 可切 RIME commit API |
| YAML parser 简单实现容易 parse 失败 | 容错:parse 失败 → 空 list + log |
| Modal 在某些 IME 状态 (composing) 下行为 | SendInput 在 composing 中仍 commit 字符,跟 RIME commit 行为一致 |
| SysTreeView32 native 视觉跟 Liquid Glass chrome 不一致 | Chrome 用 custom Liquid Glass,内部 tree 用 native → "modern container, classic content" tradeoff,可接受 |
| Alt+. 全局热键可能跟其他 app 冲突 | RegisterHotKey 失败时 log warning,不 crash |
| Tree icon "📁" emoji 在老 Windows 字体可能显示成方块 | 备选:用纯文本 "▶"/"▼"(user L92 已用)或 line drawing chars |
| 未分类 phrase 放在分类行"下方"语义模糊 | user 原话是"直接展示在分类行的下面",我理解为 uncat 也是分类(名为"(未分类)"),但放在最后(所有具名分类之后) |

## 13. Ship 顺序 (3 个并行 track + 验收)

1. **Track 1 (设计)**: 本 spec 写完 + user 审核 + 通过
2. **Track 2 (实现) + Track 3 (绑定)**: spec 通过后,并行 dispatch
   - Track 2: 写 PhrasesDialog.{h,cpp} + xmake + 测试
   - Track 3: 改 QuickPanelDialog.cpp Phrase 按钮 + WeaselServerApp.cpp Alt+. 注册
3. **双验收**: Reality Checker (visual) + Test Results Analyzer (functional)
4. **Ship**: build-v0_19_0_25.py + commit (仅当双验收 PASS)

## 14. 反模式 / 经验 (从 L86-L94 累积)

- **AP-L94-A**: 几何 layout 修法需 paint↔hit-test 同步 — PhrasesDialog 内部子控件 hit-test 要正确
- **AP-L94-B**: L88-fix drag-any-area 后 state 残留 — 本 spec Phrase 按钮 click 分支需正确处理,不能 drag 启动覆盖
- **AP-L94-C**: auto-hide grace period — 不适用本 spec
- **AP-L94-D**: 多 issue 修复用多 agent 调研 + 双验收 — 本 spec 仍按 L94 流程
- **AP-L94-E**: brand vs btn 共享居中 1 px 错位 — 不适用本 spec
- **NEW (v0.19.0.25)**: 新功能 ship 前必**先 spec 写完 + user 审核**,不要直接 code → 沙箱验收(避免 user 反向反馈后回滚)

## 15. 文件位置摘要

- Spec: `.specify/specs/042-phrases-ui/spec.md` (本文件)
- Code: `WeaselServer/PhrasesDialog.{h,cpp}`
- Hook: `WeaselServer/QuickPanelDialog.cpp` (click 分支 +2 行)
- Hotkey: `WeaselServer/WeaselServerApp.cpp` (Alt+. 注册 +20 行)
- Build: `build-v0_19_0_25.py` (从 v0.19.0.24 clone)
- Install: `release/fluxing-0.19.0.25-installer.exe`
- Test: `test/TestPhrasesDialog/TestPhrasesDialog.cpp` (新建)