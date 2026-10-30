# 0021. 前端程序目录解析收敛为单一事实源：显式遍历 32/64 注册表视图 + `InstallDir → <InstallDir>\weasel`

- **状态**: Accepted
- **日期**: 2026-10-29
- **关联**: ADR 0020（单一事实源范式）/ ADR 0016（64 位换装）/ spec 002 FR-006、FR-008 / spec 088
- **相关**: `include/FluxingInstallPaths.h` · `WeaselTSF/LanguageBar.cpp` · `WeaselTSF/WeaselTSF.cpp` · `test/TestFluxingComponents/TestFluxingInstallPaths.cpp`

## 背景（Context）

v1.1.3 真机反馈：装 1.1.3 未重启，在**任务栏输入法图标**托盘先后点「重启算法服务」「重新部署」，
快捷设置栏仍然会消失；并要求核查「最新的 dll 是否启用」。

### 1. 最新二进制**已启用**（不是 dll 没生效）

- 运行中的 `WeaselServer.exe` = `D:\Program Files\fluxing\weasel\WeaselServer.exe`，PID 463472，
  `ProductVersion = 1.1.3.0`，SHA256 = `1C388FB5E3FA5A8EC358E0B59334B1A1C64F5325F0C8CA53876132601F0B938C`，
  **与仓内 `output/Win32/WeaselServer.exe` 逐字节一致**；
- CLSID `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32` → `weaselx64.1.1.3.0.dll`；
- 未版本化的 `weaselx64.dll` 仍是 1.1.0.0 —— 属 ADR 0016 的**版本化 shim 设计内**，不是漏换。

⇒ 「面板消失」与 dll 无关。

### 2. 「面板消失」的真实原因是**入口错位**，不是缺陷

「快速面板常驻」勾选项在 **WeaselServer 托盘图标右键菜单**（`WeaselServer/WeaselServer.rc:164`
`ID_QUICKPANEL_ALWAYS_SHOW`），**不在**任务栏输入法图标菜单、**也不在**面板内。
未勾选 ⇒ 瞬时模式（grace 2000 ms + 鼠标在外 1500 ms 自动隐藏）⇒ 几秒后消失是**设计行为**。

铁证：`quickpanel.state` 在新落点、旧 `%APPDATA%\Rime`、`%APPDATA%\fluxing`、`%APPDATA%\Fluxing`
**四处全部不存在** ⇒ 用户从未勾选过常驻。

### 3. 但诊断过程挖出一个**真的 fork 级缺陷**（本 ADR 的起因）

`WeaselTSF/LanguageBar.cpp` 改前：

```cpp
static LPCWSTR GetWeaselRegName() {
  if (is_wow64()) return L"Software\\WOW6432Node\\Rime\\Weasel";  // 上游键
  else            return L"Software\\Fluxing\\Weasel";
}
...
RegGetStringValue(HKEY_LOCAL_MACHINE, GetWeaselRegName(), L"WeaselRoot", dir);
```

两处错位叠加：

| 层面 | 上游约定 | 本 fork 实际 | 后果 |
|---|---|---|---|
| 键 | `Software\Rime\Weasel`（或 WOW6432Node 变体） | `Software\Fluxing\Weasel` | 32 位分支读的是**另一个厂商的键** |
| 值 | `WeaselRoot` | 安装器只写 `InstallDir`（`output/install.nsi:447`） | 读到**空串** |

⇒ 无论进程是 32 位还是 64 位，`dir` 都是空串 ⇒

- 「重启算法服务」/「程序文件夹」**静默空操作**（改前代码：读不到就整段 `skip`，点了完全没反应）；
- `WeaselTSF/WeaselTSF.cpp` 的「服务未运行自动拉起」拿同一个空串去 `ShellExecute`
  ⇒ **算法服务异常退出后前端拉不起来**。

### 4. 最关键的坑：`InstallDir` 只存在于 **32 位注册表视图**

本机实测：

```
HKLM\SOFTWARE\Fluxing\Weasel              → ERROR: 找不到（64 位视图）
HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel → InstallDir REG_SZ D:\Program Files\fluxing
HKCU\SOFTWARE\Fluxing\Weasel              → RimeUserDir REG_SZ D:\Program Files\fluxing\user1\fluxing
HKLM\SOFTWARE\WOW6432Node\Rime\Weasel    → ERROR: 找不到
```

⇒ **任何依赖「进程字长」推断视图的固定写法都会漏**。原代码用 `is_wow64()` 二选一，
两个候选还分属不同厂商的键 —— 不可能命中。

**同一次排查中被证伪的假设（记录在案，避免后人重走）**：

曾怀疑「装机跑的是 32 位 `WeaselServer.exe`（PE machine = 0x014C），而 `RimeUserDir` 只写在
HKCU 的 64 位视图 ⇒ 服务端会落到 `%AppData%\fluxing`、与前端分裂成两份用户数据」。
**实测证伪**：

- 运行中的服务（PID 463472，23:08:52 启动）在 `D:\Program Files\fluxing\user1\fluxing\rime_ice.userdb`
  上的写入时间戳是 **23:25:56**（在服务启动之后）⇒ **32 位服务进程确实读到了那个值**；
