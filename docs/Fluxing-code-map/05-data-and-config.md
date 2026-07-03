# 05 · 数据 / 配置 / 协议

## 5.1 文件系统

### 5.1.1 安装目录（`WeaselSharedDataPath`）

```
<install>\
  weasel.dll / weaselx64.dll / weaselARM.dll / weaselARM64.dll / weaselARM64X.dll
  WeaselServer.exe / WeaselDeployer.exe / WeaselSetup.exe
  start_service.bat / stop_service.bat
  rime-install-config.bat
  install.bat / uninstall.bat
  curl.exe / 7z.exe / 7z.dll / curl-ca-bundle.crt
  WinSparkle.dll
  rime.dll / rime_console.exe   ← librime 产物（由 get-rime / librime 复制过来）
  opencc / …                    ← librime 一级 deps
  data\                         ← WeaselSharedDataPath() 返回这里
    default.yaml
    weasel.yaml
    *.yaml  *.txt  *.bin        ← 方案
    opencc\…                    ← OpenCC 词典
  archives\                     ← NSIS 产物在 build 时写
```

### 5.1.2 用户目录（`WeaselUserDataPath`）

默认 `%AppData%\Rime`，可通过 `HKCU\Software\Rime\Weasel\RimeUserDir` 覆盖。内容由 RIME 自己管理：
- `default.yaml`、`weasel.yaml`、`<schema>.yaml`
- `<schema>.userdb/`（词库）、`build/`（编译产物）

### 5.1.3 日志目录（`WeaselLogPath`）

固定 `%TEMP%\rime.weasel\`。包含 rime 引擎的 `rime.log` 等。

## 5.2 注册表

### 5.2.1 `HKCU\Software\Rime\Weasel`（Setup 写入）

| 值 | 类型 | 含义 |
|---|---|---|
| `RimeUserDir` | REG_SZ | 用户目录覆盖 |
| `Language` | REG_SZ | `chs` / `cht` / `eng` |
| `Hant` | REG_DWORD | 1 = 繁体（仅在安装时） |
| `ToggleImeOnOpenClose` | REG_SZ | `yes` / `no` |
| `UpdateChannel` | REG_SZ | `release` / `testing` |

### 5.2.2 `HKCU\Software\Rime\weasel`（TSF 读取，小写！）

- `ToggleImeOnOpenClose`（被 `WeaselTSF::OnSetThreadFocus` 读取）
- 历史原因：Setup 写大写 `Weasel`，TSF 读小写 `weasel`（大写路径里 `ToggleImeOnOpenClose` 找不到时回落到小写 key — 详见 `WeaselTSF.cpp::OnSetThreadFocus` 读 `HKCU\Software\Rime\weasel` 的逻辑）。**二次编辑时应统一**。

### 5.2.3 `HKCU\Software\Rime\Weasel\Updates`（WinSparkle）

WinSparkle 自己的持久化区。

### 5.2.4 `HKLM\SOFTWARE\Microsoft\CTF\KnownClasses` 与 `HKCR\CLSID\{A3F4CDED-...}`

TSF / COM 注册（由 `WeaselSetup\imesetup.cpp`）。

### 5.2.5 `HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\WeaselServer.exe`

崩溃转储位置（由 `imesetup.cpp` 写）。

## 5.3 IPC 文本协议

每行 `key=value`；key 用 `.` 分段（首段当 action），value 转义 `\n \t \\`。
响应以单行 `.` 结束。

### 5.3.1 Request actions

| action | key | 含义 |
|---|---|---|
| `session` | `session.client_app` / `session.client_type` | 启动会话时 client 报告应用名与类型 (`ime` / `tsf`) |

### 5.3.2 Response actions

| action | 典型 keys |
|---|---|
| `commit` | `commit=<上屏字符串>` |
| `context` | `context.preedit`, `context.sel_start/end`, `context.cursor`, `context.attributes`(序列化), `context.caret_x/y`, `context.cinfo.currentPage/totalPages/highlighted/is_last_page/candies/comments/labels` |
| `status` | `status.ascii_mode`, `status.zhung`, `status.schema_id`, `status.schema_name`, `status.disabled`, `status.composing`, `status.ascii_punct`, `status.show_notifications`, `status.show_notifications_time` |
| `style` | 整个 `UIStyle`（60+ 字段，详见 `include\WeaselIPCData.h`） |
| `config` | `config.inline_preedit`, `config.inline_code` 等 |
| `option` | `option.<key>=true/false`（如 `option.ascii_mode=true`） |

## 5.4 UIStyle 关键字段

来源：`include\WeaselIPCData.h`，由 `RimeWithWeaselHandler::_UpdateUIStyle` 从 `weasel.yaml` 读出。

```cpp
struct UIStyle {
  // 字体
  std::wstring font_face, label_font_face, comment_font_face;
  int font_point, label_font_point, comment_font_point;
  // 行为
  int hover_type, candidate_abbreviate_length;
  bool inline_preedit, ascii_tip_follow_cursor;
  int align_type, antialias_mode, mark_text, preedit_type, display_tray_icon;
  // 排版
  int layout_type, vertical_text_left_to_right, vertical_text_with_wrap, paging_on_scroll;
  int min_width, max_width, min_height, max_height;
  int border, margin_x, margin_y, spacing, candidate_spacing;
  int hilite_spacing, hilite_padding_x, hilite_padding_y;
  int round_corner, round_corner_ex;
  int shadow_radius, shadow_offset_x, shadow_offset_y;
  bool vertical_auto_reverse;
  int baseline, linespacing;
  // 颜色
  COLORREF text_color, candidate_text_color, candidate_back_color,
           candidate_shadow_color, candidate_border_color,
           label_text_color, comment_text_color, back_color, shadow_color,
           border_color, hilited_text_color, hilited_back_color,
           hilited_shadow_color, hilited_candidate_text_color,
           hilited_candidate_back_color, hilited_candidate_shadow_color,
           hilited_candidate_border_color, hilited_label_text_color,
           hilited_comment_text_color, hilited_mark_color,
           prevpage_color, nextpage_color;
  // 每客户端
  int client_caps;
  // 图标
  std::wstring current_zhung_icon, current_ascii_icon,
                current_half_icon, current_full_icon;
  // 状态栏
  int label_text_format;
  int enhanced_position, click_to_capture;
};
```

`RimeWithWeaselHandler::_UpdateUIStyleColor` 会在 `color_scheme_dark` 配置存在时切到暗色方案。

## 5.5 weasel.yaml 默认模板（在 `output\data\`）

构建时由 `rime-install-config.bat` 生成；典型字段：

```yaml
patch:
  schema_list:
    - {schema: luna_pinyin}
    - {schema: luna_pinyin_simp}
    - {schema: terra_pinyin}
    - {schema: bopomofo}
    - {schema: bopomofo_tw}
    - {schema: cangjie5}
    - {schema: stroke}
    - {schema: wubi}
  "ascii_mode/reset_on_input": true
  "menu/page_size": 5
  style:
    font_face: "Microsoft YaHei"
    font_point: 12
    inline_preedit: false
    display_tray_icon: true
    layout_type: 0   # LAYOUT_HORIZONTAL
    color_scheme: google  # 或 aqua / aqua_dark / ...
    hilited_candidate_back_color: 0x006dcc
    ...
  global_ascii: false
  show_notifications_time: 1200
  "app_options/<app>.exe": {ascii_mode: true, ...}
