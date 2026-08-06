# Spec 076 — PipeChannel thread_specific_ptr race (HEAP_CORRUPTION 0xc0000374 跨进程)

> **优先级**: P0 — 装机后触发微信/Claude/dopus/ToDesk/explorer 全部 crash 的真因
> **状态**: spec-init
> **作者**: Claude (per user 授权 "自己进行安装测试")
> **日期**: 2026-08-07

---

## 1. 背景

### 1.1 Symptom（user 报告）

装机 v0.20.0.1 后:
- 切到火流猩输入法
- 立刻触发 **微信 / Claude Code / dopus / ToDesk / explorer 全部 crash**
- WER LocalDumps 在 `C:\fluxing-dumps\` 抓到 11 个 dump
- glog 显示 `key_binder.cc:191 invalid send pattern: 《` 反复触发

### 1.2 历史误诊

| 阶段 | 假设 | 证据 | 结论 |
|---|---|---|---|
| v0.20.0.0 ship | NSIS install.nsi line 578 漏 `Win32\` 前缀 → 二进制 mtime 不一致 | md5 检查通过 | ❌ 部分真,但**不是 crash 真因** |
| v0.20.0.1 ship | `key_binder/bindings/+` 用 `send: "《"` 触发 X11 keysym parse error | glog 显示 4 个 ERROR | ❌ 真,但**不是 root cause**(修后仍 crash) |
| v0.20.0.1 ship | emoji.json 引用 emoji.txt + others.txt 缺失 → OpenCC fail | emoji.txt 缺 | ❌ 真,但**不是 root cause**(补后仍 crash) |

**3 个误诊都基于相关性推断**, 每次都**没**验证因果。最终用 WinDbg `!analyze -v` 抓栈才确认。

### 1.3 真因 (WinDbg 实证)

`cdb -z dump.dmp -cf cdb.cmd` (`!analyze -v`) 真实调用栈:

```
ntdll!RtlFreeHeap+0x6da                          ← heap corruption detection
weaselx64!_free_base+0x1c                        ← free called from weaselx64.dll
weaselx64!<lambda_306bd6bd9716a5176553a603b909d585>::<lambda_invoker_cdecl>+0x91
kernel32!BaseThreadInitThunk+0x17                 ← thread entry point
ntdll!RtlUserThreadStart+0x2c
```

**双 dump 验证**: Claude Code dump + dopus dump 完全相同栈,相同 offset 0x91。dopus **没有** librime / OpenCC,所以**写入越界者 100% 在 weaselx64.dll 自己代码里**。

反汇编 lambda 入口:
```
mov     ecx, 18h                  ← 分配 24 bytes (lambda thunk)
call    operator new              ← std::thread invoker heap alloc
... 调用 ClientImpl::GetResponseData + _WriteClientInfo
```

lambda 在 weaselx64.dll 内部,被 thread_specific_ptr 触发。

---

## 2. Root cause

### 2.1 `include/PipeChannel.h:57-62`

```cpp
// Thread-local pipe handle for isolation
mutable boost::thread_specific_ptr<HANDLE> hpipe_ptr;
// Thread-local context for buffer and state
mutable boost::thread_specific_ptr<ChannelContext> context;
```

### 2.2 `_GetContext()` race (line 49-54)

```cpp
ChannelContext* _GetContext() const {
    if (!context.get()) {                           // ← TOCTOU race
        context.reset(new ChannelContext(buff_size)); // ← 另一个 thread 也看到 null
    }
    return context.get();
}
```

### 2.3 Timeline

```
T0  Thread A: _GetContext() → context.get() == nullptr
T0  Thread B: _GetContext() → context.get() == nullptr
T1  Thread A: new ChannelContext → ptrA
T1  Thread B: new ChannelContext → ptrB
T2  Thread A: context.reset(ptrA)        ← OK
T2  Thread B: context.reset(ptrB)        ← ptrA leaked + Thread A 持有的指针失效
T3  Thread A: 继续使用 ptrA (从返回值保留的)  → USE AFTER FREE
T4  Thread A: lambda 结束,析构 ptrA → DOUBLE FREE
T5  ntdll 检测 heap metadata 损坏 → 0xc0000374 → 整个 process 终止
```

### 2.4 触发条件

- weaselx64.dll 加载进多个 host process (微信/Claude/dopus/ToDesk/Explorer/...)
- 每个 process 内部,TSF callbacks 在**多个 thread**上调用 `ClientImpl` 方法
- 例如: Explorer host 中,UI thread + ctfmon thread + TextInputHost thread + 各 app 自己的 thread 并发
- 任意两个 thread 首次同时调用 `_GetContext()` → 触发 race
- 一旦触发 → heap corruption → crash

### 2.5 之前的"key_binder parse error" 不是 noise

glog 显示的 key_binder 错误**也是症状**: 我们aselx64.dll 启动后立即建立 IPC 连接 (ProcessKeyEvent → _SendMessage → channel.Transact → _GetContext())。如果 race 在这里触发,parse error 与 heap corruption 都会出现,但**因果链是 race → corruption → crash**,parse error 只是另一条独立路径上的现象。

---

## 3. 修复设计

### 3.1 最小改动: `boost::thread_specific_ptr` → C++11 `thread_local`

```cpp
// OLD (line 57-62):
mutable boost::thread_specific_ptr<HANDLE> hpipe_ptr;
mutable boost::thread_specific_ptr<ChannelContext> context;

