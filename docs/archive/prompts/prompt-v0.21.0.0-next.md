承接 `handoff-v0.21.0.0-release-2026-08-10.md` (项目根, 必读顺序 §6.7 列了 5 个真相源) — Fluxing v0.21.0.0 milestone 已 ship + SFTP atomic upload + cross-source verify OK, 4 commits (792cb47/feac4f5/54909ae/29f0c61) + tag `v0.21.0.0` (annotated) 本地 HEAD=792cb47, branch Fluxing vs kizemo/Fluxing 944 ahead/831 behind 极大 divergence 未 push。

**下步必交付 (按优先级)**:
- P0: 处理 divergence + push v0.21.0.0 tag + 4 commits (用 `git rebase` 或 `git push --force-with-lease` — **必须 user 二次确认后才 push**)
- P1: v0.21.0.1 ship: `output/start_service.bat` 内容反 fix (`weaselserver.exe /q` → `start "" WeaselServer.exe`) + `output/data/user-custom/*.custom.yaml` key_binder `send: "《"` 等中文 parse error fix (librime `Send` 期望 key sequences not literal text, 改用 `send_text` 或重写为 `punctuator/half_shape` patch)
- P1: 并发压测验证: 装 v0.21.0.0 后同时开 Claude Code + Explorer + dopus + 微信 + WeaselServer 重启, 来回切火流猩 5+ min, 看 `C:\fluxing-dumps\` 是否新增 0xc0000374 dump (spec 076 race fix 真不触发才算 PASS)
- P2: 跑 `sandbox-verify\rime-verify.ps1 -InstallerPath release\fluxing-0.21.0.0-installer.exe`

**禁止事项**:
- 重新跑 `_nsis_v065.cmd` rebuild v0.21.0.0 installer (source 不变, 只会生成相同 md5)
- 改 `output/install.nsi` (spec 076 race fix + F3/F4 reliability 都验证 PASS, 不能动)
- 删 `release/fluxing-0.20.0.{0,1,3,4}-installer.exe` (release history per `feedback_release_output_rule.md`)
- 改 `librime` submodule (untracked deps noise 等下次正式 upgrade)
- 在 `D:\Program Files\Fluxing\weasel\` 跑 `uninstall.exe` (会破坏装机 state)
- 直接 `git push` (divergence 944/831 极大, 需 rebase + user 二次确认)

**加速模式规则 (per CLAUDE.md)**:
- 多任务并行 → `superpowers:dispatching-parallel-agents`
- ship gate 必 verify → `superpowers:verification-before-completion`
- 任何 silent-failure 代码 (file replace / registry write / mutex acquire) → 加 IfErrors 检测 + retry + 备用路径 (L##-PhaseM 关键教训)
- ship 前 md5 dual-verify 是 ship-gate (extract archive md5 == user 装机路径 md5)