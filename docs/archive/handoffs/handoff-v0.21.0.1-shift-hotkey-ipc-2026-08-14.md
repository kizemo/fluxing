# Handoff — v0.21.0.1 Shift hotkey IPC 重构 (2026-08-14)

> **任务**:实现「按 Shift 不松不上屏,松 Shift 上屏第 2/3 候选,Shift+字母 出大写字母不上屏」,通过 IPC hotkey 路径重构
> **基线**: v0.21.0.0 shipped (commit `3ab771c`),Fluxing HEAD 干净
> **当前代码**:全部 git HEAD 干净 revert,装机 DLL md5 `120d02e2`(git HEAD baseline)
> **本会话**:100% 调研 + 8 次失败尝试 → 现在交给新会话做 IPC 重构

---

## 0. 必须先读的资料(按顺序)

1. **本 handoff** (本文件)
2. **`Rime-设置说明书-重点快捷键.md`** — §6 (快捷键绑定 ⭐), §9 (本机现状), §10 (排错)
3. **`hotkey-binding.md`** — `.claude/rules/hotkey-binding.md` — **架构规则,不可违反**
4. **`.research/rime-docs/Rime-customization-digested.md`** — Rime 整体机制理解
5. **git log** — `git log --oneline -20` 了解近期变更

---

## 1. 用户最终诉求(已知,无须再问)

| # | 场景 | 预期 |
|---|---|---|
| ① | 有候选,按 Shift **不松** | 什么都不发生 |
| ② | 有候选,松 Shift(中间无其他键) | 上屏第 2 候选(左 Shift)/ 第 3 候选(右 Shift) |
| ③ | Shift+字母(有候选) | 出大写字母,松开 Shift **不上屏** |
| ④ | 无候选,Shift+空格 | 切中英文 |
| ⑤ | Shift+, | 出《 |

**重要**: 不要与用户确认,他们已经明确 — 选 B 继续。

---

## 2. 已尝试 8 次的失败路径(避免重蹈覆辙)

### 失败路径 1 — yaml-only 方案(`key_binder/bindings` 替换)
- 尝试:把 `default.yaml` 的 `Shift+Shift_L send 2` 替换成自定义 bindings
- 失败原因:用户测试「Shift 按下就上屏」 — 实际是 WeaselTSF 把 Shift down 事件加了 SHIFT_MASK,key_binder 必然匹配 `Shift+Shift_L`

### 失败路径 2 — `ascii_composer/switch_key` = `commit_text`
- 尝试:让 ascii_composer 接管 Shift release,在 500ms 内 commit + toggle
- 失败原因:用户说「上屏不正确(变成第 1 候选)」「有候选时还切中英文」

### 失败路径 3 — 直接改 build/*.yaml
- 尝试:大量 ad-hoc 编辑 build 目录下的产物
- 失败原因:build 在 rime 自动重建时被覆盖,既不持久又引入不一致

### 失败路径 4 — WeaselTSF/KeyEvent.cpp 改 mask 行为
- 尝试:Shift down 时 mask=0,Shift up 时 mask=SHIFT_MASK(无 RELEASE_MASK)
- 失败原因:MSI 修改导致场景 2 (Shift+字母) 故障 — 字母 a down 时 shiftHadInterveningKey 没被正确设置

### 失败路径 5 — `kill ctfmon` 性能灾难
- 失败原因:TSF 服务降级,输入法全局卡顿

### 失败路径 6 — WeaselTSF 加 `shiftHeld`/`shiftHadInterveningKey` 状态跟踪
- 失败原因:
  - 用户测出场景 2 仍有问题(「Shift+字母 按下就上屏」)
  - **违反 hotkey-binding.md 规则**:「不要在 WeaselTSF / WeaselServer / WeaselDeployer 维护私有 hotkey 表」

### 失败路径 7 — rebuild 配置强制重编译
- 失败原因:只是临时修复,治标不治本

### 失败路径 8 — 反复 review 仍未确认行为
- 失败原因:TSF 缓存使 DLL 部署延迟生效,用户重复测试结果不一致

