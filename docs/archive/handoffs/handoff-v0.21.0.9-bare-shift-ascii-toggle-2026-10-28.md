# 接力交接：Fluxing 0.21.0.9 — 裸 Shift 切中英文（T1）已落地

> 本文件是**给下一个会话读的完整上下文**。下一个会话的 prompt 只负责指路，不重复本文件内容。
> 生成时间：2026-10-28（会话 `mvs_3303bd8edf4d4ada80136cf7b3b6e929`）
> 仓库：`F:\soft\00selfmade\rime_claude`（远端 `kizemo/Fluxing`，**不是 origin**）

---

## 1. 必读文档（按顺序，路径已给全）

1. `F:\soft\00selfmade\rime_claude\CONTEXT.md` — 身份 / 模块 / 词汇 / 不变量（现有 17 条）
2. `F:\soft\00selfmade\rime_claude\AGENTS.md` — 操作手册；§2.5 是静默装机 smoke test 配方，§4 是危险区
3. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` — 法则层（R1–R9 / P1–P8）
4. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` — 部署 + NSIS 配方
5. `F:\soft\00selfmade\rime_claude\Fluxing-二次开发内容清单.md` — **任务总账**：哪些做完了、哪些没做、下一阶段做什么
6. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` — 工程技能消费契约
7. 本轮专项：`F:\soft\00selfmade\rime_claude\.specify\specs\082-bare-shift-ascii-toggle\{spec.md,plan.md,tasks.md}`

> **读清单第 5 条优先**——它是任务进度的唯一权威；spec 里的勾选框不可信，以 CHANGELOG + 门禁数字为准。

---

## 2. 本轮（0.21.0.9）已完成的东西

### 2.1 提交（全部已推送前在本地，push 见 §5）

| SHA | 标题 | 内容 |
|---|---|---|
| `a023ee1` | `style(fluxing): clang-format the C++ files the bare-Shift feature has to touch` | 4 个 C++ 文件整体格式化（45/3/8/15 行违规）。**这是 CI lint 门禁的前置**，见 §6.1 |
| `258e708` | `feat(fluxing): bare Shift toggles Chinese/English when no candidate menu is open` | 功能本体 + 全部配套文档 + spec 082 |

### 2.2 功能语义（ADR 0010，Accepted）

- 候选窗**没开** ⇒ 裸按 `Shift` ⇒ 中↔英切换（**只切当前会话**，与 yaml 的 `Shift+space` 逐字对齐）
- 候选窗**开着** ⇒ 裸按 `Shift` ⇒ 仍然选第 2/3 个候选（spec 077 行为不变，**无双击逃生门**）
- 开关：`output/data/weasel.yaml` 的 `[global settings]` → `bare_shift_toggles_ascii: true`
  - **缺失即默认开** ⇒ 老用户升级即生效，不需要改配置
  - 关掉：`bare_shift_toggles_ascii: false`
- 关键实现事实（别再重新推导）：
  - 「菜单开着」谓词 = `ctx.menu.num_candidates > 0 && !status.is_ascii_mode`
    —— **不能只看 `is_composing`**：英文模式下 librime 仍保留 1 个候选的 raw 段
    （`ascii_segmentor.cc:20`），且 `key_binder.cc:255` 自己构造 `has_menu` 上下文时
    也要 `&& !ascii_mode`。只看 `is_composing` ⇒ 英文模式切不回中文。
  - 切换走 `get_option`/`set_option(session, "ascii_mode", !cur)` + `_UpdateUI(ipc_id)`
    —— **绝不走 `SetOption()`**（那个函数会写 `m_global_ascii_mode` 全局镜像）
  - `interveningKey` 守卫天然挡住 `Shift+X` / `Shift+space` / `Shift+标点`
  - 判定逻辑抽成 `include/ShiftStateMachine.h` 的纯函数 `DecideShiftUpAction()`
    （无 rime_api / IPC / GUI 依赖，可单测，沿用 spec 077 模式）

### 2.3 验证数字（本地，格式化后复跑）

```
full suite:                  === ALL TESTS PASSED ===
TestShiftIPCStateMachine:    Result: 28 PASS / 0 FAIL   (17 个测试函数 / 28 条断言)
TestDefaultHotkeys:          Passed: 89 / 89
TestShiftIPCWire:            Result: 10 PASS / 0 FAIL
TestPipeChannelRace:         Result: 12 passed, 0 failed
hotkey gate (出货配置):        SUMMARY|bindings=39|reachable=39|dead=0|known_dead=0|
                                   expectations=0|mismatches=0|unused=0
                              RESULT|PASS
