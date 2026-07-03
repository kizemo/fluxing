# Fluxing 项目代码图谱 · 总览（Index）

> 本目录是对 [rime/weasel](https://github.com/rime/weasel) 在本地克隆 `F:\soft\00selfmade\rime` 上形成的代码图谱。所有图谱只读描述现有实现，**未修改任何源码**。
>
> 目标读者：将来在 `Fluxing` 分支上做"火流猩输入法"二次编辑的工程师与 AI 代理。
> 适用版本：master @ `93eec2d` （weasel 0.17.4）。

## 文档目录

| 文件 | 主题 | 适合谁先看 |
|---|---|---|
| `01-overview.md` | 项目目标、子模块清单、运行期形态、目标平台与依赖 | 所有人 |
| `02-architecture.md` | 进程划分、组件依赖、IPC 协议、运行期数据流、状态机 | 二次编辑、添加功能 |
| `03-build-pipeline.md` | `build.bat` / `xbuild.bat` / `xmake.lua` / `weasel.sln` / 子模块 / `env.bat` / CI | 改构建、改安装器、做 release |
| `04-module-deep-dive.md` | 每个子模块（WeaselTSF / WeaselUI / WeaselServer / WeaselDeployer / WeaselSetup / RimeWithWeasel / WeaselIPC / WeaselIPCServer）的入口、关键类、关键文件 | 改特定模块时 |
| `05-data-and-config.md` | 用户/共享/日志目录、注册表项、IPC 文本协议、UI Style、rime 配置、YAML 文件 | 改用户配置、改样式、改消息协议 |
| `06-customization-points.md` | "火流猩"二次编辑的具体可改点清单（产品名、CLSID、Profile、安装器、托盘、皮肤、命令） | 二次编辑负责人 |

## 项目一句话定义

**Weasel（小狼毫）** 是 **RIME（中州韻）输入法引擎** 的 Windows 原生前端，由 TSF 文本服务 + 后台服务进程 + 设置/部署 GUI + 安装/卸载器组成，使用 C++/ATL/WTL/D2D/DirectWrite，通过命名管道（PipeChannel）以文本协议与 RIME 的 librime C API 对接。

## 二次编辑的命名约定（用户已确认）

- 分枝名：`Fluxing`（已建）
- 中文名：**火流猩输入法**
- 英文名：Fluxing

后续所有"对外标识"（产品名、CLSID、托盘、安装器名、安装目录、注册表、appcast）需要从 "Weasel / 小狼毫" 映射到 "Fluxing / 火流猩输入法"。详见 `06-customization-points.md`。

## 仓库现状快照

- 跟踪文件：311
- HEAD：`93eec2d fix(WeaselTFS): Always update UIElements.`
- 当前分支：`Fluxing`（基于 `master`）
- 工作区：干净（`git status` 0 changes）
- 远程：`origin = https://github.com/rime/weasel.git`
- Git 全局代理：`http://127.0.0.1:7897`（已验证可走 `https://github.com`）

## 关键名词

| 名词 | 含义 |
|---|---|
| **TSF** | Text Services Framework，Windows Vista+ 的输入法/IME 框架。本项目用 `ITfTextInputProcessor` 等接口挂入系统 |
| **CLSID** | TSF 文本服务 GUID；本项目为 `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` |
| **Profile GUID** | TSF 输入档 GUID；本项目为 `{3D02CAB6-2B8E-4781-BA20-1C9267529467}` |
| **librime** | RIME 输入法引擎 C 库，作为子模块引入 |
| **RimeApi / RimeTraits** | librime 的 C API 入口与初始化结构体 |
| **PipeChannel** | 基于 Windows 命名管道的 IPC，per-user（`\\.\pipe\<username>...`） |
| **IPC 文本协议** | `key=value` 形式（action=session / context / status / style / commit 等）+ `.\n` 终止 |
| **WinSparkle** | 第三方更新库；appcast.xml 驱动升级检查 |
