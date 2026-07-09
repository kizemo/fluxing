---
name: api-and-interface-design
description: Design stable, well-documented interfaces that are hard to misuse. Use when designing new API endpoints, module boundaries, type contracts, or changing public interfaces.
---

# API and Interface Design

> Good interfaces make the right thing easy and the wrong thing hard. This applies to REST/GraphQL/IPC, module boundaries, component props, type contracts.

## When to use

- Designing new IPC message types (Rime/Weasel: see code-map §2.3)
- Module boundaries (WeaselTSF ↔ WeaselServer ↔ WeaselDeployer)
- Component props (FluxingComponents: Button/Toggle/Panel/Label)
- Changing existing public interface (breaking change)

## When NOT to use

- Internal helpers
- Private members
- Throwaway code

## Core principles

### Hyrum's Law

> With sufficient users, all observable behaviors of the API will be depended on by somebody, regardless of what you promise in the contract.

Implication: every public field, every return code, every timing detail IS the contract.

### Postel's Law (with care)

> Be liberal in what you accept, conservative in what you send.

For RIME/Weasel: tolerate old client/server combos (semver), but never change `distribution_code_name` in the protocol.

### Single Responsibility

One interface, one concern. Don't put "commit composition" and "deploy RIME" in the same function.

## Rime/Weasel-specific

### IPC message design (P2 hard rule)

- All new IPC message types MUST be added to BOTH:
  - `include/WeaselIPCData.h` (wire format)
  - The receiving side (ResponseParser + Deserializer)
- Format: `key=value` per line, `.` terminator
- Action: first segment is action (`commit`, `context`, `status`, `style`, `config`)
- New action types need spec.md + plan.md (R2/R7)

### RimeWithWeaselHandler public methods

- `Initialize / Finalize / AddSession / RemoveSession / ProcessKeyEvent / ...`
- Each has a clear contract (see code-map §4.6)
- Adding a new method = R7 single source of truth

### FluxingComponents widget API

```cpp
// Pattern: Create returns widget, then SetOnClick for callback
auto btn = FluxingButton::Create(parent, rect, L"中/英", Style::Primary);
btn->SetOnClick([this]() { ToggleAscii(); });
```

API conventions:
- `Create(parent, rect, ...)` returns widget pointer (never raw HWND)
- `SetOn*` methods for callbacks (no `WndProc` exposed)
- `Style::` enum (Primary/Secondary/Destructive/Current)
- `Show/Hide/Destroy` for lifecycle

## Anti-patterns

- Function that returns multiple types (union or out-param)
- Boolean param that changes behavior (`Process(silent=true)`)
- Stringly-typed API (`mode = "ascii"`)
- Implicit ordering (`Initialize()` then `Connect()` then `Start()` — explicit)
- Hidden side effects (constructor that opens a file)
