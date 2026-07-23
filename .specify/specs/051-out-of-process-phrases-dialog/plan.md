# 051 - Out-of-Process PhrasesDialog — 技术方案

## Constitution Check

| Principle | Status | 说明 |
|---|---|---|
| I (安全/隔离) | ✓ | Named pipe IPC 天然隔离；pipe ACL 限制同用户进程 |
| II (性能) | ✓ | Pipe < 50ms 延迟对 UI 无感知；短语列表 < 100 条时 payload < 10KB |
| III (可维护) | ✓ | 代码几乎原样搬出，复用现有 ModalChrome / 几何公式 / DPI |
| IV (兼容) | ✓ | 现有 186 测试适配后保持 PASS；install.nsi 增量修改 |
| V (简洁) | ✓ | 最小新增代码路径；不引入新库/新框架 |
| R1 (用户中文) | ✓ | UI 字符串保持简体中文 |
| R2 (无技术词入 spec) | ✓ | spec.md 不含 C++/WTL/Boost/TSF 等技术词 |
| R3 (任务分量) | ✓ | 每任务 ≤ 4h, 1-3 文件 |
| R4 (Commit 规范) | ✓ | 用 WeaselServer / installer / fluxing 模块前缀 |
| R5 (IPC 四边同步) | ✓ | 新增 IPC 协议时同步 server/TSF/deployer/ADR/test |
| P1 (不破坏现有) | ✓ | 现有功能保持 |
| P2 (不阻塞 TSF) | ✓ | TSF 回调不新增 I/O（pipe 通信在非 TSF 线程） |
| P4 (Conventional Commits) | ✓ | Commit message 格式合规 |
| P5 (中文优先) | ✓ | 用户可见字符串中文 |

## Tech approach

### 架构概览

```
WeaselServer.exe (TSF shim 进程)         FluxingPhrasesDialog.exe (普通 Win32 GUI)
┌──────────────────────────────────┐     ┌─────────────────────────────────────┐
│ WeaselServerApp                  │     │ WinMain + PhrasesDialog (standalone)│
│   └─> PhrasesDialogIPC::Show()  │     │   ├─ PipeClient (读 m_phrases)      │
│        ├─ CreateProcess          │────>│   ├─ PhrasesDialog::OnPaint (不变)  │
│        ├─ PipeServer             │<───>│   ├─ PhrasesDialog::OnLButtonDown   │
│        │  ├─ SendPhrases()      │     │   └─ InjectText → PipeClient::Send  │
│        │  ├─ OnAdd() ← pipe     │     │                                      │
│        │  └─ OnInject() ← pipe  │     │  IME: 系统 IMM32 (not TSF) ✓         │
│        └─ InjectText → SendInput │     └─────────────────────────────────────┘
│                                  │
│  IME: TSF (weaselx64.dll)        │
└──────────────────────────────────┘
```

### 关键技术决策

#### 1. IPC 选型：Named pipe（非 shared memory）
- **理由**: PhrasesDialog 数据量小（< 100 条短语，总 payload < 10KB），named pipe 足够
- **优点**: Windows 原生支持、ACL 简单、连接管理方便、可跨 session
- **对比 shared memory**: 需额外同步机制（event/mutex），复杂度高，收益不大
- **如性能不够**: 后续改为 shared memory + named pipe 指令通道（混合模式）

#### 2. 文本注入路径：WeaselServer.exe 端 SendInput
- **理由**: WeaselServer.exe 已有 TSF/imm32 上下文，SendInput 最可靠
- **备选**: 如 fail（pipe 延迟），尝试 FluxingPhrasesDialog.exe 端直接 SendInput

#### 3. IPC 协议设计

**Pipe 1: Data pipe** (`\\.\pipe\FluxingPhrasesDialog\data`)
```
方向: Server → Client (WeaselServer → FluxingPhrasesDialog)
内容: UTF-8 JSON phrases 数组
      {"version":1,"phrases":[{"id":0,"text":"你好","weight":0},...]}
触发: 启动时 + 每次 add/edit/delete 后推送全量
```

**Pipe 2: Command pipe** (`\\.\pipe\FluxingPhrasesDialog\cmd`)
```
方向: Bidirectional
指令集:
  Client → Server:
    ADD    {"text":"新短语"}
    EDIT   {"id":3,"text":"修改后"}
    DELETE {"id":3}
    INJECT {"id":3}
    RELOAD
    SHUTDOWN
  Server → Client:
    ACK    {"status":"ok"}
    ERR    {"msg":"..."}
    PHRASES {"version":1,"phrases":[...]}  (全量推送)
    SHUTDOWN
```

#### 4. 生命周期管理
- WeaselServer.exe 调 `Show()` → `CreateProcess("FluxingPhrasesDialog.exe", pipe_handle_as_arg)`
- FluxingPhrasesDialog.exe 启动 → 连 pipe → 收 PHRASES → 显示窗口
- 用户 Esc/✕ → Hide 窗口（进程保持，pipe 保持）
- WeaselServer.exe 退出 → 发 SHUTDOWN → 等 3s → TerminateProcess（兜底）
- FluxingPhrasesDialog.exe 崩溃 → WeaselServer.exe PipeServer 检测断连 → 重建 pipe + CreateProcess

