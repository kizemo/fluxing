#pragma once
// ADR 0031: console 会话是全局热键的唯一持有人。
//
// 为什么需要它: 2026-11-03 事故里 WeaselServer.exe 被 ToDesk 远程会话(Session
// 3) 的 explorer 经 Startup\WeaselServer.bat 拉起。RegisterHotKey
// 是**会话级**作用域, 单实例 mutex 又不带 Global\ 前缀(决策 2:
// 加了反而让远程实例占死全局锁, 否决),
// 于是远程会话注册了它那个会话的热键、console 用户按键无人接管 —— 全局热键
// 静默失效, 且 Start() 的失败路径只 return false, 无日志。
//
// 本头给出**唯一的**判定实现, 服务端 Start() 与 test/TestWeaselIPC 的
// /verify-console-session 探针共用同一份代码 (constitution R7)。判定核心
// Evaluate() 是纯函数(入参两个 SessionId),
// 因此不需要真实多会话环境也能构造失败场景。
//
// 口径: 诊断日志复用 ADR 0029 决策 7 的 include/FluxingDiagLog.h
// (%TEMP%\fluxing-profile-notify.log), 本头只产字段串, 不自己开文件。
#ifndef FLUXING_SESSION_POLICY_H
#define FLUXING_SESSION_POLICY_H

#include <windows.h>
#include <wtsapi32.h>

#include <string>

// WTSGetActiveConsoleSessionId 在 wtsapi32.lib。本项目工程文件里没有
// AdditionalDependencies 块, 逐个 vcxproj 补 8 个配置块代价大且易漏;
// #pragma comment(lib, ...) 是本仓库既有惯例
// (WeaselServer.cpp:8 gdiplus.lib、QuickPanelDialog.cpp:35 windowscodecs.lib),
// 指令随 .obj 传给链接器, 因此服务端 / TSF / 测试工程都能零改工程配置取到。
#pragma comment(lib, "wtsapi32.lib")

namespace fluxing {
namespace session {

// INVALID_SESSION_ID: 没有 console 会话(无主控台的服务器 / 纯 RDP 机器)。
inline constexpr DWORD kInvalidSessionId = 0xFFFFFFFFu;

struct Verdict {
  DWORD current_session;  // 0 = 取不到
  DWORD console_session;  // 仅当 console_known 为真时有意义
  bool console_known;
  bool allowed;
  const char* reason;  // 稳定 ascii 键名, 供日志 grep
};

// 当前进程的 SessionId。ProcessIdToSessionId 在 kernel32, 不需要额外链接库。
inline DWORD CurrentSessionId() {
  DWORD id = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &id))
    return 0;
  return id;
}

// 物理控制台(用户真正坐在屏幕前)的 SessionId。known 出参标记是否可信。
inline DWORD ConsoleSessionId(bool* known = nullptr) {
  const DWORD id = ::WTSGetActiveConsoleSessionId();
  if (known != nullptr)
    *known = (id != kInvalidSessionId);
  return id;
}

// 纯判定核心 —— 不碰任何系统 API, 因此可以用伪造入参构造必失败场景。
//
// fail-open 取舍(ADR 0031 未明写, 此处定案): 取不到 console SessionId
// 时**放行**。 判不出会话就拒启动, 会让本来正常的 console
// 用户在异常环境下彻底失去服务 UI(面板/短语窗/托盘) —— 那比"可能起错会话"更糟,
// 且留痕照样能取证。
inline Verdict Evaluate(DWORD current_session,
                        DWORD console_session,
                        bool console_known) {
  Verdict v;
  v.current_session = current_session;
  v.console_known = console_known;
  v.console_session = console_known ? console_session : 0;
  if (!console_known) {
    v.allowed = true;
    v.reason = "console_session_unknown_fail_open";
    return v;
  }
  if (current_session == console_session) {
    v.allowed = true;
    v.reason = "console_session_match";
  } else {
    v.allowed = false;
    v.reason = "not_console_session";
  }
  return v;
}

// 本进程的真实判定, 生产代码用它。
inline Verdict EvaluateCurrent() {
  bool known = false;
  const DWORD console = ConsoleSessionId(&known);
  return Evaluate(CurrentSessionId(), console, known);
}

// "current_session_id=3 console_session_id=1" —— 取证要的就是这两个数。
inline std::string SessionFields(const Verdict& v) {
  std::string s = "current_session_id=";
  s += std::to_string((unsigned long long)v.current_session);
  s += " console_session_id=";
  s += v.console_known ? std::to_string((unsigned long long)v.console_session)
                       : std::string("unknown");
  return s;
}

// 单行结构化描述: "<event> allowed=0 reason=not_console_session current_...
// console_..."
inline std::string Describe(const char* event, const Verdict& v) {
  std::string s(event);
  s += " allowed=";
  s += (v.allowed ? "1" : "0");
  s += " reason=";
  s += v.reason;
  s += " ";
  s += SessionFields(v);
  return s;
}

}  // namespace session
}  // namespace fluxing

#endif  // FLUXING_SESSION_POLICY_H
