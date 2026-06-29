# 火流猩输入法 / Fluxing

> **RIME 引擎的 Windows 原生前端 fork，基于 [rime/weasel](https://github.com/rime/weasel)**
>
> 品牌：中文名"火流猩输入法"，英文名 `Fluxing`。
> 派生分支：`Fluxing`（基于 weasel 0.17.x）
> 安装目录：`%ProgramFiles%\Fluxing\weasel`
> 用户数据：`%LocalAppData%\Fluxing`

[![Download](https://img.shields.io/badge/download-fluxing--0.17.5.0-blue)](https://github.com/kizemo/Fluxing/releases)
[![License: GPLv3](https://img.shields.io/badge/license-GPLv3-green.svg)](LICENSE.txt)

---

## 这是什么

火流猩输入法是 RIME（中州韻）输入法引擎的 **Windows 客户端**。它保留 RIME 引擎的全部灵活性（高度可定制、支持任意方案），同时提供原生 Windows 输入法框架（TSF）集成、系统托盘、自动部署等"装上就能用"的体验。

与上游 rime/weasel 的关键差异：

- **品牌化**：产品名、安装目录、注册表路径、托盘资源均替换为 `Fluxing` / `火流猩输入法`，与上游 weasel 互不冲突。
- **用户数据隔离**：默认用户数据目录为 `%LocalAppData%\Fluxing`（而非 `%AppData%\Rime`），安装路径强制 `fluxing` 后缀，便于多套输入法并存。
- **预装数据扩展**：默认捆绑 rime_ice / 八股文 等实用方案的数据文件，开箱即用。
- **v2 路线图（开发中）**：见 `.specify/specs/004-fluxing-v2-roadmap/spec.md` —— mac 风可视化设置面板、跨设备云同步、候选字右键编辑、常用短语独立库、暗色主题跟随系统等。

---

## 安装

### Windows 8.1 / 10 / 11

1. 从 [Releases](https://github.com/kizemo/Fluxing/releases) 下载最新 installer（`fluxing-X.Y.Z-installer.exe`）。
2. 双击运行；选择安装路径（默认 `C:\Program Files\Fluxing\weasel`）。
3. 选择用户数据目录（默认 `%LocalAppData%\Fluxing`，强制 `fluxing` 后缀）。
4. 安装完成后，任务栏出现 Fluxing 输入法指示器；在系统输入法列表启用"火流猩输入法"。

### 验证

- 打开任意编辑器（notepad / Word / VS Code），切换到火流猩输入法。
- 输入拼音"nihao"，候选"你好"等字词出现。
- 按 `F4` 调出方案选择；按 `Ctrl+grave` 反查。

---

## 快速上手

### 常用快捷键（v2.0.0 默认）

| 功能 | 快捷键 |
|---|---|
| 翻页（下一页 / 上一页） | `,` / `.` |
| 切换中英文 | `Shift_L` 或 `Shift_R` |
| 上屏第 2 / 第 3 候选 | `Shift_L` / `Shift_R`（有候选时） |
| 切换中英标点 | `Ctrl+Shift+9` |
| 切换简繁 | `Ctrl+Shift+0` |
| 调出方案选择 | `F4` |
| 反查 | `Ctrl+grave` |

> v2.0.0 之前的 weasel 默认快捷键（`-`/`=` 翻页、`Control+Shift+1..9` 选候选）已替换为以上"vim 风格"快捷键。详见 spec 005。

### 配置文件

用户数据目录结构：

```
%LocalAppData%\Fluxing\
├─ default.yaml          # 全局快捷键 / 方案列表 / 行为
├─ weasel.yaml           # 样式（颜色 / 字体 / 布局）
├─ <schema>.schema.yaml  # 方案定义（朙月拼音 / rime_ice / ...）
├─ user.db              # 用户词典（自动维护）
└─ installation.yaml    # 安装器写入的元数据
```

通过托盘菜单的"用户文件夹"快捷打开。修改任意 yaml 后，点击托盘 → "重新部署"生效。

### 定制 RIME

- **方案**：参考 [RIME 定制指南](https://github.com/rime/home/wiki/CustomizationGuide)。
- **样式**：修改 `weasel.yaml` 的 `style.color_scheme` / `style.layout_type`。
- **快捷键**：修改 `default.yaml` 的 `key_binder/bindings`。
- **进阶**：可用 [plum](https://github.com/rime/plum) 一键安装社区方案。

---

## v2 路线图

完整规划见 `.specify/specs/004-fluxing-v2-roadmap/spec.md`。P1（v2.0.0）核心特性：

1. **默认快捷键 rev2**（spec 005）— `,`/`.` 翻页、Shift_L/R 单键上屏候选
2. **候选字右键编辑**（spec 008）— 右键一键删除用户词典词条
3. **托盘快速设置面板**（spec 006）— mac 风 `Alt+,` 弹窗，零菜单深度
4. **常用短语独立库**（spec 009）— `Alt+K` 弹列表，独立于用户词典
5. **yaml 可视化编辑**（spec 007）— 不再手编 yaml，UI 编辑快捷键/方案/词典
6. **暗色主题跟随系统**（F11 横切）— 200ms 渐变切换

后续版本：

- **v2.1.0**：跨设备云同步（spec 010，Vercel + 邮箱 + Passkey）
- **v2.2.0**：现代化安装器 / 首启引导（spec 011）

---

## 开发

### 构建

```cmd
# 一次性：安装 Boost 1.83+、NSIS 3.x、VS 2022 BuildTools
# 详见 INSTALL.md

# x64 + Win32 全构建
build.bat
```

输出：`release/fluxing-X.Y.Z-installer.exe`。

### Spec / Plan / Tasks

所有重大变更遵循 `.specify/memory/constitution.md` 与 `spec-driven-development` 工作流：

- `.specify/specs/NNN-<name>/spec.md` — 意图 / 用户故事 / FR / SC
- `.specify/specs/NNN-<name>/plan.md` — 技术方案
- `.specify/specs/NNN-<name>/tasks.md` — 实施分解

---

## 致谢

### 上游

- **RIME 引擎**：基于 [rime/librime](https://github.com/rime/librime)（BSD 3-Clause）。
- **Weasel 前端**：基于 [rime/weasel](https://github.com/rime/weasel)（GPLv3）。
- **方案与词库**：
  - 【朙月拼音】系列及【八股文】词典 — 佛振、瑾昀
  - 【注音／地球拼音】— 佛振、瑾昀
  - 【仓颉五代】— 朱邦复先生（码表源自 chinesecj.com）
  - rime_ice 方案 — iDvel

### 贡献者

- 当前 fork 维护者：[duanyi](https://github.com/kizemo) <duanyi@aiec.fun>
- 上游贡献者：佛振、邹旭、Xiangyan Sun、Prcuvu、nameoverflow、fxliang、Azuk 443 等（见 [contributors](https://github.com/rime/weasel/graphs/contributors)）

### 美术

- 图标设计：Patricivs
- 配色方案：Aben、P1461、Patricivs、skoj、佛振、五磅兔

### 引用的开源软件

Boost C++ Libraries、curl、google-glog、Google Test、LevelDB、librime、marisa-trie、OpenCC、plum、WinSparkle、yaml-cpp、7-Zip（完整许可见 [LICENSE.txt](LICENSE.txt)）。

---

## 反馈与交流

- Bug 反馈：[github.com/kizemo/Fluxing/issues](https://github.com/kizemo/Fluxing/issues)
- Pull Request：[github.com/kizemo/Fluxing/pulls](https://github.com/kizemo/Fluxing/pulls)
- RIME 引擎与方案：[github.com/rime/home/issues](https://github.com/rime/home/issues)
- 联系维护者：<duanyi@aiec.fun>

---

## 许可

本仓库代码遵循 **GPLv3**（与上游 weasel 一致）。详见 [LICENSE.txt](LICENSE.txt)。

RIME 引擎遵循 BSD 3-Clause；本仓库引用 / 链接的所有第三方开源项目许可见各项目仓库。