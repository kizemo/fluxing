# Architecture Decision Records

This directory holds Architecture Decision Records (ADRs) for the
**Fluxing / 火流猩输入法** brand fork of `rime/weasel`. ADRs are
short, durable records of an architectural choice: what was decided,
what context forced the decision, and what became easier or harder
because of it.

ADRs are **post-hoc records**, not pre-implementation plans. The
implementation plan lives in `.specify/specs/NNN-*/`; the ADR captures
*why* the plan was chosen, in language that outlives the plan and
the spec.

Engineering skills read this directory opportunistically — a skill
only consults ADRs that are topically relevant to the current task.
Do not dump all ADRs into every prompt.

## ADR index

| Number | Title | File | Status |
| --- | --- | --- | --- |
| 0001 | ADR 0001 — C++11 thread_local replaces boost::thread_specific_ptr in PipeChannel | `docs/adr/0001-thread-local-replaces-boost-tss.md` | Accepted |
| 0007 | 0007. Shift release-only candidate select via IPC state machine | `docs/adr/0007-shift-release-ipc.md` | Accepted |
| 0008 | 0008. The IME profile switch is reported by the TSF over a dedicated notification thread and gated by the Server's active session | `docs/adr/0008-ime-profile-activation-notification.md` | Accepted |
| 0009 | 0009. The TSF front end sends the produced character, not the physical key | `docs/adr/0009-tsf-frontend-sends-character-not-key.md` | Accepted |
| 0010 | 0010. Bare Shift toggles Chinese/English when no candidate menu is open | `docs/adr/0010-bare-shift-toggles-ascii-mode.md` | Accepted |
| 0011 | 0011. The settings dialog captures keys by the character they actually produce | `docs/adr/0011-capture-keys-by-produced-character.md` | Proposed |
| 0012 | 0012. The Shift IPC answers with a response body, and a bare Shift never leaves its window | `docs/adr/0012-shift-ipc-response-body-and-session-local-ascii.md` | Accepted |
| 0013 | 0013. The QuickPanel fades through a per-pixel opacity factor, and always-show is an opt-in mode | `docs/adr/0013-quickpanel-always-show-alpha-factor.md` | Proposed |
| 0014 | 0014. The front end stops reporting the CapsLock latch state as a key modifier | `docs/adr/0014-front-end-stops-reporting-capslock-state.md` | Proposed |
| 0015 | 0015. The Shift left/right convention gets one source of truth, and the front-end popup menu gets the always-show entry | `docs/adr/0015-shift-direction-single-source-and-tsf-popup-menu.md` | Proposed |
| 0016 | 0016. 快速面板「常驻」的焦点守卫、64 位前端换装闭环，以及「先取事实再改码」的分期 | `docs/adr/0016-quickpanel-focusout-guard-and-x64-swap.md` | Proposed |
| 0017 | 0017. 裸 Shift 左右方向：前端改用物理扫描码判定，服务端加「左右互换」开关 | `docs/adr/0017-shift-side-scan-code-first-and-swap-switch.md` | Proposed |
| 0018 | 0018. 常驻面板的 idle 透明度地板与裸 Shift 部分选词的 flush 判据 | `docs/adr/0018-always-show-idle-alpha-and-partial-selection-flush.md` | Proposed |
| 0019 | 0019. 快捷面板常驻态跨进程持久化，且 ESC 不再收起面板 | `docs/adr/0019-quickpanel-always-show-persist-and-esc-no-hide.md` | Accepted |
| 0020 | 0020. 快捷面板常驻态文件落点跟随 fork 用户数据目录，旧路径只读兜底 | `docs/adr/0020-quickpanel-state-path-follows-user-data-dir.md` | Accepted |
| 0021 | 0021. 前端程序目录解析收敛为单一事实源：显式遍历 32/64 注册表视图 + `InstallDir → <InstallDir>\weasel` | `docs/adr/0021-weasel-program-root-resolution.md` | Accepted |
| 0022 | 0022. 快捷面板「常驻」改为默认态 + 静止态整块 85% 不透明 + 「重启输入法服务」走脱离式 helper | `docs/adr/0022-quickpanel-default-resident-flat-idle-alpha-detached-service-restart.md` | Accepted |
| 0023 | 0023. Shift 选词改走页内索引 + 候选屏蔽的过滤点上移到服务端 | `docs/adr/0023-shift-page-local-select-and-server-side-candidate-ignore.md` | Accepted |
| 0024 | 0024. 快捷设置栏的显隐跟随输入法激活状态（并从语言栏菜单移除「快速面板常驻」） | `docs/adr/0024-quickpanel-visibility-follows-ime-activation.md` | Proposed |

