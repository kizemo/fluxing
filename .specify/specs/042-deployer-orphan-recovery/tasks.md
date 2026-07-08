# spec 042 任务清单 - Deployer 孤儿任务恢复（v0.18.30+）

## P1 (must)

- [ ] T001: 建 spec 目录 `.specify/specs/042-deployer-orphan-recovery/`
- [ ] T002: 写 `spec.md` (intent + 6 根因 + 4 user story + 验收)
- [ ] T003: 写 `plan.md` (技术方案 = Fix 1 + Fix 2 + Mock 测试)
- [ ] T004: 新建 `WeaselDeployer/MaintenanceGuard.h` (RAII 守卫 ~30 行)
- [ ] T005: `Configurator.cpp::UpdateWorkspace()` 改用 MaintenanceGuard
- [ ] T006: `Configurator.cpp::DictManagement()` 改用 MaintenanceGuard
- [ ] T007: `Configurator.cpp::SyncUserData()` 改用 MaintenanceGuard
- [ ] T008: `Configurator.cpp::DictManagement()` 在 run_task 后加 `join_maintenance_thread()`
- [ ] T009: 追加 L55 到 `lessons-learned.md` (Deployer 孤儿任务场景)
- [ ] T010: 新建 `test/TestOrphanRecovery/TestOrphanRecovery.cpp` (5 个 mock test case)
- [ ] T011: 新建 `test/TestOrphanRecovery/{TestOrphanRecovery.vcxproj, xmake.lua, stdafx.h, stdafx.cpp, targetver.h}`
- [ ] T012: 修改 `weasel.sln` 注册新 test project + `scripts/test-infra/run-test-suite.bat` 加入
- [ ] T013: `xbuild.bat weasel` → 0 errors 0 warnings
- [ ] T014: `scripts/test-infra/run-test-suite.bat` → 16/16+1 = 17 test projects 全部 PASS
- [ ] T015: 跑 AGENTS §2.5 smoke test 验证（不修 install.nsi，smoke test 应仍 PASS）
- [ ] T016: commit `fix(WeaselDeployer): spec 042 R4+R2 fix - MaintenanceGuard RAII + join_maintenance_thread`
- [ ] T017: 更新 CHANGELOG（不动 env.bat/weasel.props = 不发 installer）

## P2 (should, 推迟到 spec 043)

- [ ] T018: `WeaselServerApp::execute()` 改 CreateProcess + Job Object (R1)
- [ ] T019: WeaselServer 端加 deployer 心跳 + watchdog (R1)
- [ ] T020: `StartMaintenance` 加 refcount (R3)
- [ ] T021: `_IsDeployerRunning()` 加进程探测 (R6)
- [ ] T022: `m_session_status_map.clear()` 通知 TSF (R5)

## 估计

- T001-T003: 20 分钟 (已完成, ~12.5 KB spec 三件套)
- T004: 30 分钟
- T005-T007: 20 分钟 (3 段同模式)
- T008: 5 分钟
- T009: 10 分钟
- T010-T012: 60 分钟
- T013-T015: 5 分钟
- T016-T017: 5 分钟
- **总计**: ~2.5 小时 (在 R5 4h 限制内)

## 依赖

- spec 045 v0.18.29.0 ship (已完成)
- weasel::Client 接口未变 (无 API 变化)
- TestDarkModeBridge 模板 (已存在)
- AGENTS.md §2.5 smoke test (已存在)

## 验收回执

- [ ] T-A1: `xbuild.bat weasel` exit 0
- [ ] T-A2: `scripts/test-infra/run-test-suite.bat` "=== ALL TESTS PASSED ==="
- [ ] T-A3: `TestOrphanRecovery` 5/5 mock assertions PASS
- [ ] T-A4: `git diff` 中 `Configurator.cpp` 三段均用 `MaintenanceGuard` 包裹
- [ ] T-A5: `lessons-learned.md` 含 L55 章节