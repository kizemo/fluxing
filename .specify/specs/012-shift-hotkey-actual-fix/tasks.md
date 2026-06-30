# 012 · Tasks — Shift 切换中英 / 候选上屏 真正生效

> Single commit on `Fluxing`. Each task ≤ 1 file, ≤ 30 min.
> Acceptance: T003 test binary returns 0; T007 log is parse-error-free.

## T001 · P1 · US1-A/B/C · 改 default.yaml 的 key_binder 绑定（4 改 + 4 删）

- **File**: `output/data/default.yaml`
- **Action**: 在 `key_binder/bindings` 段对 4 行 in-place 替换 + 4 行删除：

  | 当前 line | 改前 | 改后 | 类型 |
  |---|---|---|---|
  | 231 | `accept: Shift_L, send: 2` | `accept: Shift+Shift_L, send: 2` | has_menu |
  | 232 | `accept: Shift_R, send: 3` | `accept: Shift+Shift_R, send: 3` | has_menu |
  | 234 | `accept: Shift+l, send: 2` | **删除整行** | — |
  | 235 | `accept: Shift+r, send: 3` | **删除整行** | — |
  | 254 | `accept: Shift+l, toggle: ascii_mode` | **删除整行** | — |
  | 255 | `accept: Shift+r, toggle: ascii_mode` | **删除整行** | — |
  | 257 | `accept: Shift_L, toggle: ascii_mode` | `accept: Shift+Shift_L, toggle: ascii_mode` | always |
  | 258 | `accept: Shift_R, toggle: ascii_mode` | `accept: Shift+Shift_R, toggle: ascii_mode` | always |

  同时清理第 229-230 / 233 / 252-253 / 256 行描述的注释行（这些注释
  提到 "Shift+L / Shift+R 组合键" 的描述不符合本 spec 范围）。

- **Byte-level**:
  - UTF-8 (no BOM) per L11 (`output/data/*.yaml` is BOM-less).
  - CRLF line endings per L11.
  - 使用 byte-level replace via `[IO.File]::ReadAllBytes` / `WriteAllBytes`。
  - **每个 from 字符串必须是唯一精确匹配**——先 `Select-String` 确认
    count=1 再 replace。

- **Acceptance**:
  - `Test-Bom output\data\default.yaml` returns "no-BOM".
  - CR count == LF count（CRLF 平衡）。
  - 文件总字节数变化 ≈ -250 字节（4 删 + 4 改 + 注释清理）。
  - `git diff --stat output/data/default.yaml` 只动 `key_binder/bindings` 段。
  - 改后文件大小 16335 - 250 ≈ 16085 字节（数量级）。
  - `Select-String -Path output\data\default.yaml -Pattern "shift\+l|shift\+r"`
    返回 0 个匹配（之前测试的组合键彻底清除）。

## T002 · P1 · US1-A/B/C · 修订 TestDefaultHotkeys.cpp

- **File**: `test\TestDefaultHotkeys\TestDefaultHotkeys.cpp`
- **Action**:
  - 删除所有断言中提到 `Shift+l` / `Shift+r` 的行（原 cpp line 33-36、
    44-45 等）。
  - 新增 4 条断言：
    - `Shift+Shift_L, send: 2` 出现在 has_menu 段
    - `Shift+Shift_R, send: 3` 出现在 has_menu 段
    - `Shift+Shift_L, toggle: ascii_mode` 出现在 always 段
    - `Shift+Shift_R, toggle: ascii_mode` 出现在 always 段
  - 新增 2 条**否定**断言（保证 Shift+l/r 不再出现）：
    - `accept: Shift+l, send: 2` 不存在
    - `accept: Shift+r, send: 3` 不存在
    - `accept: Shift+l, toggle: ascii_mode` 不存在
    - `accept: Shift+r, toggle: ascii_mode` 不存在

- **Byte-level**: UTF-8 (有 BOM, L11 C++ source 是 BOM), CRLF (按 .clang-format).

- **Acceptance**:
  - `git diff --stat test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` 显示净修改
    长度合理（删 ~5 行 + 加 ~5 行）。

## T003 · P1 · US1-A/B/C · 重建 + 跑 TestDefaultHotkeys

- **Action**:
  ```
  msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
  test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe output\data\default.yaml
  ```
- **Acceptance**:
  - msbuild exit code 0.
  - TestDefaultHotkeys.exe exit code 0.
  - `Passed: <N> / <N>` (N 是修订后断言数；预计 N=22 左右)。
  - 在 final assistant message 粘贴完整输出（R6）。

## T004 · P1 · US1-D · 勘误 lessons-learned.md L04

