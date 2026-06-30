# 010 · Tasks · 跨设备云同步

> v2 P2 仅设计。v2.1+ 实施时此表生效。

## Phase 1 · 设计（v2 阶段）

- [x] T001 [P2] [US7-A/B] design.md 详细 schema（sync-package.json + 冲突策略）
- [x] T002 [P2] [US7-C] 网络失败降级策略（继续用本地缓存）

## Phase 2 · 后端（v2.1 启动时）

- [ ] T003 [P2] Vercel Serverless Functions 部署（auth / sync / register）
- [ ] T004 [P2] 数据库 schema（用户 / 设备 / sync-package history）

## Phase 3 · 客户端（v2.1 实施）

- [ ] T005 [P2] [US7-A] 新建 `RimeWithWeasel/Sync/{SyncClient,SyncScheduler,ConflictResolver}.{h,cpp}`
- [ ] T006 [P2] [US7-A] 启动时拉取 + 输入停止 5s 后增量 push
- [ ] T007 [P2] [US7-B] 跨设备验证（A 加短语 → B 出现）

## Phase 4 · 验证

- [ ] T008 [P2] 新建 `test/TestSyncClient.cpp` + `TestConflictResolver.cpp`（mock Vercel API）
- [ ] T009 [P2] `xbuild.bat weasel installer` → 0 errors
- [ ] T010 [P2] 手动验证 2 设备同步

## Phase 5 · 提交 & Release

- [ ] T011 [P2] commit：`feat(fluxing): spec 010 cloud sync`
- [ ] T012 [P2] release `fluxing-2.1.0-installer.exe` 推 `kizemo`