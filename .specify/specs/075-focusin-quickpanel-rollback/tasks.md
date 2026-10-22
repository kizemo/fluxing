# 075 - Tasks

## P1 (must) - all marked [ ] at start

- [x] T001: 用 `codebase-memory-mcp` 路径审计 FocusIn + OnUpdateInputPosition
      (本会话 Phase 1 已完成, evidence 写入 `handoff-input-crash-root-cause-2026-07-31.md`)
- [x] T002: 跑 `_diag_v0741_dumps.ps1` + `_diag_v0741_evidence.ps1` +
      `_diag_v0741_logs.ps1` 取 host 真机证据 (本会话 Phase A 已完成, 6 dump 找到,
      12+ Application Error 0xc0000374 实证)
- [x] T003: 写 spec 075 spec.md (R2 compliance — no tech-stack words outside
      Tech approach section)
- [x] T004: 写 spec 075 plan.md (Constitution Check 5 principles + file-level changes)
- [x] T005: 写 spec 075 tasks.md (this file)
- [x] T006: Edit `RimeWithWeasel.cpp:434-438` 加 `if (false && ipc_id > 0)` gate
      (删除 v0.19.0.65 re-enable 的双份 Show)
- [x] T007: Edit `RimeWithWeasel.cpp:487-492` 删除 duplicate Show (注释保留)
- [x] T008: Edit `WeaselServerImpl.h:103` 加 `m_pfnPhysicalToLogical` field
- [x] T009: Edit `WeaselServerImpl.cpp:33-38` 构造期 cache GetProcAddress
- [x] T010: Edit `WeaselServerImpl.cpp:355-365` 用 cached pointer + m_hWndMsg
- [x] T011: Edit `output/install.nsi:.onInit` 加 admin token check (silent 6002)
- [x] T012: Edit `env.bat:24-25` `WEASEL_PATCH=3`, `PRODUCT_VERSION=0.19.0.74.3`

## P2 (should)

- [ ] T101: (待 build 后) `xbuild.bat` rebuild + md5 verify
- [ ] T102: (待 user acceptance) 装机测试 + Application Error event count 0
- [ ] T103: 写 spec 075 的 commit (post-build success, user 决策 ship time)
- [ ] T104: 写 L##-PhaseL-9.11 entry 进 `.specify/memory/lessons-learned.md`
      (post-build success + user 真机验证后)

## P3 (could)

- [ ] T201: (v0.75 defer) 真机 spec 076 — WeaselPanel GDI+ → pure GDI/D2D
      rewrite + spec 074 (QuickPanel) 真机 stress test 后才能 re-enable
      QuickPanel Show in FocusIn
- [ ] T202: (v0.75 defer) emoji.ocd ship 化 (L##-PhaseL-9.8 真因验证)
- [ ] T203: (v0.76 defer) QuickPanel out-of-process (spec 070 演化)

## Verification

- [ ] V001: `xbuild.bat weasel installer` 成功 exit 0 (per L107 双步 + L108 `_buildflow.cmd`)
- [ ] V002: 5 binary md5 验证:
      - weasel.dll == v0.74.1 (md5 `231fd0cfed97fbe6e40c1498cb10a1f1`)
      - weaselx64.dll == v0.74.1 (md5 `ea4dece5b4c76a01704204472c2aa5f4`)
      - WeaselServer.exe != v0.74.1 (FocusIn + hot-path 改了 → 必 new md5)
      - WeaselDeployer.exe == v0.74.1 (md5 `f8d635342128364056ca748116dceecd`)
      - WeaselSetup.exe == v0.74.1 (md5 `bb4440d6c38a885ee33213d393bf1ab7`)
- [ ] V003: installer md5 != v0.74.1 (`6ee983a365648d923162c0c3121aee68` 因为 install.nsi 改了)
- [ ] V004: `_check_install_v3.ps1` happy path PASS (既有 L##-PhaseL-9.10 ship-gate 不变)
- [ ] V005: `_check_install_v3.ps1` non-admin silent → exit 6002 (新 admin check
      功能验证)
- [ ] V006: (post-ship, real-machine) `_diag_v0741_evidence.ps1` happy path:
      `fluxing-install.log` 不含 "current user not admin" warning
- [ ] V007: (post-ship, real-machine) `_diag_v0741_dumps.ps1` 监控 24h CrashDumps:
      0xc0000374 in ntdll.dll event count = 0 (was ≥ 12 in v0.74.1/2)