#### 5. 代码组织
- `FluxingPhrasesDialog/FluxingPhrasesDialog.cpp` — WinMain + WndProc + OnCreate + OnPaint（从现有 PhrasesDialog.cpp 搬出，重命名）
- `FluxingPhrasesDialog/PipeClient.cpp/.h` — Named pipe 客户端封装
- `FluxingPhrasesDialog/ModalChrome.cpp/.h` — 复制（或改为 static lib 共享）
- `WeaselServer/PhrasesDialogIPC.cpp/.h` — 服务端（替换现有 Show/Hide/AddTop/InjectText）
- `WeaselServer/PhrasesDialog.cpp` — 删（或保留作为接口 compat shim，内部全委托到 IPC）
- `include/FluxingPipeProtocol.h` — IPC 数据结构（JSON schema / 二进制 struct）

## File-level changes

### 新建
| 文件 | 用途 |
|---|---|
| `FluxingPhrasesDialog/` (新 VS 项目) | 独立 exe 项目 |
| `FluxingPhrasesDialog/main.cpp` | WinMain 入口 |
| `FluxingPhrasesDialog/PhrasesDialog.cpp` | 核心 UI（从 WeaselServer/ 搬出） |
| `FluxingPhrasesDialog/PhrasesDialog.h` | UI 头文件 |
| `FluxingPhrasesDialog/PipeClient.cpp` | Named pipe 客户端 |
| `FluxingPhrasesDialog/PipeClient.h` | 客户端头文件 |
| `FluxingPhrasesDialog/ModalChrome.cpp` | 复制（或共享 lib） |
| `FluxingPhrasesDialog/ModalChrome.h` | 复制（或共享 lib） |
| `WeaselServer/PhrasesDialogIPC.cpp` | Pipe 服务端 + CreateProcess 管理 |
| `WeasServer/PhrasesDialogIPC.h` | 服务端头文件 |
| `include/FluxingPipeProtocol.h` | IPC 数据结构定义 |
| `include/FluxingPipeProtocol.cpp` | IPC 序列化/反序列化 |
| `docs/adr/0051-out-of-process-phrases-dialog.md` | 架构决策记录 |

### 修改
| 文件 | 改动 |
|---|---|
| `WeaselServer/WeaselServerApp.cpp` | `PhrasesDialog::Show()` → `PhrasesDialogIPC::Show()` |
| `WeaselServer/PhrasesDialog.cpp` | 保留作为 compat shim（内部转调 IPC），或直接删除 |
| `output/install.nsi` | 新增 FluxingPhrasesDialog.exe 安装 + pipe ACL |
| `output/uninstall.nsi` | 新增加载删除 |
| `_check_install_v2.ps1` | 新增 FluxingPhrasesDialog.exe md5 校验 |
| `Fluxing.sln` | 新增 FluxingPhrasesDialog 项目 |
| `test/TestPhrasesDialog/` | 适配 IPC mock（注入 mock pipe client） |

### 删除 (最终)
| 文件 | 原因 |
|---|---|
| `WeaselServer/PhrasesDialog.cpp` | 代码迁移到 FluxingPhrasesDialog/ |
| `WeaselServer/PhrasesDialog.h` | 同上 |

## Build / Test / Release impact

### Build
- 新增 `FluxingPhrasesDialog.vcxproj`（Win32 GUI Application, 依赖 WTL + gdi32 + user32）
- Fluxing.sln 新增项目
- `build_v046.ps1` / `build_v048.ps1` 新增 FluxingPhrasesDialog.exe 构建步骤
- Release 输出：`FluxingPhrasesDialog.exe` + `.pdb`

### Test
- `TestPhrasesDialog` 186 测试需适配：测试进程改为 CreateProcess FluxingPhrasesDialog.exe + mock pipe server
- 等价性验证：每个原有测试的行为在新架构下等价
- 新增 `test/PipeProtocol/` 测试 IPC 序列化/反序列化
- 新增 `test/FluxingPhrasesDialog/` 集成测试（pipe 通信 + UI）

### Release
- 版本号 v0.19.0.50+（按 ship 递增）
- installer 打包 FluxingPhrasesDialog.exe
- _check_install_v2.ps1 更新 md5 列表
- CHANGELOG.md 记录架构变更

## Risk mitigation

| 风险 | 缓解措施 |
|---|---|
| R-001 Pipe 延迟 | 全量推送（批量），非逐条；实测 TransportSend 耗时 |
| R-002 SendInput 差异 | 保留两个路径（Server 端优先），可切换 |
| R-003 Pipe 断连丢数据 | Add/Edit/Delete 等 ACK 再清 buffer；超时本地缓存 |
| R-004 Z-order 冲突 | 测试两种 TopMost 策略（子窗口 vs 独立 TopMost） |
| R-005 DPI 不同步 | FluxingPhrasesDialog.exe 独立 GetDpiForWindow |
| R-006 测试大规模 rewrite | 先写 mock pipe，保持测试结构，验证等价性 |