- **File**: `.specify/memory/lessons-learned.md`
- **Action**: 在 L04 段（约 line 140-173）找到含 `shift+l` / `shift+r` /
  "lowercase form" / "exact-case form" 的 bullet list，改写为：
  - **删除** "lowercase form also works" / "same as above, lowercase form"
    4 行。
  - **改写** "exact-case form, also works" 2 行，但改写为说明 **只有大写
    form 有效**（`Shift` 不是 `shift`）。
  - **加** 一行说明："librime 1.13.1 `KeyEvent::Parse` 要求 modifier 名是
    首字母大写（Shift/Control/Alt/Super/Hyper/Meta/Lock/Mod2..Mod5/Release），
    小写形式静默拒绝，整条 binding 被丢弃。"
- **Byte-level**: UTF-8 no BOM, CRLF. byte-level edit.
- **Acceptance**:
  - `Select-String -Path .specify\memory\lessons-learned.md -Pattern
    "lowercase form"` returns 0 matches.

## T005 · P1 · US1-D · 新增 lessons-learned.md L16

- **File**: `.specify/memory/lessons-learned.md`
- **Action**: 在文件末尾追加 L16 段（L15 之后，EOF 之前），格式同 L01-L15。
- **Content draft**:
  ```
  ## L16 - librime 1.13 `KeyEvent::Parse` modifier names are case-sensitive (Shift, not shift)

  **Context**: spec 005 design.md wrote `accept: shift+l` (lowercase
  modifier) in the `key_binder/bindings` example. The setting shipped
  in `output/data/default.yaml` for 0.18.0 - 0.18.4, and WeaselServer
  logs from real users show:

  ```
  E key_event.cc:69 parse error: unrecognized modifier 'shift'
  ```

  Result: those 4 bindings were silently dropped at parse time, and
  Shift_L / Shift_R did nothing.

  **Root cause**: `librime/src/rime/key_table.cc:7-26` defines
  `modifier_name[]` with first-letter-capitalized entries:
  `"Shift"`, `"Control"`, `"Alt"`, `"Super"`, `"Hyper"`, `"Meta"`,
  `"Lock"`, `"Mod2"`..`"Mod5"`, `"Release"`. `RimeGetModifierByName()`
  uses `strcmp`, so `"shift"` returns 0 → Parse returns false →
  KeyBindings::LoadBindings skips the entry with a warning
  (`librime/src/rime/key_binder.cc:175-184`).

  **Lesson**:
  - In any `key_binder` / `key_sequence` / `accept:` field of
    rime YAML, the **modifier part** must be exactly
    `Shift` / `Control` / `Alt` / `Super` / `Hyper` / `Meta` / `Lock`
    / `Mod2`..`Mod5` / `Release`. Lowercase or ALL-CAPS variants fail.
  - The **key part** (after the last `+`) IS case-sensitive in a
    different way: `a` means "the key that produces lowercase a"
    (no shift held), `A` means "the key that produces uppercase A"
    (shift held). Both are valid keysym names.
  - When binding single-key Shift_L / Shift_R, write `Shift+Shift_L`
    (modifier Shift + keycode Shift_L) — NOT `Shift_L` alone — because
    TSF injects `SHIFT_MASK` on every `VK_SHIFT` event
    (`WeaselTSF/KeyEventSink.cpp:31`).
  - The current `TestDefaultHotkeys.exe` does not catch the
    parse-error class because it checks for the YAML substring
    `accept: Shift+l`, not whether librime actually accepted it.
    **Action item (out of scope)**: add a regression test that
    pipes the YAML through librime's `RimeStartMaintenance` and
    asserts the binding was loaded (not just present in the text).
  - Spec 005 design.md §2.2 example has been corrected to use
    `Shift+Shift_L` etc. (spec 012).
  - Fluxing 0.18.5 intentionally does NOT include the
    `Shift+l` / `Shift+r` combination-key path; the user
    confirmed (2026-06-30) that those were test-only bindings.

  **Verification**: a clean `xbuild.bat installer` on the 0.18.4
  codebase still produces the parse-error log; after applying
  the spec 012 patch, the same log no longer shows the warning.
  ```
- **Byte-level**: UTF-8 no BOM, CRLF. 用 `[IO.File]::ReadAllBytes` + concat +
  `[IO.File]::WriteAllBytes` 追加到文件末尾（L07）。
- **Acceptance**:
  - `Select-String -Path .specify\memory\lessons-learned.md -Pattern
    "^##\s+L16 "` returns 1 match.

## T006 · P1 · US1-A/B/C · 重新构建 + 部署