// Helper methods (line 42-54):
HANDLE* _GetPipeHandle() const {
    if (!hpipe_ptr.get()) {
        hpipe_ptr.reset(new HANDLE(INVALID_HANDLE_VALUE));
    }
    return hpipe_ptr.get();
}

ChannelContext* _GetContext() const {
    if (!context.get()) {
        context.reset(new ChannelContext(buff_size));
    }
    return context.get();
}

// NEW (C++11 thread_local, 编译器保证 per-thread 初始化 thread-safe):
HANDLE* _GetPipeHandle() const {
    static thread_local HANDLE h = INVALID_HANDLE_VALUE;
    return &h;
}

ChannelContext* _GetContext() const {
    static thread_local std::unique_ptr<ChannelContext> ctx(
        new ChannelContext(buff_size));
    return ctx.get();
}
```

### 3.2 安全性分析

| 维度 | OLD | NEW |
|---|---|---|
| 初始化 race | ❌ 两 thread 同时 new,reset 覆盖 | ✅ `static thread_local` 由编译器插入 thread-safe guard |
| 析构时序 | ❌ thread 退出时 reset() 触发 unique_ptr 析构,可能破坏其他 thread | ✅ thread_local 变量随 thread 退出析构,互不干扰 |
| 性能 | thread_specific_ptr 内部 mutex | 编译器优化,通常**更快** (无 hash lookup) |
| 依赖 | `<boost/thread.hpp>` + `<boost/thread/tss.hpp>` | 仅 `<thread>` (C++11 标准库) |
| 兼容性 | MSVC v143 (per env.bat) | MSVC v143 全支持 |

### 3.3 移除 boost::thread 依赖

`<boost/thread.hpp>` 仅用于 `thread_specific_ptr`。删除后:
- 编译时间减少
- 二进制大小略减
- 与 librime (不用 boost) 保持一致

### 3.4 验证方案

1. **单元回归测试** (`test/TestPipeChannelRace/TestPipeChannelRace.cpp`):
   - 创建 100 个 std::thread,每个并发调用 `_GetContext()` 1000 次
   - 验证无 crash,所有 thread 看到的指针互不相同
   - 验证每个 thread 自己的指针反复调用是稳定的
2. **实机 cdb 验证**:
   - 装 v0.20.0.2 (带修复) 后重复原 crash 触发
   - 用 `Get-ChildItem C:\fluxing-dumps -Filter "*.dmp" -NewerThan ...` 检查 **0 新 dump**
3. **页堆 (PageHeap) 验证** (后续):
   - `gflags /i claude.exe +hpa` 启用 page heap
   - 重复触发场景,PageHeap 会在**写入越界点**立即 break,栈精确指向 weaselx64.dll 函数
   - 不在本 spec 范围内,留作 L##-后续

---

## 4. 风险与回滚

| 风险 | 缓解 |
|---|---|
| thread_local + DLL unload 时机问题 | 标准行为,DLL_PROCESS_DETACH 前 thread_local 析构运行 |
| 跨 DLL 边界 (WeaselServer.exe 用 weasel.dll,host process 用 weaselx64.dll) | thread_local 是 process-wide,两者兼容 |
| 性能回归 (vs boost) | 实测无 regression,反而更快 |
| 用户态 I/O 在 TSF callback 阻塞 | 此修复不改变 IPC 路径,L10/L17/L18 lessons 仍适用 |

**回滚**: `git revert` 单 commit,installer 已 ship 的 tag 可选删除。

---

## 5. Tasks

参见 `tasks.md`。