# spec 093 — WeaselServer 命令行参数解析加固（L160）

- **状态**：实施中（代码与测试已落地，待产品构建 / 全量套件 / 提交）
- **来源**：交接文档 `docs/archive/handoffs/handoff-2026-10-31-fluxing-1.1.6-spec091-delivery.md` §6.3 方向 ③
- **性质**：**bugfix**（不是"加固"）—— 决策按 CLAUDE.md §3 分流到 `.specify/memory/lessons-learned.md` 的 `L160` 节，**不进 ADR**
- **协议变更**：零。IPC 线协议不动 ⇒ 不触发 P2 的双端同步，也不触发"server / TSF / deployer 三端 + ADR + `test/TestWeaselIPC/*`"的一次性提交要求

---

## 1. 缺陷

`WeaselServer.exe` 的 `_tWinMain` 收到的是 WinMain 的**原始** `lpCmdLine`。按 MSDN 的经典示例，
该字符串在第一个参数之前带一个空格：

```
WeaselServer.exe /quit   ->   lpCmdLine == " /quit"
```

`WeaselServer/WeaselServer.cpp` 修复前用**整串比较**判定选项：

```cpp
if (!wcscmp(L"/quit", lpstrCmdLine)) { ... }
```

对 `" /quit"` 恒为假。因此下列 7 个命令行选项**全部是死选项**：

| 选项 | 预期行为 | 实际行为（修复前） |
|---|---|---|
| `/q` | 停止正在运行的服务 | 落进"已在运行则重启"分支 → **重启服务** |
| `/quit` | 同上 | 同上 |
| `/ascii` | 开启 ASCII 模式 | 无效 |
| `/nascii` | 关闭 ASCII 模式 | 无效 |
| `/userdir` | 打开用户数据目录 | 无效 |
| `/weaseldir` | 打开安装目录 | 无效 |
| `/update` | 检查更新 | 无效 |

`/quit` 的实际行为与用户意图**完全相反**（用户要停服，程序在重启服务），这是本项的核心问题。

**对照组**：`test/TestWeaselIPC/TestWeaselIPC.cpp` 用的是 CRT 的 `argv[1]`（已分词，无前导空格问题），
所以它一直正常 —— 这也是为什么这个缺陷一直没被现有测试发现。

## 2. 目标

1. 命令行选项解析收敛到**唯一同源**的共享头纯函数（宪法 **R7**：生产代码与测试共用共享头纯函数）。
2. `WeaselServer.cpp` 的调用点改走共享解析，7 个选项全部恢复可达。
3. 新建专用测试工程 `test/TestServerCmdLine/`：行为断言 + **调用点守卫**双重钉死回归。
4. 登记进 `scripts/test-infra/run-test-suite.bat`、`weasel.sln`、`.gitignore`。

## 3. 非目标

- 不改变任何选项的**语义**：`/quit` 仍是"停服"，`/ascii`/`/nascii` 的开关方向不变，
  `/update` 仍在原位置（重启分支之后）执行。
- 不扩大为不区分大小写 —— 保持旧的**精确匹配**语义（`/QUIT` 仍不识别）。
- 不引入新的命令行选项。
- 不改 IPC 协议、不新增 IPC 消息类型。
- 不改 `WeaselServer` 的重启/停服流程本身（那段代码一直是对的，只是永远进不去）。

## 4. 验收标准（AC）

| # | 验收内容 | 判据 |
|---|---|---|
| AC1 | 带前导空格的原始 `lpCmdLine` 下 7 个选项全部可达 | `Parse(L" /quit") == Option::Quit` 等 7 条断言全绿 |
| AC2 | 缺陷根因被钉死 | 断言 `wcscmp(L"/quit", L" /quit") != 0` 等 7 条"旧比较恒为假"见证 |
| AC3 | 生产调用点不再回退到整串比较 | `WeaselServer.cpp` 必须出现 `servercmd::Parse`，且不得残留任何 `wcscmp(L"/` |
| AC4 | 专用测试工程可跑且退出码 0 | `Release\TestServerCmdLine.exe` 输出 `=== ALL TESTS PASSED ===`，退出码 0 |
| AC5 | 生产改动可编译 | `build.bat weasel` 构建成功 |
| AC6 | 套件不回归 | `scripts/run-tests.bat` 退出码 0（含新增的 `TestServerCmdLine`） |
| AC7 | 工程登记完整 | `run-test-suite.bat` 构建/运行两处循环均含 `TestServerCmdLine`；`weasel.sln` 含工程条目 + 4 条配置映射；`.gitignore` 含 `*.obj` 规则 |

## 5. 风险

- **行为面变宽**：旧行为要求整串精确相等；新行为取**首个记号**。`WeaselServer.exe /ascii extra`
  现在会被识别成 `/ascii`。判定为可接受（旧行为下 `/ascii` 根本不可达，不存在回归面）。
- **`/update` 的位置**：保持不变（仍在"已在运行则重启"分支之后），不改原有流程。
- **不做真机停服验收**：`/quit` 的端到端行为依赖运行中的 WeaselServer 与命名管道，留待下次装机；
  本 spec 只交付解析层与调用点的**可验证**证据（AC1–AC6）。
