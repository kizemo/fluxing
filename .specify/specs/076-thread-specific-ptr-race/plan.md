# Plan 076 — PipeChannel thread_specific_ptr race

## 技术路径

1. **`include/PipeChannel.h`**: 替换 `boost::thread_specific_ptr` 为 `thread_local`
2. **新增 `test/TestPipeChannelRace/TestPipeChannelRace.cpp`**: 并发回归测试
3. **`test/TestPipeChannelRace/TestPipeChannelRace.vcxproj`**: 配套项目文件
4. **`env.bat`**: bump version 0.20.0.1 → 0.20.0.2
5. **`CHANGELOG.md`**: 加 v0.20.0.2 entry
6. **`.specify/memory/lessons-learned.md`**: 新增 L##-ThreadSpecificPtr-Race
7. **`docs/adr/NNNN-thread-local-replaces-boost-tss.md`**: MADR 记录架构决策

## 文件改动总览

| 路径 | 改动 | 行数 |
|---|---|---|
| `include/PipeChannel.h` | thread_specific_ptr → thread_local | ~30 行 |
| `test/TestPipeChannelRace/TestPipeChannelRace.cpp` | 新增 | ~100 行 |
| `test/TestPipeChannelRace/TestPipeChannelRace.vcxproj` | 新增 | ~50 行 |
| `weasel.sln` | 加 TestPipeChannelRace 项目 | +5 行 |
| `env.bat` | PRODUCT_VERSION 0.20.0.1 → 0.20.0.2 | +1 行 |
| `CHANGELOG.md` | 新 v0.20.0.2 entry | +10 行 |
| `.specify/memory/lessons-learned.md` | L##-ThreadSpecificPtr-Race | +60 行 |
| `docs/adr/NNNN-thread-local-replaces-boost-tss.md` | ADR | ~40 行 |

## 不改

- `install.nsi` (无变化)
- 3 个 user-custom YAML (回滚到 v0.20.0.1 状态)
- librime (零改动)
- 其他无关模块

## 验证流程

1. **Build**: `msbuild weasel.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32`
2. **Unit**: `test/TestPipeChannelRace/Release/TestPipeChannelRace.exe`
3. **集成**: `test/TestPipeProtocol/Release/TestPipeProtocol.exe` (回归)
4. **Self-install**: 装 v0.20.0.2 到 `D:\Program Files\fluxing`
5. **Crash repro**: 切到 Fluxing + 输入 + 抓 dumps
6. **cdb 验证**: 跑 cdb 在新 dump 上确认 lambda + offset 不变 / 0 新 dump

## 时间预算

- spec + plan + tasks: 5 min (已完成)
- code change: 5 min
- test: 10 min
- build: 5 min (incremental)
- install: 2 min
- crash test: 5 min
- lessons + CHANGELOG: 5 min
- commit: 2 min
- **Total: 40 min** (符合 2h 预算)