- Numbers `0002`-`0006` are **unused**: no ADR file has ever carried
  those numbers (`git log --diff-filter=D -- docs/adr/` returns nothing, and
  `git ls-tree HEAD docs/adr/` lists only the files above). Pick the next
  free number rather than filling the gap.
- The **Status** column is copied from each ADR's own status field. The
  label differs by generation (`- **Status**:` in 0007-0013,
  `- **状态**:` in 0014-0024, a `## Status` section in 0001), so read the
  ADR itself for the authoritative value.

## When to write an ADR

Write one when the decision:

- pins the project to a constraint that future changes will have to
  honor (e.g. "we ship only x86 because of PPL + librime Win32-only"),
- introduces a new vocabulary term that belongs in `CONTEXT.md` §3,
- changes an existing invariant (`CONTEXT.md` §4) or constitution
  principle (`.specify/memory/constitution.md`),
- is the kind of choice where "we did this six months ago, why?"
  is a real question future-you (or a future skill) will ask.

Do **not** write an ADR for:

- a bug fix with a clear root cause (use `.specify/memory/lessons-learned.md`),
- an implementation detail inside a single module (use a code comment),
- a routine feature spec (use `.specify/specs/NNN-*/`).

## File format

Filename: `NNNN-kebab-case-summary.md`, zero-padded to four digits.
Start at `0001`. Pick the next number that does not collide with an
existing file in this directory.

Each ADR is **self-contained** and uses the four-section MADR shape:

```markdown
# NNNN. Title

- **Status**: Proposed | Accepted | Superseded by NNNN
- **Date**: YYYY-MM-DD
- **Deciders**: <name, agent, or "Fluxing maintainers">
- **Related**: <issue #>, <spec NNN>, <PR #> (any of these, as applicable)

## Context

What was the situation? What forces were at play? What constraints
(constitutional, technical, user-data) shaped the choice?

## Decision

What was chosen? State it in the imperative ("We …", "The fork …").
If alternatives were considered, list them with a one-line rejection
reason each.

## Consequences

What becomes easier? What becomes harder? What new constraints does
this impose on future changes? If the decision is reversible, say so;
if it is not, say *that* too.
```

### Worked example (skeleton, do **not** add unless a real decision matches)

```markdown
# 0001. WeaselServer runs as Protected Process Light on Windows

- **Status**: Accepted
- **Date**: 2026-07-09
- **Deciders**: Fluxing maintainers
- **Related**: constitution §P7, lessons L17, L18

## Context

WeaselServer supervises the RIME engine and must survive arbitrary
user logon activity. On Windows, PPL prevents standard user processes
from terminating or injecting into the supervisor, which protects
user-dictionary integrity against tampering by malware running as the
same user. The cost is that legitimate replacement of WeaselServer.exe
during an upgrade also fails — `taskkill /F` is denied, and the new
binary will not load until the user logs out or reboots.

## Decision

WeaselServer.exe ships as a PPL binary. Installer upgrades replace
the file on disk but the running process is replaced only at the next
logon or reboot. The user is informed via a "logout to apply update"
prompt at the end of the installer.

## Consequences

- ✅ User-dictionary integrity is preserved against same-user tampering.
- ✅ Crash-recovery is simpler (WeaselServer cannot be killed mid-deploy).
- ⚠️ Inner-loop testing of WeaselServer changes requires a logout/reboot
  per iteration; this is the dominant cost in PPL-aware development.
- ⚠️ Auto-updates cannot apply in-session; users on long-running RDP
  sessions must explicitly log out for upgrades to take effect.
```

## Lifecycle

- `Status: Proposed` — drafted but not yet adopted; expect revisions.
- `Status: Accepted` — adopted. This is the canonical record of the
  decision. Do not edit the body of an accepted ADR; supersede it
  with a new ADR that links back.
- `Status: Superseded by NNNN` — replaced by a later decision. The
  file stays in the directory for history; do not delete.

## Cross-references

- **Vocabulary updates**: if an ADR introduces a new term, add it to
  `CONTEXT.md` §3 in the same change.
- **Invariant updates**: if an ADR changes a rule in `CONTEXT.md` §4
  or `.specify/memory/constitution.md`, the change is **major** and
  requires an amendment to the constitution (governance §Amendment
  Process) and a fresh ADR.
- **Spec linkage**: if a spec implements a decision recorded here,
  cite the ADR number in `spec.md` and `plan.md`.

## See also

- [CONTEXT.md §7](../CONTEXT.md#7-adrs-architecture-decision-records) —
  the consumer-facing description of this directory
- [AGENTS.md §9.3](AGENTS.md#9-agent-skills) — the engineering-skills
  pointer to this directory
- [docs/agents/domain.md](docs/agents/domain.md) — the consumer rules
  for `CONTEXT.md` and `docs/adr/`
- [.specify/memory/constitution.md](.specify/memory/constitution.md) —
  binding principles and the amendment process
