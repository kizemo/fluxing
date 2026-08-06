# Tasks 076

- [ ] T001 修改 `include/PipeChannel.h`: `boost::thread_specific_ptr` → `thread_local`
- [ ] T002 创建 `test/TestPipeChannelRace/TestPipeChannelRace.cpp`: 100 thread × 1000 calls 并发回归
- [ ] T003 创建 `test/TestPipeChannelRace/TestPipeChannelRace.vcxproj`: 测试项目配置
- [ ] T004 修改 `weasel.sln`: 加 TestPipeChannelRace 项目
- [ ] T005 修改 `env.bat`: PRODUCT_VERSION 0.20.0.1 → 0.20.0.2
- [ ] T006 修改 `CHANGELOG.md`: 加 v0.20.0.2 entry (per P5)
- [ ] T007 新增 `.specify/memory/lessons-learned.md` L##-ThreadSpecificPtr-Race (per CLAUDE.md §3)
- [ ] T008 新增 `docs/adr/NNNN-thread-local-replaces-boost-tss.md` (MADR)
- [ ] T009 `msbuild weasel.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32` 编译
- [ ] T010 跑 `TestPipeChannelRace.exe` + `TestPipeProtocol.exe`
- [ ] T011 装 v0.20.0.2 到 `D:\Program Files\fluxing`
- [ ] T012 触发原 crash 路径 + 检查 `C:\fluxing-dumps` 无新 dump
- [ ] T013 写 `_rollback_v0.20.0.2.ps1` (一键卸载 + 恢复 v0.20.0.1)
- [ ] T014 `git add` + commit (per AGENTS.md §5 + commit-checklist.md)