```

完整 schema 字段定义见 librime 文档。

## 5.6 appcast 与更新

- `update/appcast.xml`（Sparkle RSS）：包含当前版本的 `enclosure url`、`sparkle:version`、release notes 链接。
- `update/bump-version.ps1` 与 `bump-version.sh`：
  - 修改 `build.bat` / `xbuild.bat` 里的 `VERSION_MAJOR/MINOR/PATCH`；
  - 在 `CHANGELOG.md` 顶部插入新条目（按 commit 类型分组：build/ci/fix/feat/docs/style/refactor/test/chore）；
  - 可选 `-tag` 创建 git tag。
- `update/write-release-notes.sh`：抽取 `RELEASE_CHANGELOG.md`。
- `CI`：`master` 分支构建出 `Nightly Build` 预发布；`[0-9]+.*` tag 出正式 release（草稿，需人工 publish）。
- `update-appcast.yml` 在 release published 时调 `rime/home` 仓库的 `gh-pages.yml` 更新 appcast。

## 5.7 日志与诊断

- RIME 引擎日志：`%TEMP%\rime.weasel\rime.log`。
- Debug 流：`include\WeaselUtility.h::DebugStream` + `DEBUG` 宏，输出到 `OutputDebugString`（用 DebugView 可见）。
- `LOG / DLOG`（`include\logging.h`）接 glog（C++ 流式，INFO/WARNING/ERROR）。
- WinSparkle 日志：默认在 `%TEMP%`。
- 崩溃转储：`HKLM\...\LocalDumps\WeaselServer.exe`。
