---
paths:
  - "Weasel**/Hotkey*"
  - "Weasel**/Key*"
  - "Weasel**/Shift*"
  - "test/Test*Hotkey*"
  - "test/Test*Shift*"
  - "test/Test*Binding*"
  - "test/TestDefaultHotkeys/**"
  - "test/TestShiftSelectBinding/**"
  - "test/TestCandidate*"
  - "test/TestResponseParser/**"
---

# Hotkey / Shift / Binding

此层在 0.18.x 多次被打破 (spec 012 / 014 / 018 / 019; L18 / L19 / L21)。
"hotkeys behave wrong" 或 "shift doesn't do X" → 先读本文件。

## 双层语义 (这是项目特有的)

- `default.yaml` 的 `ascii_composer/switch_key/Shift_L: noop` + `Shift_R: noop`
  — **必须**, 中和 librime 默认 Shift 行为。
- 实际的 shift-select (e.g. 上屏第 2 / 第 3 候选) 在 `key_binder/bindings` —
  不是 `switch_key`, 两层在 librime 里有不同语义。

## Hotkey 单一真相

`WeaselIPC::Client::HotkeyBinding` 单 source of truth, 通过 IPC pipe 共享。
不要在 `WeaselTSF` / `WeaselServer` / `WeaselDeployer` 维护私有 hotkey 表 — 一周内会漂移并产生 phantom shifts。

## Shift 候选上屏 (spec 014) — 必须通过

`WeaselIPCData.h::Hotkey` 激活;不要直接调 librime。

## Tests 必须 pass

`test/TestDefaultHotkeys` (hotkey 表) · `test/TestShiftSelectBinding` (spec 014) ·
`test/TestBindingResolution` (复合键) · `test/TestCandidateIgnoreFilter` ·
`test/TestCandidateRButtonDown` · `test/TestUserDictUpdate` ·
`test/TestResponseParser`。

## Anti-patterns

- 给 `Shift_L` 在全局 `default.yaml` 写 `noop` 而非 `switch_key` — 破坏其他 schemas。
- 在新 binding 上改变 TSF-thread / IPC-thread 分工 — P2 仍生效;shift 走 key event, 不直接 post TSF。
- YAML 层 binding 编辑与 C++ hotkey 代码同 commit — 不同 reviewer、不同测试, 应拆。