**结论**:在 TSF 层的 mask 路径/状态路径 8 次尝试均无法稳定实现 release-only 触发。**唯一可行路径是 IPC hotkey 重构**(用户已选 B)。

---

## 3. 根因(新会话必须理解)

### 3.1 TSF 平台限制

**WeaselTSF/KeyEvent.cpp:ConvertKeyEvent** 当前实现(line 15-28):

```cpp
if ((keyState[VK_SHIFT] & KEY_DOWN) != 0)
    result.mask |= ibus::SHIFT_MASK;

if (kinfo.isKeyUp)
    result.mask |= ibus::RELEASE_MASK;
```

### 核心矛盾

| 事件 | mask (ibus) | expand → rime modifier | 能否匹配 `Shift+Shift_L`(期望 modifier=kShiftMask=1) |
|---|---|---|---|
| Shift down | `SHIFT_MASK`(1) | 1 | ✅ 匹配 → send 2 上屏 |
| Shift up | `RELEASE_MASK`(0x4000) | `1<<30` (kReleaseMask) | ❌ 不匹配改 |
| 字母 a down (Shift held) | `SHIFT_MASK`(1) | 1 | 只匹配 `Shift+a` |
| 字母 a up (Shift held) | `SHIFT_MASK\|RELEASE_MASK`(0x4001) | `1\|1<<30` | ❌ 不匹配 |

**Shift down 必然 mask=SHIFT_MASK** → key_binder 必然匹配 → **TSF 层无法区分 down/release**。

### 3.2 rime 端限制

- `key_binder/bindings`:`KeyEvent::operator==` (librime/src/rime/key_event.h:43) 只比较 keycode + modifier, 不区分 release flag
- `accept: Release+Shift+Shift_L` Parse:`Release` 修饰符变成 `1<<30`,Shift down 事件的 modifier=1,**永远不匹配**
- `ascii_composer/switch_key`:任何 style 都会 toggle ascii_mode,无法做到 「有候选时只 commit 不 toggle」

### 3.3 架构约束 (hotkey-binding.md)

1. **`WeaselIPC::Client::HotkeyBinding` 单 source of truth**,通过 IPC pipe 共享
2. **不要在 WeaselTSF / WeaselServer / WeaselDeployer 维护私有 hotkey 表**
3. **Shift 候选上屏必须通过 `WeaselIPCData.h::Hotkey` 激活**
4. yaml 层的 binding 改动与 C++ hotkey 代码不同 commit,不同 reviewer

---

## 4. 推测的最小化代码修改方案(新会话验证)

### 4.1 假设:IPC hotkey 路径可以复用现有 select_candidate API

**rime_api.h:436** 已有:
```cpp
Bool (*select_candidate)(RimeSessionId session_id, size_t index);
```

**WeaselServer 端可调用 `rime_api->select_candidate(session_id, 1)`** 选第 2 个候选(0-indexed)。

### 4.2 最小化方案设计

**修改 1: WeaselTSF/KeyEventSink.cpp (新状态跟踪 — input state, NOT hotkey 表)**
- 维护 `shiftHeld` + `shiftHadInterveningKey` (实际上是 system input state,不是 hotkey 表)
- Shift down/up 时通过现有 IPC 通知 WeaselServer

**修改 2: WeaselIPCData.h (新增 IPC 消息类型)**
- 新加 `WEASEL_IPC_SHIFT_DOWN` / `WEASEL_IPC_SHIFT_UP` 消息
- 或扩展现有 `KeyEvent` 消息带 `shiftAloneRelease` 标志

**修改 3: WeaselServer 端 (WeaselServerApp.cpp / RimeWithWeaselHandler)**
- 处理 Shift 状态消息
- Shift up 且 `!shiftHadInterveningKey` 且 `rime_api->get_option(session, "has_menu")`:
  - 调用 `rime_api->select_candidate(session_id, 1)` (左 Shift) 或 `select_candidate(session_id, 2)` (右 Shift)
- **同时**:key_binder 的 `Shift+Shift_L send 2` binding 必须删除(避免重复触发)

