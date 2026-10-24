# Handoff — v0.20.0.3 WeaselServer 不启动 + crash 根因 (2026-08-09)

> **状态**: v0.20.0.3 已 build 完整, installer md5 `3414b3cde9e0ad9f67e4687a73dd2d37`,
> 内部 weaselx64.dll md5 `a289525bb18511568785e9ab2e7ffec0` (含 PipeChannel 修复).
> **新问题**: 用户装 v0.20.0.3 后, 算法服务 (WeaselServer.exe) 没启动,
> 切火流猩输入法无响应, 右键重启无效, 但**没有 crash** (无新 dump).
> **未完成**: 找 WeaselServer 不启动的根因 + 验证 PipeChannel 修复效果.

---

## 0. 大白话总结

| 项 | 值 |
|---|---|
| **PipeChannel 修复** | `boost::thread_specific_ptr` TOCTOU race → `static thread_local` (C++11 magic statics) |
| **之前 crash 根因** | 微信/Claude/dopus/explorer 在切 Fluxing 后 0xc0000374 HEAP_CORRUPTION 全部 crash |
| **v0.20.0.3 installer** | 40.8 MB, 5 binary md5 与 source build 完全一致, 修复**已装入** installer |
| **新问题** | v0.20.0.3 装后 WeaselServer **不启动** (无 crash, 但服务未运行) |
| **用户测试** | 切火流猩 → 算法服务没启动 → 输入中文无响应 → 右键重启算法服务**无效** |

**旧 bug 不算修好**（v0.20.0.3 装后用户根本没测出 crash 路径，因为服务先不启动）。

---

## 1. 已经做的

### 1.1 include/PipeChannel.h (line 41-57 修复)
```cpp
// 之前 (TOCTOU race):
mutable boost::thread_specific_ptr<ChannelContext> context;
ChannelContext* _GetContext() const {
  if (!context.get()) {              // 2 个 thread 同时看 null
    context.reset(new ChannelContext(bs));  // 同时 alloc
  }                                   // 第二个 reset() 覆盖第一个
  return context.get();
}

// 之后 (C++11 thread_local, 编译器保证 thread-safe lazy init):
ChannelContext* _GetContext() const {
  static thread_local std::unique_ptr<ChannelContext> ctx(
      new ChannelContext(buff_size));
  return ctx.get();
}
```

### 1.2 include/StringAlgorithm.hpp (line 62-69 setlocale 修复)
```cpp
// 之前 (process-wide locale mutation, P2 违):
std::setlocale(LC_ALL, "");
std::transform(wstr.begin(), wstr.end(), wstr.begin(), std::towlower);

// 之后 (thread-safe):
const std::locale loc("");
const auto& facet = std::use_facet<std::ctype<wchar_t>>(loc);
facet.tolower(&*wstr.begin(), &*wstr.end());
```

### 1.3 env.bat (v0.20.0.2 → v0.20.0.3)
```bat
set "FLUXING_VERSION=0.20.0"
set "WEASEL_BUILD=3"
set "PRODUCT_VERSION=0.20.0.3"
set "FILE_VERSION=0.20.0.3"
```

### 1.4 v0.20.0.3 installer 生成
- `F:\soft\00selfmade\rime_claude\release\fluxing-0.20.0.3-installer.exe` (40.8 MB)
- md5: `3414b3cde9e0ad9f67e4687a73dd2d37`
- 内部 5 binary md5 vs source build (Aug 9 凌晨 build):
  - WeaselServer.exe: `fd0732378ffd2339fef43e327e96008d` ✓
  - FluxingPhrasesDialog.exe: `85c7faf86dd2401d340a2abd2184d509` ✓
  - weasel.dll: `83d76e2665ec19090115a3a1a854fcc3` ✓
  - rime.dll: `7c4e9824ac771bc31019319d2a4938bb` (保持 v0.20.0.2 build, 无变化)
  - weaselx64.dll: `a289525bb18511568785e9ab2e7ffec0` ✓ (含 PipeChannel 修复)
- v0.20.0.2 weaselx64.dll md5 `fba4b16c...` (无修复) vs v0.20.0.3 `a289525b...` (有修复)