clang-format-18 (CI 同版本) 对全部触及的 .cpp/.h: clean
```

### 2.4 交付物

- 安装包：`F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.21.0.9-installer.exe`
  - 大小 **42,761,559 B**
  - SHA256 **D1FCF78304FB89955F61F8925C894AB1F2CA5941FF9E9283D859D93A3E820E1C**
  - 注意：这是**格式化后重编**的包；格式化前那个 42,749,322 B / `1B5CEA8E…` 的包已作废
- `env.bat`（gitignored，不进 commit）：`WEASEL_BUILD=9`、`PRODUCT_VERSION=FILE_VERSION=0.21.0.9`

---

## 3. 还没做的（下一阶段的起点）

| 项 | 状态 | 说明 |
|---|---|---|
| **真机按键验收** | ⏳ 等用户 | 6 条清单见 §3.1。**只有人能做**，代码侧已全绿 |
| **清单 T2**（设置界面抓键路径 layout-aware 改造） | ⏸ 未授权 | 清单 §4.1 T2 行；改 `WeaselUI` 设置界面的抓键逻辑 |
| **清单 T3/T4/T5** | ⏸ 未开工 | 见清单 §4.1 表格 |
| **共享 `data/` 目录其余文件升级** | ⚠️ 待用户操作 | 除 `default.yaml` 外仍是 2026-08 旧版；**装 0.21.0.9 安装包**后一致 |
| **0.21.0.9 进 `release/` 发布链** | ⚠️ 未做 | `release/` 里还没有它 |
| **T2.13 / T14** | ⚠️ 已知遗留 | `test/v0_19_0_30_e2e/build_e2e.bat` 写死 4 处机器路径；`build (xmake)` arm64 目标缺失 |
| **CI `sandbox-gate` job** | ⛔ 排队中 | 缺带 `sandbox` 标签、支持嵌套虚拟化的 Win11 自托管 runner（基础设施缺口，非代码问题） |

### 3.1 真机验收清单（spec 082 §3.4，用户照着敲）

1. 中文模式、候选窗**没开** ⇒ 单独按一下 `Shift` ⇒ 切英文
2. 再单独按一下 `Shift` ⇒ 切回中文
3. 中文模式、**先打拼音**让候选窗弹出 ⇒ 单独按一下 `Shift` ⇒ **仍选第 2 个候选**，**不**切模式
4. 英文模式、候选窗**没开** ⇒ 按一下 `Shift` ⇒ 切回中文
5. 任意状态下 `Shift+X` ⇒ 只出 `X`，**不**切模式
6. 两个窗口各开一个会话 ⇒ 在 A 里按裸 `Shift` ⇒ 只切 A，B 不变

---

## 4. 环境障碍（不是代码问题，别去"修代码"）

### 4.1 沙箱权威门禁本轮跑不起来

- 工装：`F:\soft\00selfmade\sandbox-verify\rime-claude\`（`run-verify.cmd` 一键入口）
- 2026-10-25 那轮**成功**过：`PASS=38 FAIL=2 / 共 46`（E.2 无 D: 盘、E.4 无外网，是常驻环境事实）
- 2026-10-28 本轮**失败**：`3 次启动都没拿到 claims.json`，沙箱客户端一起来就消失，
  `sandbox-artifacts\rime\logs\` 里**连 boot.log 都没有** ⇒ 沙箱 VM 根本没起来
- 诊断到的线索：`Microsoft-Windows-Hyper-V-VmSwitch` 报 NIC「延迟删除操作请求一文件对象的
  非关闭操作」；`vmcompute`/`vmms`/`hvhost` 都在 Running，主机是裸机 ASUS（嵌套虚拟化应当可用）
- 下一会话若要重试：先 `Get-VM` 看有无残留沙箱 VM，清掉；必要时重启 `hns` + `vmms`（**有副作用**：
  可能短暂打断本机网络 / Docker / WSL，先问用户）
- 旧证据已归档为 `C:\Users\Duanyi\sandbox-artifacts\rime\FINAL-claims-20261025-pre090.{json,log}`
- **注意**：AGENTS.md 的"必须跑沙箱 harness"规则只对**改了 `output/install.nsi` 或
  `output/data/user-custom/**`** 的提交生效。本轮两者都没动 ⇒ 不强制。

### 4.2 本机静默装机的副作用（所以没在本机做）

`output/install.nsi` 安装时会对 TSF DLL 跑 `regsvr32`，并写 `HKLM\...\CTF\KnownClasses`、
`HKCU\...\CTF\Assemblies\0x00000804\{3D02CAB6-…}`、`HKLM\SOFTWARE\Microsoft\CTF\TIP\{A3F4CDED-…}`。
在开发机上静默装到临时目录 ⇒ **会把用户正在用的 `D:\Program Files\fluxing` 的输入法注册指到临时目录**。
所以走沙箱（虚拟机）而不是本机。真机验收由用户自己装。

### 4.3 MiniMax Code 运行时反复重启

1 小时内重启 6 次会杀掉在飞的后台任务（根因未定位，无 crash dump）。
已提报：`F:\soft\00selfmade\mavis-bug-reports\2026-10-26-runtime-restart-loses-background-task.md`

---

## 5. 下一步可以直接用的命令

```powershell
# 构建 / 测试
cmd /c "F:\soft\00selfmade\rime_claude\build.bat data"
cmd /c "F:\soft\00selfmade\rime_claude\build.bat weasel installer"
cmd /c "F:\soft\00selfmade\rime_claude\scripts\test-infra\run-test-suite.bat"

# 快捷键门禁（出货配置；scan 的每个文件都是一份配置副本，源 + 产物都要扫）
& "F:\soft\00selfmade\rime_claude\Release\TestHotkeyBindings.exe" --baseline `
  "F:\soft\00selfmade\rime_claude\test\TestHotkeyBindings\known-dead-bindings.txt" `
  "F:\soft\00selfmade\rime_claude\output\data\default.yaml" `
  "F:\soft\00selfmade\rime_claude\output\data\user-custom\rime_ice.custom.yaml" `
  "F:\soft\00selfmade\rime_claude\output\data\user-custom\double_pinyin_sogou.custom.yaml" `
  "F:\soft\00selfmade\rime_claude\output\data\user-custom\radical_pinyin.custom.yaml"

# 推送（远端名是 kizemo，不是 origin）
git -C "F:\soft\00selfmade\rime_claude" -c http.proxy= push kizemo Fluxing

# CI 复核
gh run list --repo kizemo/Fluxing --workflow ci.yml --limit 5
```

---

## 6. 本轮踩过的坑（别重试）

### 6.1 CI 的 C++ lint 门禁只查「本次 push 触及的文件的整个文件」

`scripts/lint-changed-cpp.sh` 用 `git diff --name-only $BASE HEAD -- '*.cpp' '*.h'` 取文件，
然后对每个文件跑 `clang-format --style=file --dry-run --Werror`。
⇒ **动 C++ 就必须把那个文件整体格式化干净**，哪怕你只改了一行。
本仓库的既定做法（先例 `4ab52db`）：单独一个 `style(fluxing):` 提交，body 写清「违规行数」。

- 本机 clang-format 在 `C:\Python314\Scripts\clang-format.exe`（pip 版 18.1.8），**CI 用的是
  apt 的 clang-format-18**（Linux）——版本对得上，但记得先把 clang-format 目录加进 PATH
- `.clang-format` 全历史只有**一个上游提交** `afecb64`，fork 没改过 ⇒ `BasedOnStyle: Chromium`
  + 80 列是既定配置，别去改它来"迁就"代码
- 拆两个提交的技巧：`clang-format(cat-file blob HEAD:<file>)` = 纯格式化版，
  `clang-format(工作区文件)` = 含功能版；先提交前者，再写后者
- `git cat-file blob HEAD:<path> > file` 必须走 `cmd /c`（cmd 的 `>` 是字节重定向；
  PowerShell 的 `>` 会写成 UTF-16）

### 6.2 PowerShell / 工具链的老坑（沿用）

- `Remove-Item` 被硬安全策略拦截 → 删除一律 `mavis-trash.cmd`，或换全新唯一目录
- bash 工具里 `cd X; ... 写 Y` 被硬拦 → 用绝对路径或 write 工具
- `cmd /c "cd /d X && ..."` 同样被拦（写目标无法校验）→ 用 `git -C`
- 构建进程持有日志独占句柄 → `[IO.File]::ReadAllText` 会报「正由另一进程使用」→ 用 `Get-Content -Tail N`
- PowerShell 内联 python 引号常被吃 → 改脚本一律用 write 工具
- 控制台输出中文/emoji 会 GBK 报错 → 重定向到 UTF-8 文件再读
- `Get-Content` 读 UTF-8 中文文件默认 cp936 解码 → 判断真伪用 UTF-8 方式读
- `Select-Object -First N` 会掐断管道；`-Last N` 可用
- `.NET` 的相对路径按会话工作区解析 → 读文件一律绝对路径
- git 读中文路径：`-c core.quotepath=false`；输出要 `encoding="utf-8"`
- `git` / `gh` 的 `sh: warning: setlocale` 会让 PowerShell 误判失败 → 用 `git log` 复核
- `gh run watch` 在 `sandbox-gate` 永久 queued 时永不退出 → 只盯 4 个门禁 job
- **不要连续轮询同一个后台任务**（运行时第 3 次读会发提醒）→ 用后台完成通知自动唤醒
- 写含中文的文件后必须做 U+FFFD 全量校验 + C0 控制符校验；每个文件保持 HEAD 的原始行尾风格
  （`RimeWithWeasel.cpp` / `RimeWithWeasel.h` 是 CRLF；`ShiftStateMachine.h` / 测试 cpp 是 LF）

---

## 7. 决策备忘（下一会话别推翻）

1. `accept:` 键名必须写「组合键产生的字符」的键名（WeaselTSF 语义），`send:` 必须是单个 ASCII 字符
2. known-dead 基线清零 = 最严阈值，空基线不是调松
3. 门禁扫描范围必须覆盖配置的全部副本（源 + 部署产物各扫一遍）
4. ADR 已 Accepted 后**不得改正文**（要改就新 ADR supersede）；新术语进 `CONTEXT.md` §3，
   改不变量要走宪法修正流程；实现计划放 `.specify/specs/NNN-*/`
5. 注释里凡是断言「生效/不生效/数量」的，都必须有门禁或测试数字背书
6. 文档批量更新走「幂等 Python 补丁脚本 + 锚点强校验 + 自动保 EOL/BOM」，不用 edit 逐条手改
   （脚本：`F:\soft\00selfmade\tmp-dllcmp\patch_docs.py`，幂等，可重跑）
7. spec 的 tasks 勾选框不可信 —— 以 CHANGELOG + 门禁数字为准
8. 验收门禁要指向**部署产物**再跑一遍（源绿 ≠ 产物绿）
9. 用户要求：按清单推进开发并**持续更新清单**；到需要实机验收的节点就**打包 exe、告知路径、
   提醒验收**；在**合适的实机节点自动 commit，不用请示**

---

## 8. 用户当前待办

- 装 `F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.21.0.9-installer.exe`
  （SHA256 `D1FCF78304FB89955F61F8925C894AB1F2CA5941FF9E9283D859D93A3E820E1C`）
- 按 §3.1 六条做真机按键验收
- 装完顺手把共享 `data/` 目录升级到 0.21.0.9（清单 T17）