**修改 4: yaml (default.yaml)**
- 删除 `Shift+Shift_L send 2` / `Shift+Shift_R send 3` 两个 binding
- 改由 IPC 实现

### 4.3 风险点

- IPC 消息延迟(TSF → Server) 延迟大约 5-10ms,可能影响「down→up 500ms 内」判定
- `select_candidate` 是同步调用,但 IPC 异步 → 选候选可能在错误时刻
- rime_ice 还有 `select_first_character: bracketleft` lua_processor,可能干扰

### 4.4 新会话必须先验证

- 现有 IPC 机制是否能支持自定义消息类型(看 WeaselServerApp.cpp 是否有 message handler 扩展点)
- `rime_api->select_candidate` 是否能通过 RimeWithWeaselHandler 暴露
- TSF 端是否需要改 `KeyEventSink.cpp` 现有的 `_ProcessKeyEvent` 调用顺序

---

## 5. 关键文件位置(新会话按需 Read)

### 核心源文件
- `F:/soft/00selfmade/rime_claude/WeaselTSF/KeyEvent.cpp` — ConvertKeyEvent(mask 计算)
- `F:/soft/00selfmade/rime_claude/WeaselTSF/KeyEventSink.cpp` — _ProcessKeyEvent(IPC 入口)
- `F:/soft/00selfmade/rime_claude/WeaselTSF/KeyEvent.h` — KeyEvent 结构
- `F:/soft/00selfmade/rime_claude/WeaselServer/WeaselServerApp.cpp` — IPC 服务端
- `F:/soft/00selfmade/rime_claude/WeaselServer/WeaselServerApp.h`
- `F:/soft/00selfmade/rime_claude/RimeWithWeasel/RimeWithWeasel.cpp` — IPC 客户端 ← rime 调用
- `F:/soft/00selfmade/rime_claude/RimeWithWeasel/RimeWithWeasel.h`
- `F:/soft/00selfmade/rime_claude/include/WeaselIPCData.h` — IPC 消息结构
- `F:/soft/00selfmade/rime_claude/include/KeyEvent.h` — weasel::KeyEvent
- `F:/soft/00selfmade/rime_claude/librime/src/rime_api.h` — rime C API
- `F:/soft/00selfmade/rime_claude/librime/src/rime/key_event.{h,cc}` — KeyEvent operator==
- `F:/soft/00selfmade/rime_claude/librime/src/rime/gear/key_binder.cc` — binding 解析
- `F:/soft/00selfmade/rime_claude/librime/src/rime/gear/ascii_composer.cc` — switch_key 行为

### 配置文件
- `F:/soft/00selfmade/rime_claude/output/data/default.yaml` — 目前含 `Shift+Shift_L/R send 2/3` (需要删除)
- `F:/soft/00selfmade/rime_claude/output/data/rime_ice.schema.yaml` — 干净 baseline
- `F:/soft/00selfmade/rime_claude/output/data/user-custom/rime_ice.custom.yaml` — 干净 baseline
- `F:/soft/00selfmade/rime_claude/output/data/user-custom/double_pinyin_sogou.custom.yaml` — 干净 baseline

### 装机路径(用户数据)
- `D:/Program Files/fluxing/weasel/data/` — 装机源 yaml
- `D:/Program Files/fluxing/weasel/weasel.dll` — 装机 TSF DLL (md5 `120d02e2`,git HEAD baseline)
- `D:/Program Files/fluxing/weasel/WeaselServer.exe` — IPC 服务端
- `D:/Program Files/fluxing/user1/fluxing/` — rime 用户数据(registry 指向)
- `D:/Program Files/fluxing/user1/fluxing/build/` — rime build 产物

### 文档
- `F:/soft/00selfmade/rime_claude/Rime-设置说明书-重点快捷键.md` — §6 / §9 / §10
- `F:/soft/00selfmade/rime_claude/.claude/rules/hotkey-binding.md` — **架构规则**
- `F:/soft/00selfmade/rime_claude/.claude/rules/codebase-graph.md` — 图谱查询规则
- `F:/soft/00selfmade/rime_claude/.research/rime-docs/Rime-customization-digested.md` — Rime 机制总览

