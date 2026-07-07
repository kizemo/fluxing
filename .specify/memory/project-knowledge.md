# Fluxing 项目知识手册（Project Knowledge Base）

> **用途**：跨会话上下文恢复。本文档是 Fluxing v2 (rime/weasel fork) 项目的事实速查手册，
> 涵盖架构、关键文件、配置覆盖关系、用户词典/常用短语机制、快捷键陷阱、yaml 编辑落地路径。
>
> **阅读对象**：接手这个项目的 AI agent 或新工程师。读完即可建立完整心智模型。
>
> **维护周期**：每次架构性变更后更新；每个 spec ship 后追加相关章节。
>
> **最后更新**：2026-07-07（spec 045 v0.18.29.0 ship + Q1-Q4 整理）

---

## 0. 项目基本事实

- **项目路径**：`F:\soft\00selfmade\rime`
- **当前分支**：`Fluxing`（推到 `kizemo/Fluxing`）
- **当前版本**：v0.18.29.0（spec 045 - QuickPanelDialog 8 入口）
- **最近 commit**：`9bc6905 docs(PRD+TDD): spec 041 verification §13/§14 + L53 spec 042+ 路线图`
- **用户安装路径**：`D:\Program Files\fluxing\`（用户原始部署，绝对不能改！）
- **用户数据路径**：`D:\Program Files\fluxing\user1\fluxing\`（用户词典 + 用户配置）
- **代码量**（fork 新增）：~1600 行 C++（7 个 FluxingComponents + QuickPanelDialog）
- **当前 spec**：000-011（基础架构）+ 033-041（DPI/部署）+ 045（QuickPanel）
- **docs**：
  - `.specify/PRD.md` - 产品需求（5 P1 + 3 P2 + 4 P3）
  - `.specify/TDD.md` - 测试策略
  - `.specify/memory/constitution.md` - 5 原则 + 9 硬规则
  - `.specify/memory/lessons-learned.md` - L01-L54 复盘记录
  - `.specify/specs/NNN-*/spec.md|plan.md|tasks.md` - 每个 spec 的三级文档

---

## 1. 仓库目录速查

| 目录 | 用途 | 备注 |
|---|---|---|
| `WeaselServer/` | TSF 服务进程 + tray icon + QuickPanel | 核心修改点 |
| `WeaselTSF/` | Text Services Framework 实现（Windows IME 前端） | librime 上游版，不修改 |
| `WeaselDeployer/` | 部署 GUI + 字典管理 + 偏好设置 | 现有 UI 参考 |
| `WeaselUI/` | 候选框 UI + FluxingComponents 控件库 | 二次开发重点 |
| `WeaselUI/FluxingComponents/` | 7 个 mac 风控件（Button/Toggle/Panel/Label/D2DRenderer/FluxingTheme/stdafx） | 已可用 |
| `WeaselIPC/` `WeaselIPCServer/` | 进程间通信（PipeChannel） | 上游版 |
| `RimeWithWeasel/` | librime API 适配层（WeaselServer → librime） | 本次新增接口：GetCurrentSchemaId / GetAvailableSchemas / SelectSchema / IsSimplification / IsFullShape |
| `include/` | 公共头文件 | `RimeWithWeasel.h` 在此 |
| `librime/` | **git submodule**，librime C API | 不修改（只能 vendored plugin） |
| `thirdparty/librime-lua/` | librime-lua 插件源码（vendored） | 通过 `scripts/prepare-librime-lua.bat` 复制到 librime |
| `output/data/` | **部署配置目录**（只读，升级会被覆盖） | default.yaml / weasel.yaml / *.schema.yaml / custom_phrase.txt / *.dict.yaml / cn_dicts/ / opencc/ |
| `output/Win32/` | 编译产物（WeaselServer.exe / rime.dll / etc） | x86 only |
| `FluxingConfigEditor/` | yaml-cpp 封装（spec 024） | `YamlRoundTrip.{h,cpp}` 保留 key order |
| `scripts/test-infra/` | 测试基础设施 | `run-test-suite.bat` |
| `test/` | 单元测试 | `TestDefaultHotkeys/`, `TestResponseParser/`, `TestWeaselIPC/`, `TestFluxingComponents/`, `TestQuickPanelDialog/`, `TestQuickPanelRefactor/`, `TestYamlRoundTripE2E/` |
| `release/` | 历史 installer binary（git tracked） | `fluxing-<X.Y.Z.W>-installer.exe` |
| `output/archives/` | 最新 build 的 installer | 同名 |
| `output/install.nsi` | NSIS 安装脚本（带 BOM 的 UTF-8） | L09: 必须有 BOM EF BB BF；L13: ForceFluxingSuffix 在 silent 模式不触发 |

---

## 2. 三层部署架构（绝对不能混淆）

```
┌─────────────────────────────────────────────────────────┐
│ D:\Program Files\fluxing\weasel\            (部署目录)  │
│   ├── WeaselServer.exe  ← WeaselServer.cpp 编译产出    │
│   ├── WeaselDeployer.exe ← WeaselDeployer.cpp 编译产出  │
│   ├── weasel.dll        ← WeaselIPC 实现                │
│   ├── rime.dll          ← librime 编译产出 (x86)         │
│   ├── WinSparkle.dll / weaselx64.dll / 7z.dll / curl.exe │
│   └── data/   ← output\data\ 部署后的配置              │
│       ├── default.yaml          (全局设置)              │
│       ├── weasel.yaml           (UI 样式)               │
│       ├── *.schema.yaml         (方案定义)              │
│       ├── custom_phrase.txt     (常用短语)              │
│       └── build/*.table.bin     (deploy 编译产物)       │
└─────────────────────────────────────────────────────────┘
         ↓ WeaselServer 运行时读写 ↓
┌─────────────────────────────────────────────────────────┐
│ D:\Program Files\fluxing\user1\fluxing\    (用户数据)   │
│   ├── user.yaml              ← RIME 自动写（运行时状态） │
│   ├── installation.yaml      ← installer 写（安装元数据）│
│   ├── default.custom.yaml    ← UI 编辑写（用户覆盖）    │
│   ├── weasel.custom.yaml     ← UI 编辑写（UI 样式覆盖） │
│   ├── *.custom.yaml          ← UI 编辑写（方案覆盖）    │
│   ├── rime_ice.userdb/       ← librime 自动写（LevelDB）│
│   └── build/                 ← deploy 编译产物（缓存）  │
└─────────────────────────────────────────────────────────┘
         ↓ HKLM/HKCU 注册表 ↓
┌─────────────────────────────────────────────────────────┐
│ HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel                │
│   └── InstallDir = D:\Program Files\fluxing\weasel      │
│ HKCU\Software\Fluxing\Weasel                            │
│   └── RimeUserDir = D:\Program Files\fluxing\user1\fluxing│
│ HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing│
│   └── DisplayVersion = 0.18.29.0                        │
└─────────────────────────────────────────────────────────┘
```

**绝对禁止**：
- ❌ 把任何数据写到 `C:\Program Files\fluxing\`（用户 D 盘是真实安装）
- ❌ 改用户数据目录（`D:\Program Files\fluxing\user1\`）
- ❌ silent install 不带 `/D=` 参数（NSIS InstallDirRegKey 会用注册表值）

---

## 3. 配置文件覆盖关系（RIME 标准机制）

### 3.1 目录角色

| 目录 | 角色 | 谁写 | 升级影响 |
|---|---|---|---|
| `<shared_data_dir>/data/*.yaml` | **default 配置**（不可改） | installer 部署 | 升级覆盖 |
| `<user_data_dir>/*.custom.yaml` | **用户覆盖**（patch 段） | UI / RimeLeversApi | 升级保留 |
| `<user_data_dir>/user.yaml` | **运行时状态** | librime 自动 | 升级保留 |
| `<user_data_dir>/installation.yaml` | **安装元数据** | installer | 升级保留 |
| `<user_data_dir>/<schema>.userdb/` | **用户词典**（LevelDB） | librime 自动 | 升级保留 |
| `<user_data_dir>/build/` | **编译缓存** | WeaselDeployer /deploy | 升级可重建 |

### 3.2 覆盖机制

`librime\src\rime\lever\custom_settings.cc`：

1. `Load()` 先读 `<config_id>.yaml`（default），再读 `<config_id>.custom.yaml`（用户）
2. `Customize(key, item)` 把 item 写入 `custom_config_` 的 `patch` map
3. `Save()` 用 `Signature.Sign` 加签名后写到 `<user_data_dir>/<config_id>.custom.yaml`
4. RIME 启动时 deep merge：default + patch → 最终生效配置

### 3.3 用户当前 D 盘状态

`default.custom.yaml`（425 bytes）：
```yaml
customization:
  distribution_code_name: Fluxing
  distribution_version: VERSION_MAJOR.VERSION_MINOR.VERSION_PATCH
  generator: "Rime::SwitcherSettings"
  modified_time: "Mon Jul  6 21:56:59 2026"
  rime_version: 1.13.1
patch:
  schema_list:
    - {schema: rime_ice}
    - {schema: double_pinyin_sogou}
    - {schema: double_pinyin_flypy}
    - {schema: double_pinyin_ziguang}
    - {schema: double_pinyin_jiajia}
```
**注意**：当前只有 schema_list patch，**没有 key_binder 之类的快捷键 patch**。用户后续 UI 改的快捷键会写到这里。

---

## 4. 用户词典 vs 常用短语（两个完全不同的机制）

### 4.1 常用短语（custom_phrase）

| 字段 | 值 |
|---|---|
| 源文件 | `output\data\custom_phrase.txt`（部署到 `weasel\data\custom_phrase.txt`） |
| 格式 | `词语<TAB>编码<TAB>权重`（如 `火流猩\trime_ice\t1`） |
| 编译 | `WeaselDeployer /deploy` 编译成 `data\build\custom_phrase.table.bin` |
| schema 引用 | `rime_ice.schema.yaml`：`table_translator@custom_phrase` |
| 编辑方式 | 直接编辑 `custom_phrase.txt` 然后重新 deploy |
| 源码 | `librime\src\rime\dict\table_translator.cc` + `table_db.cc` |
| 用户能否 UI 编辑 | ✅ 直接编辑 txt 文件，无需特殊 API |

### 4.2 用户词典（user dictionary）

| 字段 | 值 |
|---|---|
| 存储目录 | `<user_data_dir>/rime_ice.userdb/`（LevelDB） |
| 关键文件 | `000005.ldb`, `MANIFEST-000007`, `LOG`, `LOG.old`, `LOCK` |
| RIME 实现 | `librime\src\rime\dict\user_dictionary.h:49 class UserDictionary` |
| CRUD API（librime C++ 内部） | `UpdateEntry(entry, commits, new_entry_prefix)` |
| 自动写入路径 | `librime\src\rime\gear\memory.cc:128 Memory::OnDeleteEntry` |
| 公开 C API（levers） | `librime\src\rime\lever\levers_api_impl.h` |
| **公开 C API 只提供** | `user_dict_iterator_init / next_user_dict / backup / restore / export / import` |
| **公开 C API 不提供** | 逐条 entry 的增删查 |
| 但完整 CRUD 在 librime 内部 | `UserDictionary::UpdateEntry(DictEntry, commits=±N, prefix)` |

**value 格式**（`user_db.h:22-32 UserDbValue`）：
```
c=<commits> d=<dee> t=<tick>
```

### 4.3 TSV Export 格式（`librime\tools\rime_dict_manager.exe -e`）

实测 D 盘 `rime_ice.userdb` dump：

```
# Rime user dictionary export
#@/db_name	rime_ice
#@/db_type	userdb
#@/rime_version	1.13.1
#@/tick	244
#@/user_id	0c436c21-ce78-47ca-860b-0797cde5f345
鎸夐挳	an niu	1
鍚?ba	1
鍖呭惈	bao han	1
...
```

每行 `phrase<TAB>code<TAB>commits_count`，commit_count 表示上屏次数。

### 4.4 UI 编辑用户词典的实现路径（3 种）

**路径 A: Export → 编辑 → Import（最可靠，标准 RIME 上游做法）**

```cpp
// 1. 导出到 TSV
RimeLeversApi* api = (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();
api->export_user_dict("rime_ice", "D:\\TEMP\\dump.tsv");

// 2. UI 解析 TSV（跳过以 # 开头的 metadata 行）
//    解析每行 phrase\tcode\tcommits 到 list/table
//    用户编辑（增删改查某条）
//    写回新的 TSV 文件

// 3. 重新导入（覆盖整个 userdb）
api->import_user_dict("rime_ice", "D:\\TEMP\\edited.tsv");
```

**优点**：
- 完全用 librime 官方公开 API（无需改 librime）
- 简单可靠，已经在 RIME 生态广泛使用（ibus-rime, fcitx-rime 都这么做）
- TSV 格式是 librime `rime_table_entry_formatter` 标准输出
- Commit 计数（commits 字段）保留，排序权重不变

**缺点**：
- 必须读写整个 userdb（不能在增量编辑时不影响其他用户）
- 需要停掉 librime 写 userdb 的路径（否则 Export 时 LevelDB 被 LOCK，会失败；L54 lesson）
- import 时整个 userdb 重建，丢失 ticks（实际上保留在 metadata 行）

**路径 B: 调用内部 UserDictionary API（C++ 模块化方案）**

需要新增 `librime_levers_api` 函数，绕过现有 levers 接口：
```cpp
// user_dict_manager.cc 加：
bool CommitEntry(const string& dict_name, const DictEntry& entry, int commits);
bool DeleteEntry(const string& dict_name, const string& code, const string& text);
an<DictEntryList> LookupEntries(const string& dict_name, const string& input);
```
然后通过 levers API 暴露。

**优点**：增量更新、不需要停写、保留 ticks
**缺点**：要修改 librime + 重新 build + 维护成本

**路径 C: 利用现有 `delete_candidate` 间接实现**

`RimeAPI::delete_candidate(session_id, index)` 走 `Memory::OnDeleteEntry` 路径，最终调 `user_dict_->UpdateEntry(entry, -1)`，可以删除候选词。但是：
- 只能删除当前菜单可见的候选
- 不能新增（新增必须通过实际输入上屏）
- 不能查（已有 lookup 在 RimeContext 里）

**适用**：简单的"删除我刚选错的词"场景，不适合通用编辑器。

### 4.5 推荐方案

**首选 路径 A**（Export/Import）。这是 RIME 生态标准做法，不需改 librime。

**核心难点**：`export_user_dict` 调用需要 leveldb 不被 LOCK。运行时 WeaselServer 持有 LOCK，必须：
- 在 WeaselServer 进程内调用（同一进程的 lever）
- 或者通过 IPC 让 WeaselServer 代为调用
- 或者停 WeaselServer（用户已表示不要弹安装界面，所以不要重启）

### 4.6 字典管理 UI 的 WeaselDeployer 现状

`WeaselDeployer/DictManagementDialog.cpp` **已经**有：
- 用户词典列表（`user_dict_iterator_init`）
- backup（`backup_user_dict`）
- restore（`restore_user_dict`）
- export（`export_user_dict`）
- import（`import_user_dict`）

**但只有导入导出，没有逐条编辑**。这就是为什么 cursor 之前的实现需要写自己的 UI。

---

## 5. 快捷键陷阱（Shift 单键切中英）

### 5.1 当前默认设置（已 ship 0.18.29.0）

`output\data\default.yaml`：

```yaml
ascii_composer:
  good_old_caps_lock: true
  switch_key:
    Caps_Lock: clear
    Shift_L: noop        # 火流猩 v2: 避免 ascii_composer 接管
    Shift_R: noop
    Control_L: noop
    Control_R: noop

key_binder:
  bindings:
    - { when: composing, accept: Shift+Tab, send: Shift+Left }
    - { when: composing, accept: Tab, send: Shift+Right }
    - { when: composing, accept: Alt+Left, send: Shift+Left }
    - { when: composing, accept: Alt+Right, send: Shift+Right }
    - { when: has_menu, accept: comma, send: Page_Up }
    - { when: has_menu, accept: period, send: Page_Down }
    - { when: has_menu, accept: Shift+Shift_L, send: 2 }
    - { when: has_menu, accept: Shift+Shift_R, send: 3 }
    - { when: has_menu, accept: Control+1, send: 2 }
    - { when: has_menu, accept: Control+2, send: 3 }
    - { when: always, toggle: ascii_punct, accept: Control+Shift+9 }
    - { when: always, toggle: ascii_mode, accept: Shift+space }   # 当前切中英
    - { when: always, toggle: ascii_punct, accept: Control+Shift+numbersign }
    - { when: always, toggle: traditionalization, accept: Control+Shift+0 }
    - { when: always, toggle: traditionalization, accept: Control+Shift+dollar }
```

### 5.2 librime KeyEvent 实现（`librime\src\rime\key_event.h:52`）

```cpp
bool operator==(const KeyEvent& other) const {
  return keycode_ == other.keycode_ && modifier_ == other.modifier_;
}
```

**严格比较 keycode + modifier**。modifier 在 `key_table.h`：
- `kShiftMask = 0x01`
- `kReleaseMask = 0x80`

### 5.3 历史事故（L18 / L19 / L21）

见 `default.yaml` 注释：

- **L18**：用户反馈 shift+Enter / shift+<letter> release 事件在 0.18.5.0 仍触发 ascii_mode 切换
- **L19**：移除 `has_menu: Shift+Shift_L/R` binding，候选选择改用 `Control+1/2`
- **L21**：恢复 L19 误删的 binding（用 `Shift+Shift_L/R` 形式，modifier=Shift，确保 keycode=Shift_L, modifier=0 的 TSF release event 不会误匹配）

**根因**：TSF 的 release event 误匹配。librime 1.13 在某些 TSF 实现下解析有缺陷。

### 5.4 解决方案（如果想改回 Shift 单键切中英）

**方案 A（推荐）：保持 Shift+space 现状**

最稳，已 ship 0.18.5+，L18 验证过。

**方案 B：改用其他非 Shift 修饰键**

```yaml
- { when: always, toggle: ascii_mode, accept: Control+space }     # Ctrl+空格
- { when: always, toggle: ascii_mode, accept: Caps_Lock+Shift_L }  # 双键组合
- { when: always, toggle: ascii_mode, accept: grave }               # ` 反引号单键
```

**方案 C：在 WeaselTSF 层拦截 release events**

librime 不能改，但 WeaselTSF 可以拦截 modifier-only key 的 release event：

```cpp
// WeaselTSF/WeaselTSF.cpp ITfKeyEventSink::OnKeyUp
if (keyCode == VK_SHIFT && (lParam & KF_UP)) {
  return S_OK;  // 拦截 Shift release，不传给 librime
}
```

需要改 WeaselTSF + 加回归测试。

---

## 6. yaml 配置编辑落地的两条路径

### 6.1 路径 1：用 RimeLeversApi（推荐）

**写文件位置**：`<user_data_dir>/<config_id>.custom.yaml`

```cpp
#include <rime_levers_api.h>

RimeLeversApi* api = (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();

// 打开 default.yaml + default.custom.yaml
RimeCustomSettings* settings = api->custom_settings_init("default", "Fluxing::SettingsEditor");
api->load_settings(settings);

// 读 / 改（注意：levsers API 用 / 分隔，不是 .）
RimeConfig cfg = {0};
api->settings_get_config(settings, &cfg);
// ... 用 RimeConfigIterator 遍历读 ...
api->customize_string(settings, "key_binder/bindings/3/accept", "Shift+space");

// 保存（自动加 Signature）
api->save_settings(settings);
api->custom_settings_destroy(settings);
```

**优点**：
- 自动加 Signature（`custom_settings.cc:51-55`）
- 不污染部署目录（升级保留）
- 自动 deep merge

**缺点**：
- key separator 用 `/`（不是 `.`）
- 不支持 sequence index（`bindings[i]` 写不了）
- 必须用现有 scheme（不能新增）

### 6.2 路径 2：用 FluxingConfigEditor/YamlRoundTrip

**写文件位置**：直接读写 `<user_data_dir>/<config_id>.custom.yaml`

```cpp
#include <YamlRoundTrip.h>

// 读
std::string content = ReadFile("D:\\Program Files\\fluxing\\user1\\fluxing\\default.custom.yaml");
fluxing::YamlDocument doc;
fluxing::Load(content, &doc);

// 修改（用 . 分隔，支持 sequence index）
fluxing::WriteString(&doc, "patch.key_binder.bindings.3.accept", "Shift+space");

// 保存（保留 key order）
std::string out;
fluxing::Save(doc, &out);
WriteFile("D:\\Program Files\\fluxing\\user1\\fluxing\\default.custom.yaml", out);

// 触发 deploy 让 RIME 重读
ShellExecuteW(NULL, NULL, L"WeaselDeployer.exe", L"/deploy", NULL, SW_SHOWNORMAL);
```

**优点**：
- 保留 key order（spec 024 核心价值）
- 支持 sequence index
- 可以新增 key / 删除 key

**缺点**：
- 不写 Signature block（librime 会认为不是 trusted source，下次 deploy 会被覆盖）
- 必须手动 trigger `/deploy`

### 6.3 推荐组合

| 任务 | 用什么 | 触发动作 |
|---|---|---|
| 改快捷键（key_binder） | YamlRoundTrip + 写 default.custom.yaml | `/deploy` |
| 改 ascii_composer 行为 | YamlRoundTrip + 写 default.custom.yaml | `/deploy` |
| 改 weasel 样式（字体/配色） | RimeLeversApi customize_*（"weasel" config） | 即时生效 |
| 编辑常用短语 | 直接编辑 custom_phrase.txt | `/deploy` |
| 编辑用户词典（增删查） | Export → 编辑 → Import（TSV） | `/deploy` |

---

## 7. QuickPanelDialog 状态（v0.18.29.0）

### 7.1 当前实现

`WeaselServer/QuickPanelDialog.{h,cpp}` 实现 8 入口 mac 风面板：

```
行 1: ⚙ Fluxing                                          [X]
行 2: [中/英 toggle]  [简/繁 toggle]  [全/半角 toggle]
行 3: 当前方案: <schema>                              [切换]
行 4: [📁 用户文件夹]  [📂 程序文件夹]
行 5: [🚀 部署]  [⏻ 退出]
```

### 7.2 Show() 签名（11 个参数）

```cpp
static void Show(bool currentAscii, bool currentSimp, bool currentFullwidth,
                 const std::wstring& currentSchema,
                 const std::vector<std::wstring>& availableSchemas,
                 std::function<void(bool)> onAsciiToggle,
                 std::function<void(bool)> onSimpToggle,
                 std::function<void(bool)> onFullwidthToggle,
                 std::function<void(const std::wstring&)> onSelectSchema,
                 std::function<void()> onOpenUserFolder,
                 std::function<void()> onOpenProgramFolder,
                 std::function<void()> onDeploy,
                 std::function<void()> onQuit);
```

### 7.3 触发方式

`Alt+,`（注册到 WeaselTSF 全局热键）或左键点击任务栏托盘图标。

### 7.4 已知 caveat

PID 2060 WeaselServer 是 PPL（Protected Process Light）守护进程，taskkill 无法终止。
- 部署新 binary 后，**必须用户注销或重启**才能加载新代码（L17 / L18 lesson）
- 切换按钮（"切换"）当前是 cycle-to-next 行为，spec 046 会替换为真正的方案选择 popup list

---

## 8. FluxingComponents 控件库

`WeaselUI/FluxingComponents/` 7 个控件（mac 风，D2D + DirectWrite 渲染）：

| 控件 | 头文件 | 关键方法 |
|---|---|---|
| FluxingButton | Button.h | `Create(parent, rect, label, Style::{Primary/Secondary/Destructive})` + `SetOnClick` |
| FluxingToggle | Toggle.h | `Create(parent, rect, initial)` + `SetOnChanged(bool on)`（带 200ms slide 动画） |
| FluxingPanel | Panel.h | `Create(parent, rect, Style::{Card/Plain})`（8px rounded） |
| FluxingLabel | Label.h | `Create(parent, rect, text, FontSize::{Small/Medium/Large})` |
| FluxingD2DRenderer | D2DRenderer.h | 内部使用，UI 层不直接调 |
| FluxingTheme | FluxingTheme.h | 主题（dark/light + 自定义色板） |
| stdafx / targetver | | `_WIN32_WINNT_WIN10` (GetDpiForWindow 可用) |

**已知问题**：
- 144 DPI 下需要 `Clear()` 防 backing store 透黑（L52）
- 必须包含 d2d1.h / dwrite.h 顺序：`unknwn.h` → `d2d1.h` → `dwrite.h` → `wrl/client.h`（L47）

---

## 9. 编译与构建

### 9.1 三条命令（按使用频率排序）

| 命令 | 工具 | 用时 | 用途 |
|---|---|---|---|
| `xbuild.bat weasel installer` | xmake | 30-90s | 日常 inner loop |
| `xbuild.bat installer` | xmake + NSIS | 60-120s | 生成 installer |
| `build.bat all` | msbuild end-to-end | 30-60min | 首次 / 每月 hygiene |

### 9.2 环境

- VS 2022 Build Tools + vcvars32.bat
- Boost 1.84+ at `F:\b183`
- xmake 2.9.4+ at `F:\soft\08tools\xmake\xmake.exe`
- NSIS 3.x at `C:\Program Files (x86)\NSIS\`
- Python 3.10+ (plum / get-rime)

### 9.3 测试

```batch
:: Build + run unit tests
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe
test\TestResponseParser\Release\TestResponseParser.exe
test\TestWeaselIPC\Release\TestWeaselIPC.exe
test\TestYamlRoundTripE2E\Release\TestYamlRoundTripE2E.exe
```

或者用 `scripts/test-infra/run-test-suite.bat`。

---

## 10. NSIS Installer 陷阱（L09 / L13 / L14 / L17 / L52）

- **L09**：NSIS 脚本必须 UTF-8 BOM（`EF BB BF`）+ 100% CRLF
- **L13**：`ForceFluxingSuffix` 在 silent 模式（`/S` + `/D=`）不触发；silent 模式必须显式传 `/D=<正确路径>`
- **L14**：所有 Weasel*.exe 是 x86（`output\Win32\`），`weaselx64.dll` 是唯一 x64 文件（TSF shim）
- **L17**：`InstallDirRegKey` directive 在 .onInit **之前**就把 `$INSTDIR` 设成注册表值，`/D=` 会被忽略（除非注册表为空）
- **L52**：DPI handling 必须在每个控件都加 `Clear()` + WM_DPICHANGED handler
- **L54**（最新）：silent install 之前必须先清注册表 `HKLM\Software\Fluxing\Weasel`；NSIS `uninst` 函数不会清理 InstallDir pollution，导致每次 install 都用错误的旧路径

---

## 11. 常用命令速查

```powershell
# 查注册表安装路径
Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -ErrorAction SilentlyContinue
Get-ItemProperty "HKCU:\Software\Fluxing\Weasel" -ErrorAction SilentlyContinue

# 查所有 WeaselServer 进程 + 来源
Get-CimInstance Win32_Process -Filter "Name='WeaselServer.exe'" |
  Select-Object ProcessId, ExecutablePath, CommandLine

# 强制杀 WeaselServer（普通 taskkill 被 PPL 拒绝）
cmd /c "taskkill /F /IM WeaselServer.exe"  # PPL 进程会失败

# 卸载 D 盘（保留 user1 数据）
cmd /c 'rmdir /S /Q "D:\Program Files\fluxing\weasel\data"'
cmd /c 'rmdir /S /Q "D:\Program Files\fluxing\weasel"'
# user1 保留

# 重新 install 到 D 盘（先清注册表 + 杀进程）
Remove-Item "HKLM:\SOFTWARE\WOW6432Node\Fluxing" -Recurse -Force
Remove-Item "HKCU:\Software\Fluxing" -Recurse -Force
cmd /c "`"$installer`" /S /D=`"D:\Program Files\fluxing`""

# 验证 PE 架构
$b = [System.IO.File]::ReadAllBytes("path\to\WeaselServer.exe")
$peOff = [BitConverter]::ToInt32($b[0x3C..0x3F], 0)
[BitConverter]::ToUInt16($b[($peOff+4)..($peOff+5)], 0)  # 0x14C=x86, 0x8664=x64

# Dump user dict 为 TSV
cmd /c "cd /d ""D:\Program Files\fluxing\weasel"" && rime_dict_manager.exe -e rime_ice D:\TEMP\dump.tsv"
```

---

## 12. 参考链接

- librime 上游：https://github.com/rime/librime
- RIME wiki：https://github.com/rime/home/wiki
- weasel 上游：https://github.com/rime/weasel
- 雾凇拼音方案：https://github.com/iDvel/rime-ice
- Fluxing fork: https://github.com/kizemo/fluxing

---

## 13. 后续 spec 路线图（2026-07-07）

| 优先级 | spec | 内容 | 估计 ship |
|---|---|---|---|
| P0 | spec 046 | 切换按钮替换为真正的方案选择 popup list | v0.18.30.0 |
| P1 | spec 047 | 修复 PPL 进程检测文件替换 + 自动退出 | v0.18.31.0 |
| P2 | spec 048 | 用户词典 UI 编辑器（路径 A: Export/Import） | v0.18.32.0 |
| P3 | spec 049 | 常用短语 UI 编辑器（直接编辑 custom_phrase.txt） | v0.18.33.0 |
| P4 | spec 050 | 偏好设置 UI（改 key_binder / ascii_composer） | v0.18.34.0 |
| P5 | spec 051 | yaml 可视化配置 UI（spec 007） | v0.18.35.0+ |
| P6 | spec 052 | Alt+K 短语 | 未来 |
## 附录 A：从 rimetrae cursor 项目学到的实战经验（2026-07-07）

> 来源：F:\soft\02office\rimetrae\weasel\（cursor 之前二次开发的完整工作副本）。
> cursor 在 2026-06 用 2 天时间独立完成了所有 UI + 后端 + 部署，build 产物完整保留在 msbuild/Release/Win32/。
> 我们要学习但不复用——把他的设计思想提炼出来，按 rime/weasel 现有架构重新实现。

### A1. cursor 实现的全景（已经 100% 编译通过的组件）

**WeaselDeployer 新增 5 文件**（F:\soft\02office\rimetrae\weasel\WeaselDeployer\）：

| 文件 | 行数 | 作用 |
|---|---|---|
| CustomPhraseListDialog.{h,cpp} | ~984 | 主列表 UI，可查看/增删改/导入导出，区分固定短语和学习词条 |
| CustomPhraseDialog.{h,cpp} | ~中等 | 单条编辑对话框（编码 + 短语） |
| CommonPhraseEditDialog.{h,cpp} | ~中等 | 批量编辑入口（startup_mode 参数） |
| HotkeySettingsDialog.{h,cpp} | ~中等 | 快捷键设置面板（KeyCaptureEdit 抓键） |
| KeyCaptureEdit.{h,cpp} | ~中等 | 单个按键抓取控件（替换原来的 EDIT 控件） |

**WeaselServer 新增 1 文件**：

| 文件 | 作用 |
|---|---|
| TrayCommonPhrasePanel.{h,cpp} | 托盘弹出短语面板（独立短语列表，键盘钩子支持） |

**include 新增 1 文件**：

| 文件 | 作用 |
|---|---|
| CommonPhraseStore.h | 独立短语面板的数据 store，存 weasel_common_phrases.txt |

**WeaselDeployer 新增 1 header-only 库**：

| 文件 | 作用 |
|---|---|
| DeployerUiHelper.h | 高 DPI / 暗色标题栏 / 窗口几何持久化 / 字体 / 焦点抓取（inline 函数库） |


### A2. cursor 实现 UI 编辑用户词典 的真实机制（修正之前的错误判断）

**重要修正**：用户说之前 cursor 实现了 UI 编辑用户词典和常用短语——经查证，cursor **没有**实现用户词典（rime_ice.userdb leveldb）的逐条 UI 编辑。**它只实现了常用短语（custom_phrase.txt）的 UI 编辑**。这与我们 spec 048（P2 用户词典 UI 编辑器）是不同的事。

具体三步流程（参考 CustomPhraseListDialog::SaveEntries 实现，约 603-635 行）：

`
1. UI 编辑（OnAdd/OnEdit/OnDelete/OnApplyEdit）
   ↓ 修改内存中的 custom_entries_ vector<CustomPhraseEntry>{code, phrase, source_file, learned:false}>
2. SaveEntries():
   a. 把 custom_entries_ 写回 custom_phrase.txt 和 custom_phrase_double.txt：
      - header: # Rime table\n# coding: utf-8\n#@/db_name\tcustom_phrase\n#@/db_type\ttabledb\n...\n
        （custom_phrase_double.txt 时 db_name = custom_phrase_double）
      - body: phrase<TAB>code 逐行（用 wtou8 转 UTF-8）
   b. 调用 rime_get_api()->deploy(); // 让 WeaselDeployer 触发 RIME 重新编译 .bin
   c. RefreshServerUserDictSnapshot():  // ~行 130-134（匿名 namespace）
        weasel::Client client; client.EndMaintenance(); client.StartMaintenance();
        // 通过 IPC 通知 WeaselServer 释放 userdb lock + 重新加载快照（顺序反着的技巧）
3. ReloadList() 刷新 UI（PopulateListBox + UpdateStatsLabel + UpdateDetailPanel）
`

**关键限制**（cursor 也没绕过去）：

- 学习词条（来自 rime_ice.userdb 的 leveldb）**不能**在 UI 中编辑，只能查看 + 删除——必须通过输入时右键候选词触发 Memory::OnDeleteEntry（删除 commit 计数）或导出为 custom_phrase 后再编辑。cursor 在 CustomPhraseListDialog::UpdateDetailPanel 里检测 IsLearnedSelection(index)，如果是 learned entry 就禁用编辑控件并提示请在输入时通过候选词右键删除。
- Cursor 用 looks_like_code() 函数自动判断左右列谁是编码谁是短语（编码必须是字母数字下划线 + 短横线 + 撇号 + 空格），避免用户填反了。ParsePhraseLineImpl（行 40-80）就靠这个判断。
- custom_phrase.txt header **必须**包含 #@/db_name\tcustom_phrase 才能被 RIME 识别——cursor 在 PhraseFileHeader(filename) 里强制写。删掉这行 RIME 会忽略整个文件。
- GetPhraseFileName()（行 132-153）智能选择：优先用 custom_phrase_double.txt（如果 default.custom.yaml 里写了 double_pinyin 方案），否则用 custom_phrase.txt。


### A3. cursor 实现的两套短语系统（关键区分！）

| 维度 | 常用短语（custom_phrase.txt） | 独立短语（weasel_common_phrases.txt） |
|---|---|---|
| **数据载体** | RIME 内置 tabledb | 纯文本，每行一条 |
| **触发方式** | 输入编码（如 oa） | 直接选（无编码） |
| **关联文件** | user_dir/custom_phrase.txt | user_dir/weasel_common_phrases.txt |
| **管理 UI** | CustomPhraseListDialog | TrayCommonPhrasePanel（托盘面板） |
| **调用引擎** | RIME deploy() 重新编译 | 不需要，纯文本读写 |
| **用途** | oa → 常用语 快捷触发 | 即时贴 无编码短语 |

**对我们的启发**：

- spec 049（常用短语 UI 编辑器）= cursor 的 CustomPhraseListDialog 三件套（CustomPhraseListDialog + CustomPhraseDialog + CommonPhraseEditDialog）——**完整可用**。但 spec 049 当前是 P3，建议提升为 **P1 spec 046**，因为它 1-2 天就能 ship，是 spec 048 的良好过渡。
- cursor 的 TrayCommonPhrasePanel（独立短语面板）+ CommonPhraseStore（数据 store）我们**不需要**——已经被 QuickPanelDialog（spec 045，8 入口 mac 风格）替代，而且独立短语在 RIME 内没有触发入口，没用户场景。**建议从路线图里删除 spec 049 的独立短语部分**。

### A4. cursor 的 DeployerUiHelper.h 设计（强烈推荐复用）

这是个**纯 header 库**（7.7KB，372 行，inline 函数），核心 8 个工具（按使用频率）：

`cpp
namespace deployer_ui {
  // 7.7KB, ~372 行, 纯 inline, 零编译开销, 零链接依赖（除 shcore.lib/Dwmapi.lib 已通过 pragma 引入）
  constexpr wchar_t kUiRegistryKey[] = L" Software\\\\Rime\\\\Weasel\\\\UI\; // 几何持久化用此 key
 constexpr int kTitleBarHeightDp = weasel_ui::kTitleBarHeightDp; // 32dp

 inline void EnableDarkTitleBar(HWND hwnd); // DWMWA_USE_IMMERSIVE_DARK_MODE = 20, BOOL=TRUE
 inline UINT GetDpiForWindow(HWND hwnd); // GetDpiForMonitor(MDT_EFFECTIVE_DPI) + GetDeviceCaps fallback
 inline int Scale(int v, UINT dpi); // MulDiv(v, dpi, 96)
 inline HFONT CreateUiFont(int pt, UINT dpi, bool bold = false); // Microsoft YaHei UI, 负 height
 inline void BringDialogToFront(HWND hwnd); // AttachThreadInput(cur_tid, fg_tid, TRUE/FALSE) 技巧
 inline void EnableResizableFrame(HWND hwnd); // WS_THICKFRAME | WS_MAXIMIZEBOX, 清 DS_MODALFRAME
 inline bool LoadWindowGeometry(...); // HKCU\Software\Rime\Weasel\UI\<key>X/Y/W/H (REG_DWORD)
 inline bool SaveWindowGeometry(...); // 持久化到同一注册表位置
}
`

**优点**：

- header-only，零编译开销，零链接依赖
- 高 DPI 全套工具，比单文件 Scale() 强很多
- 暗色标题栏一句调用（DWMWA 20 = Win10 1903+）
- 几何持久化自动写到注册表（不是文件，避免 %APPDATA% 路径问题）
- 字体用 Microsoft YaHei UI（中文环境默认字体）

**移植路径**：直接 copy 到 WeaselDeployer/DeployerUiHelper.h（与 cursor 一致），所有现有 Dialog 改用它替换当前散落的代码。**预计可让我们省的 30% UI 代码**——DictManagementDialog、UIStyleSettingsDialog、SwitcherSettingsDialog 都能受益。

### A5. cursor 的 UI 渲染技术栈选择（避免我们再踩 D2D 坑）

cursor 用的：

- **CDialogImpl + WTL**（不是 MFC）→ 与 weasel 现有风格一致
- **GDI**（HDC + SetBkColor/SetTextColor）→ 与现有 DictManagementDialog 一致
- **DWMAPI** → 暗色标题栏
- **shcore.lib** → GetDpiForMonitor
- **注册表 HKCU\Software\Rime\Weasel\UI** → 窗口几何持久化
- **未用 Direct2D / DWrite** → 简化构建

**对我们的启发**：cursor 没用 D2D 是对的——QuickPanelDialog（spec 045）之前用 D2D 在 144 DPI 上有 layout 错位问题（spec 041 修复过）。**新 UI 都走 GDI + DeployerUiHelper**，避免再踩 D2D 的坑。WeaselUI/FluxingComponents/ 里的 D2D 控件（Button/Toggle/Label/Panel）只用于托盘弹出层，不要混进 Dialog 里。

### A6. cursor 解决 shift 快捷键问题的核心代码

cursor 在 Configurator::EnsureDeployAsciiConfig() 里写了 deploy 钩子。每次 deploy 时强制把所有 Shift 绑定打成 noop（三个文件全部覆盖：weasel_hotkeys.yaml + default.yaml + default.custom.yaml）。TSF 层 _ToggleAsciiMode 处理单击 Shift，RIME 层不做任何反应——避免 spec 041/L43/L46 修的 shift 其它键误触问题再次发生。

覆盖的 5 种 mode：commit_code / inline_ascii / set_ascii_mode / unset_ascii_mode / clear，全部打成 noop。

如果 hotkeys 文件没有 Control+Space 自动加上 key_binder/bindings/+ 段：
`
key_binder/bindings/+:
  - { when: always, toggle: ascii_mode, accept: Control+space }
`

**对我们的启发**：这正是我们 spec 045 修复的核心——但 cursor 用 deploy 钩子自动修复，比我们手动改 yaml 文件更优雅。**未来 spec 目标**：把 EnsureDeployAsciiConfig 移植到 Configurator::Run 里。


### A7. cursor 的 IPC 通信模式（推荐学习）

cursor 在所有需要 WeaselServer 释放 userdb lock 的操作前都用：

`cpp
weasel::Client client;
if (client.Connect()) {
  client.StartMaintenance();   // 让 WeaselServer 进入维护模式（释放 userdb lock）
}

// ... 做 RIME 操作（deploy / 配置变更 / 词典导入导出）...

if (client.Connect()) {
  client.EndMaintenance();     // 让 WeaselServer 退出维护模式（重新加载）
}
`

**关键点**：

- 用 Mutex WeaselDeployerMutex + StartMaintenance/EndMaintenance 双重保护（CreateMutex(NULL, TRUE, ...) + GetLastError() == ERROR_ALREADY_EXISTS 检查）
- Deploy 时 client.Connect() 可能失败（WeaselServer 没启动），需要 graceful degradation
- Export/Import 必须先 StartMaintenance，否则会撞 userdb lock——L10 lesson 已记录此问题
- RefreshServerUserDictSnapshot（A2 提到）顺序反着：先 EndMaintenance 再 StartMaintenance，目的是让 WeaselServer 重新走一遍 maintenance 流程，主动丢弃内存缓存

### A8. cursor 的 keyboard hook 实现细节（参考但不要照搬）

虽然我们不需要 TrayCommonPhrasePanel，但 cursor 的键盘钩子写法值得学：

`cpp
HHOOK keyboard_hook_ = NULL;
void StartKeyboardHook() {
  keyboard_hook_ = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                     GetModuleHandle(NULL), 0);
}
void StopKeyboardHook() {
  if (keyboard_hook_) UnhookWindowsHookEx(keyboard_hook_);
}
void HandleHookKey(WPARAM vk) {
  // 处理 Esc 关闭、Enter 提交、Up/Down 选择等
}
`

**注意**：WH_KEYBOARD_LL 是系统级钩子，性能敏感——cursor 在 panel 隐藏时立刻 StopKeyboardHook。**我们 spec 046（如果做 UI 编辑面板）不要用系统级钩子**，改用 Dialog 内的 PreTranslateMessage 即可。

### A9. cursor 的 Configurator::EnsureDeployAsciiConfig 完整逻辑

cursor 在每次 deploy 时强制执行 4 件事：

1. Shift_L/R 全部 noop（5 种 mode 都覆盖：commit_code/inline_ascii/set_ascii_mode/unset_ascii_mode/clear）
2. 如果 hotkeys 文件没有 Control+Space 自动加上（key_binder/bindings/+ 模式）
3. 同步处理三个文件：weasel_hotkeys.yaml + default.yaml + default.custom.yaml
4. 修改后调用 rime->deploy() 触发重新加载（注意顺序：先 Apply 再 deploy）

**对我们的启发**：spec 045 已经 ship，但 hotkeys 修复应该做成**自动 deploy 钩子**，而不是让用户手动改 yaml。**这是一个未来 spec（待编号）的目标**：把 EnsureDeployAsciiConfig 移植到 Configurator::Run 里，并在 L43/L46 lessons-learned 中标注自动修复模式作为后续计划。

### A10. cursor 的 DictManagementDialog 没改 4 按钮（反例：用户词典 UI 编辑的边界）

这是反例：cursor 在 WeaselDeployer 添加了 5 个新 Dialog，但**没有**改 DictManagementDialog 的 4 按钮（backup/restore/import/export）。说明 cursor 也认为用户词典的逐条 UI 编辑超出 2 天工作量。

**对我们的启发**：

- spec 048（用户词典 UI 编辑器）= 真正难点，需要 librime 扩展 commit_entry/delete_entry/lookup_entries API（参考 UserDictionary::UpdateEntry(DictEntry, commits=-N, prefix)）
- 优先级 P2 是合理的——不能 1 周内完成
- 短期方案仍是 DictManagementDialog 的 4 按钮（export/import）
- **不要在 spec 046 里尝试同时做 spec 048**——会失控


### A11. cursor 的 MSBuild 工程改动模式（vcxproj 维护）

cursor 给 WeaselDeployer.vcxproj 加 5 个文件，给 WeaselServer.vcxproj 加 1 个文件。

**关键改动清单**：

- 资源 ID 在 resource.h 里新增：IDD_CUSTOM_PHRASE、IDD_HOTKEY_SETTINGS、IDC_USER_DICT_LIST、IDC_KEY_CAPTURE 等
- .rc 文件要加对应控件定义（LTEXT ..., IDC_LABEL_CODE, ...）
- vcxproj 必须用 ClCompile Include + ClInclude Include 两个标签
- 如果用 xmake 构建，xmake.lua 的 add_files(WeaselDeployer/*.cpp) 模式已经自动包含，无需改
- **Cursor 没碰 librime-lua 集成**（rimetrae 项目里 thirdparty/librime-lua/ 是空的）

### A12. cursor 没做的事情（反推我们不该做）

- 没动 librime C++ 源码（用户词典 API 没扩展）
- 没实现 IPC 双向消息（只有 maintenance 模式开关）
- 没做 yaml 可视化 UI
- 没做 Direct2D/DWrite 重构
- 没集成 librime-lua
- 没改 weaselx64.dll（保持原样）

**启发**：cursor 2 天完成的**边界**就是 GDI + WTL Dialog + IPC maintenance + 文件读写。**我们要保持这个边界**，不要为了看起来更高级引入 D2D/WebView/yaml 解析器。FluxingComponents 控件库（D2D）只用于托盘弹出层（spec 045 验证过 OK），不要进 Dialog。

### A13. cursor 的 build 输出对照表（验证编译完整性）

`
F:\soft\02office\rimetrae\weasel\msbuild\Release\Win32\
├── WeaselDeployer\
│   ├── CommonPhraseEditDialog.obj    ✓
│   ├── CustomPhraseDialog.obj        ✓
│   ├── CustomPhraseListDialog.obj    ✓
│   └── ... 其他现有 .obj
└── WeaselServer\
    └── TrayCommonPhrasePanel.obj     ✓
`

**对照我们项目**：

- F:\soft\00selfmade\rime\output\Win32\ 没有 CustomPhrase*.obj（缺 3 文件）—— spec 046 需要补
- F:\soft\00selfmade\rime\output\Win32\ 有 QuickPanelDialog.obj（spec 045 ship 过）


### A14. 把 cursor 经验映射到我们路线图

| Cursor 已实现 | 我们路线图对应 | 优先级 |
|---|---|---|
| CustomPhraseListDialog 三件套 | **建议提升为 spec 046**（从 spec 049 提前） | **P1** |
| HotkeySettingsDialog + KeyCaptureEdit | spec 050（偏好设置 UI） | P3 |
| DeployerUiHelper.h | 立即 copy，作为所有新 UI 的基础设施 | **P0** |
| EnsureDeployAsciiConfig deploy 钩子 | spec 045 后续增强（待编号） | P2 |
| TrayCommonPhrasePanel | **不需要**（已被 QuickPanelDialog 替代） | — |
| CommonPhraseStore.h | **不需要**（同上） | — |

**结论**：我们下一步高 ROI 任务是 **spec 046 = 移植 cursor 的 CustomPhraseListDialog + DeployerUiHelper**，预计 1-2 天可 ship。spec 048（用户词典 UI 编辑器）继续 P2 推后。

### A15. rimetrae cursor 项目保留价值（永久 reference）

- **可作为 reference 永久保留**——F:\soft\02office\rimetrae\weasel\ 不要再删（用户已确认）
- **每次移植前先 diff rimetrae vs rime/weasel**——看 cursor 改了哪些文件，定位源码
- **可以直接 copy-paste 的代码**：
  - WeaselDeployer/DeployerUiHelper.h（header-only 工具库，最容易复用）
  - WeaselDeployer/CustomPhraseListDialog.h（WTL Dialog 框架，~50 行 header）
  - WeaselDeployer/CustomPhraseDialog.h（单条编辑 Dialog 框架）
  - include/CommonPhraseStore.h（如需要独立短语面板）
- **不能直接 copy**：
  - TrayCommonPhrasePanel.*（我们用 QuickPanelDialog 替代）
  - CommonPhraseStore.h（如不需要独立短语面板）
- **需要本地化的**：
  - 资源 ID（resource.h 已有的不冲突）
  - 字符串（要翻译成 rime/weasel 现有用词，如小鹤 → 火流猩）
  - vcxproj + xmake.lua（加入新文件）
  - DPI 缩放适配（cursor 用 96/120/144 DPI 测试过，我们要测 100%/125%/150%/175%）
  - 暗色/亮色主题适配（rimetrae 测了暗色，我们要测亮色）

**约定**：所有移植的 cursor 代码在 commit message 里加 origin: rimetrae 前缀，便于后续追溯。