- `%AppData%\fluxing` 里最新文件时间戳是 **2026-07-31**（0.21 时代残留）⇒ 不是当前在用的目录。

⇒ **用户数据目录没有被分裂**；视图差异**只出现在 HKLM 侧**（`InstallDir` 确实只在 32 位视图）。
本 ADR 的结论不变：**不能靠进程字长推断视图，必须显式遍历两个视图**（这次是 HKLM 侧中招）。

## 决策（Decision）

### D1 不改安装器，改前端读路径

本轮**不碰** `output/install.nsi`（改动它必须跑仓外 Windows Sandbox harness，本机 Hyper-V vSwitch 异常跑不起来）。
安装器写出的 `InstallDir` 是**正确且足够**的（`<安装根>`），缺的是前端的解析逻辑 ⇒ 改前端。

### D2 解析规则收敛为单一事实源

新增 header-only 的 `include/FluxingInstallPaths.h`（ADR 0020 同款范式：server 与 test 两侧都能直接调用，
测试无需改工程链接）。公开入口：

- `ResolveFluxingWeaselRoot()` —— 主入口，返回**程序目录**（`<InstallDir>\weasel`）；
- `ReadFluxingInstallDir()` —— 只读安装根 `<InstallDir>`；
- `InstallRootSources()` —— 候选来源清单（可机检）；
- `WeaselServiceLaunchTarget(root)` —— 启动目标选择；
- `LooksLikeWeaselRoot(dir)` —— 文件系统校验（内含 `WeaselServer.exe` / `start_service.bat`）。

### D3 显式遍历 32/64 两个注册表视图

`detail::ReadRegStringWithView()` 用 `RegOpenKeyExW` 显式带 `KEY_WOW64_64KEY` / `KEY_WOW64_32KEY`
各读一次，`ReadRegStringAnyView()` 取第一个非空值。
**禁止**再出现「按进程字长猜视图」的写法。

### D4 程序目录 = `<InstallDir>\weasel`，并按文件系统校验

安装器 `output/install.nsi:25` 定义 `WEASEL_ROOT = $INSTDIR\weasel`，二进制（`WeaselServer.exe`、
`start_service.bat`、`weaselx64*.dll`）全在该子目录下 ⇒ 解析出安装根后必须补 `\weasel`。
解析结果再经 `LooksLikeWeaselRoot()` 校验（含 `WeaselServer.exe` 或 `start_service.bat`）才被认为有效。

### D5 旧键保留为**只读兜底**

来源顺序：fork 键 `Software\Fluxing\Weasel`（`WeaselRoot` 优先、`InstallDir` 次之）→ 旧键
`Software\Rime\Weasel`（上游遗留，仍可能是有效安装）。
落实 spec 002 FR-006（fork 写入集中）与 FR-008（旧键读取必须继续可用）。

### D6 失败不再静默

解析失败一律弹**简体中文** `MessageBoxW`（P2/P5）：

- 程序目录找不到 → 「未找到火流猩程序目录，请重新安装后再试。」
- 启动脚本找不到 → 「未找到算法服务启动脚本，请重新安装后再试。」

**禁止**再出现「点了完全没反应」。

### D7 启动目标优先 `start_service.bat`，缺失时退回直接启动 `WeaselServer.exe`

`WeaselServiceLaunchTarget()` 返回**存在的那个**；两者都没有才报错。

### D8 「本 DLL 自身目录」作为最后兜底

前端 DLL（`weasel.dll` / `weaselx64.dll`）与 `WeaselServer.exe` 同在 `<InstallDir>\weasel`
⇒ 注册表解析全失败时，用 DLL 自身目录兜底（`GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)`
+ `GetModuleFileNameW`；因成员函数指针不能转 `LPCWSTR`，故用文件内静态数据锚点取本模块基址）。

## 后果（Consequences）

- (+) 「重启算法服务」「程序文件夹」在 fork 下**真正可用**；算法服务异常退出后前端也能自动拉起。
- (+) 失败路径从「静默」变成「弹窗说明原因」，用户可据此判断是「没装好」还是「路径异常」。
- (+) 解析规则有了单一事实源，将来任何地方要拿程序目录一律调 `ResolveFluxingWeaselRoot()`。
- (−) 安装器与前端之间多了一条**隐式契约**：安装根 = `InstallDir`，程序目录 = `<InstallDir>\weasel`。
  契约写在 `output/install.nsi:25` 与本 ADR D4 两处；将来改安装布局必须同步本 ADR。
- (−) 未版本化的 `weaselx64.dll` 仍是 1.1.0.0（ADR 0016 设计内），真实生效的是版本化 shim
  `weaselx64.1.1.4.0.dll`。
- 回归测试：`test/TestFluxingComponents/TestFluxingInstallPaths.cpp` 新增 T1–T13（19 条断言），
  含**负对照** `LegacyResolveWeaselRoot()`（复刻上游「只读 `WeaselRoot`」的旧写法），
  断言它**解析不到程序目录** ⇒ 断言不是恒真。19/19 全过（先红后绿已验证）。