### 测试
- `F:/soft/00selfmade/rime_claude/test/TestShiftSelectBinding/` — spec 014 测试
- `F:/soft/00selfmade/rime_claude/test/TestDefaultHotkeys/` — hotkey 表测试
- `F:/soft/00selfmade/rime_claude/test/TestPipeChannelRace/` — IPC pipe 竞态测试(spec 076)

---

## 6. 部署与测试工具(新会话复用)

- `_buildflow.cmd` — 全量 Release 编译(`F:/soft/00selfmade/rime_claude/`)
- `_deploy_clean.ps1` — 部署 DLL(rename + Copy + regsvr32 + 重启 WeaselServer)
- `_deploy_v02101.ps1` — 上次部署脚本(已修复)
- `_check_dll.ps1` — 验证 DLL md5 和 TSF 进程加载状态
- `F:/soft/00selfmade/rime_claude/sandbox-verify/` — **sandbox 验证(必做)**

**TSF 强制重载 DLL**:**用户必须 Win+L 锁屏 + 解锁**(DLL 进程缓存)。

---

## 7. spec 编号与 git

- v0.21.0.0 shipped: commit `3ab771c` (验证 PhaseM milestone)
- 当前 HEAD: `Fluxing` 分支,ahead 1(我有 working tree 改动但未 commit)
- 推荐为本次 IPC 重构开新 spec: `.specify/specs/076-shift-release-ipc/` 或 `077-`

**当前未 staged 改动**(`git status`):
- `CLAUDE.md` 修改(本会话相关)
- `librime` 修改(子模块,本会话相关)
- 多个 `??` untracked 项目(本会话无关,不需要管)

---

## 8. 必交付(新会话目标)

1. **架构设计文档** — IPC 消息设计、状态机、消息流图
2. **最小化代码修改** — WeaselTSF/KeyEventSink.cpp 状态上报 + WeaselServer 接收 + 处理
3. **yaml 改动** — `default.yaml` 删除 `Shift+Shift_L/R send 2/3` binding(交给 IPC)
4. **测试覆盖** — spec 014 测试 + 新 IPC 路径测试
5. **spec 文档** — `.specify/specs/N77-shift-release-ipc/spec.md`(至少 brainstorming + plan)
6. **rebuild installer** — v0.21.0.1 installer md5 dual-verify
7. **sandbox-verify** — 真机验证 5 个场景
8. **commit** — Conventional Commits + 模块前缀

---

## 9. Anti-patterns (踩过的坑)

- ❌ 在 WeaselTSF 加 `shiftHeld` 状态跟踪 — 违反 hotkey-binding.md
- ❌ 改 `KeyEvent.cpp` 的 mask 行为 — 治标不治本,8 次失败
- ❌ `kill ctfmon` 强制 TSF 重载 — 性能灾难,TSF 服务降级
- ❌ 直接改 `build/*.yaml` — build 重建时丢失
- ❌ 改 `protobuf` 端的 cast / `git add -A` / 删 `release` 历史 installer(per 用户的硬规则)
- ❌ 把过往会话的 send_text binding 改动混到本次 commit(per AGENTS.md / git-workflow)
- ❌ 重新构建时同时改 librime 子模块 — 单独 commit librime 改动(per CLAUDE.md 强制)

---

## 10. 接下来的第一步(新会话)

按 superpowers:brainstorming 启动新会话,先做:

1. 读本 handoff + 说明书 §6/§9 + hotkey-binding.md
2. 用 `codebase-memory-mcp` (`F-soft-00selfmade-rime-claude`) 搜索:
   - `search_graph(query="WeaselIPC::Hotkey", "IPC message dispatch")` 找现有 hotkey 路径
   - `trace_path("CreateSession","select_candidate")` 找 rime 入口
   - `get_architecture()` 看全局架构
3. 制定最小化修改方案(可参考 handoff §4,但**必须自己验证**)
4. 写 spec.md + plan.md
5. 实现 → 测试 → sandbox-verify → installer 打包

---

**准备好!下一会话第 1 句话直接复制 `prompt-v0.21.0.1-shift-hotkey-ipc-next.md`**。
