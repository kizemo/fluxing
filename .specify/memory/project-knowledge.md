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