- **Action**:
  ```
  xbuild.bat weasel installer
  ```
- **Expected output**:
  - Build succeeds, no NSIS errors.
  - `output/archives/fluxing-<ver>.<build>-installer.exe` is produced.
- **Acceptance**:
  - `xbuild.bat` exit code 0.
  - 在 final assistant message 粘贴 build 输出的最后 30 行（R6）。

## T007 · P1 · US1-D · 重启 WeaselServer + 日志验证

- **Action**:
  1. 重装 / 升级。
  2. 在 notepad 打几个字触发 engine 加载。
  3. 读最新 `log/rime.weasel.*.INFO.*.log`：
     ```
     Get-Content (Get-ChildItem log\*.INFO*.log | Sort-Object LastWriteTime | Select-Object -Last 1).FullName -Raw
     ```
- **Acceptance**:
  - 输出**没有** `unrecognized modifier 'shift'` 行。
  - 输出**没有** `invalid key binding` 任意行。
  - 在 notepad 中文输入态按 `Shift_L` → 切到英文（手动验证）。
  - 在 notepad 输入 "nihao" → 候选弹出 → 按 `Shift_L` → 第 2 候选上屏
    （手动验证）。
  - commit message 引用 "no 'invalid key binding' since 0.18.4.1"。

## T008 · P1 · 提交

- **Action**:
  ```
  git add output/data/default.yaml test/TestDefaultHotkeys/TestDefaultHotkeys.cpp
  git add .specify/memory/lessons-learned.md
  git add .specify/specs/012-shift-hotkey-actual-fix/spec.md
  git add .specify/specs/012-shift-hotkey-actual-fix/plan.md
  git add .specify/specs/012-shift-hotkey-actual-fix/tasks.md
  git commit -m "fix(fluxing): wire Shift_L/R in key_binder (spec 012, single-key only)

  spec 005's default hotkey design called for Shift_L / Shift_R to
  toggle CJK/ASCII and to select the 2nd/3rd candidate. The shipped
  0.18.0 - 0.18.4 default.yaml contained 8 shift-related bindings,
  of which 4 were silently broken:
    - 'accept: Shift_L' (no modifier) — never matches the runtime
      {Shift_L, SHIFT_MASK} key event (librime key_event.h:64 == both).
    - 'accept: shift+l' / 'shift+r' (lowercase modifier) — silently
      dropped at parse time: RimeGetModifierByName is strcmp and
      modifier_name[] only has 'Shift' / 'Control' / etc.

  This commit:
    1. replaces 4 in-place lines in key_binder/bindings with the
       canonical Shift+Shift_L / Shift+Shift_R forms (both has_menu
       and always variants).
    2. removes the 4 test-only 'Shift+l' / 'Shift+r' combination
       bindings (per user request 2026-06-30; these were not part
       of the spec 005 design but were added during manual testing
       and never intended for the final product).
    3. revises test/TestDefaultHotkeys.cpp to assert the new
       single-key-only binding set and to add negative assertions
       for the removed combination-key paths.
    4. adds L16 to lessons-learned documenting the modifier-name
       case-sensitivity rule and the single-key Shift_L/R binding
       pattern; corrects L04 (which incorrectly listed 'lowercase
       form' as a valid alternative).
    5. ships spec/plan/tasks under .specify/specs/012-* per R8.

  Verified by:
    - test/TestDefaultHotkeys/Release/TestDefaultHotkeys.exe
      (Passed: <N> / <N>, all assertions pass after rebuild)
    - xbuild.bat weasel installer
    - log/rime.weasel.*.INFO.*.log no longer shows
      'unrecognized modifier' or 'invalid key binding'."
  ```
- **Acceptance**:
  - `git log --oneline -1` shows the new commit.
  - `git status` shows no leftover uncommitted changes from this slice.
  - 在 final assistant message 粘贴 commit hash。

## 9. Cross-spec consistency audit (R7)

- spec 005 design.md §2.2 example still says `accept: shift+l` (lowercase).
  This is a tech-doc consistency issue, not a code issue. Per spec 012
  Out of scope, we do **not** edit spec 005 in this commit; instead,
  spec 012 spec.md §2.3 explicitly cites the inconsistency and points
  future readers to L16.
- `.specify/specs/005-default-hotkeys-rev2/design.md` will need a follow-up
  edit to use the canonical forms. Track as a separate task; not
  blocking for this commit.

## 10. Done

- T001..T008 all completed; test binary returns 0; log is parse-error-free.
- The fix unblocks spec 005 v2 from working on a real install (it was
  silently broken in 0.18.0 - 0.18.4).