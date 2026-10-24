承接 `handoff-install-nsi-d-override-2026-08-17.md`。

**基线**: main `0184481` (v0.21.0.1 ship) | fix branch `fix/install-nsi-d-override-2026-08-17` HEAD = `b3fba97` (未 push)。

**必交付** (按优先级):
1. **真机验证 install.nsi fix** — 5 个 install 场景,关键看「升级 + /D=path」(行为矩阵第 4 行)。不依赖 sandbox。
2. 验证通过 → push fix branch 到 kizemo
3. 评估是否 bump v0.21.0.2 — 只有验过 fix 才有意义
4. (可选) sandbox-verify T20 8 场景 — 仅在真机验证通过后做,避免 sandbox flake 误报
5. spec 077 completion 报告 + 新 handoff

**禁止**:
- ❌ 修改 spec 077 已 ship 的代码 / yaml / IPC
- ❌ commit env.bat / weasel.props / installer binary
- ❌ 在没验证前 push fix branch
- ❌ 在 fix branch 上做 spec 077 completion / 版本 bump
- ❌ 进入 fix-and-break 循环(用户明确警告过,见 `feedback_avoid_fix_break_loop.md`)

**加速规则**: 用户的 fix-and-break 疲劳是真的,真机验证跑完 5 场景就停;不要主动展开其他「改进」。