### 1.5 dirty state 清理
- `git rm` 51 个 tracked deleted 文件:
  - `release/fluxing-0.19.0.10..62-installer.exe` (49 个 0-byte)
  - `output/7z.dll` + `output/WinSparkle.dll` (2 个)
- 1108 个 untracked `_*.ps1` 之前会话遗留 dirty state, **未**清理 (不越界)

### 1.6 test/TestPipeChannelRace/ 修复
- cpp 加 `#include <immintrin.h>` (修 _mm_pause missing include)
- cpp 加 public wrapper methods `GetContextForTest()` / `GetPipeHandleForTest()` (避免 using-decl 不生效)
- vcxproj 加 `<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />` + `<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />` + `<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />` (Fix ClCompile task not found error)
- vcxproj 加 `<Target Name="Build" Outputs="...">` (fix 404 target not found)
- vcxproj 加 `$(BOOST_ROOT)` 到 AdditionalIncludeDirectories (修 boost header 找不到)
- **状态**: build 进行中, 当时 build 还卡在 void* wrapper / qualified name 问题上, **未完成**

---

## 2. 用户新问题 (本次)

### 2.1 用户反馈 (2026-08-09)
> 装 v0.20.0.3 后切火流猩输入法, 算法服务没启动, 输入中文无响应. 重启算法服务无效.
> 旧的bug没解决, 又出现新的bug.

### 2.2 已验证的事实

| 项 | 值 |
|---|---|
| v0.20.0.3 installer md5 | `3414b3cde9e0ad9f67e4687a73dd2d37` |
| 内部 weaselx64.dll md5 (含 fix) | `a289525b...` |
| Fluxing 安装路径 | `D:\Program Files\Fluxing\weasel\` |
| weaselx64.dll installed | v0.20.0.3 build (md5 匹配 source) |
| rime.weasel.INFO log (用户尝试期间) | **无文件** (v0.20.0.3 装后 0 logs 写入) |
| rime.weasel.INFO log (我手动 start 后) | 17:51:00 (刚刚), 加载了所有 components, 无 key_binder parse error |
| WeaselServer.exe 手动启动 | **成功** (PID 5508, Responding=True) |
| Application Event Log (WeaselServer 相关) | **无 error** (我查了 50+ recent events) |
| HKLM/HKCU Run key (WeaselServer auto-start) | **未查到** (system 限制, 不知道完整内容) |

### 2.3 关键观察

**用户报告 "没启动 + 重启无效 + 无 crash" → 真实 WeaselServer 状态需要进一步诊断**:
- **A 假设**: v0.20.0.3 install 没有注册 WeaselServer auto-start (注册表 Run key 缺失)
- **B 假设**: v0.20.0.3 install 注册了, 但 WeaselServer 启动后立刻退出 (静默崩溃)
- **C 假设**: 用户点 "重启" 触发了错误的启动路径 (可能 start_service.bat 在新安装路径下找不到)
- **D 假设**: install.nsi 的 ExecWait 启动逻辑有 bug

**最可能**: A 或 C 假设. **建议诊断方向**:
1. **重新启动 cdb 分析用户机器 dump** (但用户报告无新 dump, 所以无新分析)
2. **手动试 start_service.bat 看输出** (用户机器 `D:\Program Files\Fluxing\weasel\start_service.bat`)
3. **查 install.nsi 的 `Section "-PostInstall"` 或 `ExecWait` 段**, 看 v0.20.0.3 装机后是否调用了 start_service.bat
4. **重装 v0.20.0.3 用更详细 log** (`/D=... /S /LOG=C:\install.log`)

---

## 3. 需要做的

### 3.1 立即 (下一步第一件事)
1. **重装 v0.20.0.3 时用 NSIS /LOG=** 抓装机 log, 看 ExecWait 启动了哪些东西
2. **重装后查注册表**:
   - `HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel` 还在不在 (装机路径)
   - `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 有没有 Fluxing / WeaselServer auto-start
3. **手动跑 `D:\Program Files\Fluxing\weasel\start_service.bat`** 看 stderr / popup
4. **如果 WeaselServer 启动后立刻 crash**, 用 WER LocalDumps + cdb 抓 dump
5. **如果 WeaselServer 启动正常**, 测 Fluxing 是否能输入中文

