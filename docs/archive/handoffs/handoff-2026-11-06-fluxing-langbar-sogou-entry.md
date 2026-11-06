# Handoff · 语言栏右键菜单「导入搜狗用户词库」：完成收尾（2026-11-06）

- **仓库**：`F:\soft\00selfmade\rime_claude`（Fluxing / 火流猩，分支 `Fluxing`，HEAD `ae667b6`）
- **本棒性质**：**收尾完成** —— 编译双架构 → 打包 → 静默安装 → 产物哈希与注册核验 → explorer 已加载新 DLL。仅剩**人工目视 + 点击一次**的真机验收。
- **用户诉求原话**：「在输入法任务栏图标的右键菜单中，没有同步搜狗词库的子菜单。需要添加，便于手动进行词库同步。」

## 1. 结论（一句话）

三套 TSF 语言栏菜单（英/繁/简）各补了一条「导入搜狗用户词库 (&G)」`MENUITEM`，双架构已编译、已打包、已装到本机，任务栏（`explorer.exe`）已加载含该项的新 DLL；**C++ 零改动**，点击后经既有 IPC 转发到 `WeaselServer` 的 `40022` handler。

## 2. 已完成（逐项有证据）

| 环节 | 结果 | 证据 |
|---|---|---|
| 源码改 | `WeaselTSF/WeaselTSF.rc` 三处 + `include/resource.h` 新增 `ID_WEASELTRAY_SOGOU_IMPORT 40022` | rc 新行 `:73`（EN）/`:98`（HANT）/`:123`（HANS），均紧跟 `ID_WEASELTRAY_DICT_MANAGEMENT`，`&G` 三套菜单均未占用；rc UTF-16LE+BOM，CRLF 241 / 裸 LF 0 |
| 编译 | x64 `output\weaselx64.dll` 1,989,632 B；x86 `output\weasel.dll` 1,678,336 B；`output\Win32\WeaselServer.exe` 2,324,480 B | `tmp\build-tsf-menu.cmd` 末行 `NSIS_RC=0` / `ALL_STEPS_OK` |
| 打包 | `output\archives\fluxing-1.2.1-installer.exe` 42,835,332 B | makensis 仅既有 7010 warning（weaselARM*、`data\*.gram`、`data\lua\*.txt` 缺失为常态） |
| 安装 | `D:\Program Files\fluxing\weasel\` 下 `weaselx64.1.2.1.0.0.dll`（新 shim）、`weasel.dll`、`WeaselServer.exe` 全部为本次构建 | 静默安装 `/S /D=D:\Program Files\fluxing`；无 `D:\Program` 残留目录 ⇒ `/D=` 未被截断 |
| 产物一致性 | 三项 SHA256 前 16 位全 MATCH | `weaselx64.1.2.1.0.0.dll` C708E86C13C67EA0 / `weasel.dll` 823C5545FF64DB85 / `WeaselServer.exe` 52FDA23F833609FF |
| 注册 | x64 CLSID `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` → `weaselx64.1.2.1.0.0.dll`；WOW6432Node → `weasel.dll` | HKLM `Software\Classes\` 与 `Software\WOW6432Node\Classes\` |
| 生效 | **explorer.exe pid 18112（2026-11-07 00:00:22）已映射 `weaselx64.1.2.1.0.0.dll`** | `Get-Process explorer \| ForEach Modules \| ? ModuleName -like 'weasel*'` |
| 菜单资源 | 两个 DLL 的 `RT_MENU` 由 518/404/404 B 变为 **590/436/436 B**，模板内含三条新文案；前缀 `鱖`=U+9C36=**40022** 恰为新项 `mtID` | `tmp\verify-tsf-menu.ps1`（`LoadLibraryEx(...,2)` + `EnumResourceNames(RT_MENU)` + 宽串扫描） |
| 文档 | CHANGELOG 顶部新增 `## [Unreleased]` 段 | 断言 `bareLF=0`、`HEAD_OK=True` |

## 3. 唯一未完成：人工真机验收

**做法**：右键任务栏右下角输入指示器（UIA 名称 `任务栏输入指示 简体中文(中国大陆) 火流猩输入法`，矩形 `2240,1368,66,72`）→ 简中菜单里应出现「导入搜狗用户词库 (&G)」→ 点它应拉起 `WeaselDeployer.exe /sogou` 弹出导入框。

**为什么没自动验**：模拟右键在 4 个候选坐标（`2216,1404` / `2200,1404` / `2273,1404` / `2216,1380`）下均未产生 `#32768` 菜单窗口，判定为 Win11 XAML 任务栏对合成输入的自动化限制，非代码缺陷（`tmp\probe-tray-menu.ps1` 保留）。

**点击前提醒**：点「导入搜狗用户词库」会**真的执行一次导入**（写 userdb），不是空跑。

## 4. 本轮踩的坑（下次别再踩）

1. **`Start-Process -ArgumentList ... -Wait` 会等整个进程树** —— 安装器把 `WeaselServer` 作为子进程拉起，父安装器退出后 `-Wait` 仍挂住不返回。静默安装必须由 `.bat` 经 `cmd /c` 启动（见交接卡 `handoff-2026-11-06-fluxing-121-release-and-sogou-import.md` §九）。
2. **`-ArgumentList` 会在 `=` 边界粘连** `/D=D:\Program Files\fluxing`，必须走裸尾参且放在最后。
3. **删/重启 `TextInputHost.exe` 不换 DLL** —— 它惰性加载，重启后不立即映射 `weasel*` 模块；真正承载任务栏语言栏按钮的是 **`explorer.exe`**，必须重启 explorer 才会加载新 shim。
4. **旧 `weaselx64.dll` 留在盘上不影响** —— CLSID 已指向版本化 shim（L120 机制），新进程走 shim，旧文件只是已退出进程的遗留。

## 5. 下一步建议（按优先级）

1. 人工目视 + 点击一次，完成真机验收（唯一遗留）。
2. 若右键仍看不到：先确认点的是**任务栏输入指示器**，而不是**系统托盘区的 WeaselServer 图标**（后者本来就有该项，`98e8380` 已加）。
3. 验收通过后可考虑把本次两个 commit（`ae667b6` + docs）push 到 `kizemo/Fluxing`（**push 前问用户**）。
4. 与本次无关但仍在 backlog：`tmp\cleanup-install-leftovers.cmd` 需 admin 跑一次；SGOU 降级率 95.3% 为已知限制；SGPU 3,109 条英文自造词主动拒收为设计取舍。

## 6. 复现命令（可直接抄）

```powershell
# 构建（后台，成功标志 ALL_STEPS_OK）
& cmd /c "\"F:\soft\00selfmade\rime_claude\tmp\build-tsf-menu.cmd\""

# 菜单资源验证（对任意 weasel*.dll）
pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\tmp\verify-tsf-menu.ps1'

# 静默安装（正确写法：cmd /c，/D= 为裸尾参）
cmd /c '"F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.2.1-installer.exe" /S /D=D:\Program Files\fluxing'

# 让新 DLL 生效
Get-Process explorer | Stop-Process -Force
```