### 3.2 继续修 root cause
- 如果 PipeChannel 修复**部分有效** (减少 crash 概率), 还需要找第二个 bug
- subagent 已确认 11 个 dump 全一致 (同一 offset, 同一 lambda hash) → 仍可能是 thread_specific_ptr race 或其他 shared state bug
- 看 `ClientImpl::_WriteClientInfo` (line 193-199 of `WeaselIPC/WeaselClientImpl.cpp`) 用 `channel << ... << app_name.c_str()` — `app_name` 是不是被 dangling reference?

### 3.3 继续 task 列表 (本会话遗留)
- Task #28: Add regression test — **未完成** (build 卡在 cpp error)
- Task #25-27: **完成** (verified md5 + installer)

---

## 4. 关键文件位置

| 文件 | 路径 | 状态 |
|---|---|---|
| v0.20.0.3 installer | `F:\soft\00selfmade\rime_claude\release\fluxing-0.20.0.3-installer.exe` | ✅ build 完成, md5 verified |
| PipeChannel 修复 | `F:\soft\00selfmade\rime_claude\include\PipeChannel.h` line 41-57 | ✅ source 已改 |
| setlocale 修复 | `F:\soft\00selfmade\rime_claude\include\StringAlgorithm.hpp` line 62-69 | ✅ source 已改 |
| env.bat v0.20.0.3 | `F:\soft\00selfmade\rime_claude\env.bat` | ✅ 已改 |
| 测 test | `F:\soft\00selfmade\rime_claude\test\TestPipeChannelRace\` | ⚠️ build 失败 (cpp error 待解) |
| 用户机器 | `D:\Program Files\Fluxing\weasel\` | ⚠️ WeaselServer 不启动 |
| rime.weasel log | `C:\Users\Duanyi\AppData\Local\Temp\rime.weasel\` | 仅 1 文件 (我手动 start) |
| dumps | `C:\fluxing-dumps\` | v0.20.0.1 老 dumps, **无** v0.20.0.3 dump (无 crash) |

---

## 5. 已 commit 的 fix (waiting for user verify)

```bash
# 现状 (modified files in working tree):
M include/StringAlgorithm.hpp        # setlocale fix
M include/PipeChannel.h             # thread_specific_ptr fix
M env.bat                             # v0.20.0.2 → v0.20.0.3 (gitignored)
M test/TestPipeChannelRace/TestPipeChannelRace.cpp  # regression test (build broken)
M test/TestPipeChannelRace/TestPipeChannelRace.vcxproj  # vcxproj fixes
M WeaselTSF/WeaselTSF.vcxproj          # 加 WeaselIPC include
M WeaselIPC/WeaselIPC.vcxproj         # 加 WeaselIPC include
?? release/fluxing-0.20.0.3-installer.exe  # 新 installer

# 51 个 deleted tracked (已 git rm):
D release/fluxing-0.19.0.{5,7,8,9,10..62}-installer.exe
D output/7z.dll
D output/WinSparkle.dll
```

---

## 6. 给新会话的 Prompt (一句话)

```
继续 Fluxing v0.20.0.3 调试: 用户装 v0.20.0.3 后 WeaselServer 不启动 (无 crash, 无新 dump, 服务不运行), 旧 PipeChannel 修复未验证. 第一步: 用 NSIS /LOG= 重装 v0.20.0.3 抓装机 log + 查注册表 (HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel 和 HKCU\..\Run) 看 auto-start 是否注册; 第二步: 手动跑 D:\Program Files\Fluxing\weasel\start_service.bat 看 stderr; 第三步: 用 cdb 重新分析用户的实际启动状态 (ProcessExplorer 看 WeaselServer 进程状态), 如果需要, 查 rime.weasel.*.INFO log 看 librime init 失败原因. 详见 F:\soft\00selfmade\rime_claude\handoff-v0.20.0.3-weaselserver-not-starting-2026-08-09.md.
```

---

**作者**: Claude (本会话) · **日期**: 2026-08-09 (夜)
**session 状态**: API Error 中断, 立即结束以保 handoff 完整
