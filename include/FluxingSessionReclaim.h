#pragma once
//
// ADR 0067 - orphan IPC session reclaim (criteria + table selection).
//
// Background:
//   The TSF front end (weasel.dll) lives INSIDE the host application
//   process. When the host exits normally it sends remove_session and
//   the server drops the session. When the host CRASHES that message
//   never arrives, so the session entry (and the rime session behind
//   it) stays in the server tables forever.
//
// Decision:
//   The client reports its host process id inside the existing text
//   request body (session.client_pid=). The server probes that process
//   and drops the session only when the process is provably gone.
//
// Single source of truth (constitution R7):
//   The production side (RimeWithWeasel.cpp) and the unit test
//   (TestFluxingCloud.cpp) include THIS header. Never copy the criteria
//   into another file.
//
// ASCII only:
//   This header is included by RimeWithWeasel.cpp, whose translation
//   unit is compiled WITHOUT /utf-8. Keep every byte of this file
//   ASCII, comments included.
//
// Safety:
//   The only allowed failure direction is a LEAK (an orphan session
//   that is not reclaimed). Dropping a live session breaks typing,
//   which is the worst bug class in this repository.
//   PID reuse can only produce a false "alive" verdict, never a false
//   "dead" verdict, because a reused pid belongs to a running process.
//

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace fluxing {
namespace session_reclaim {

// The diagnostic line the server writes when it reclaims an orphan
// session (ADR 0067, release restart checklist item B-2).
//
// The checklist greps the server log for the EXACT shape
//   reclaim orphan session ipc_id=<n> client_pid=<pid> live=<m>
// so the format lives HERE (single source of truth, R7) and the unit
// test asserts the very same string.
//
//   ipc_id     : the reclaimed session id
//   client_pid : the host pid the client reported for that session
//                (0 when the client never reported one)
//   live       : number of live sessions AFTER the reclaim
inline std::string FormatReclaimLogLine(unsigned long ipc_id,
                                         unsigned long client_pid,
                                         unsigned long live_after) {
  std::string line = "reclaim orphan session ipc_id=";
  line += std::to_string(ipc_id);
  line += " client_pid=";
  line += std::to_string(client_pid);
  line += " live=";
  line += std::to_string(live_after);
  return line;
}

// ADR 0067: the client reports the pid of its host process inside the
// existing text request body, as one extra line:
//
//   session.client_pid=<decimal pid>
//
// An OLD client does not write that line at all. Such a client MUST be
// treated as "pid unknown" (never as "pid 0", never as reclaimable), so
// the parser below reports "not found" for a body without that line.
//
//   body    : the handshake text, lines separated by '\n'
//   out_pid : receives the parsed pid (only when the return value is true)
//
// Returns true when the body carried at least one WELL FORMED pid line,
// and *out_pid holds the LAST well formed value. Returns false when the
// line is absent or when every pid line was malformed; *out_pid is then
// left untouched, so the caller keeps "pid unknown".
//
// Strictness (never guess a number):
//   - the value must be a non empty run of ASCII decimal digits;
//   - the value must fit in a DWORD (<= 0xFFFFFFFF);
//   - anything else is malformed => unknown, never a guessed number.
inline bool ParseClientPid(const std::wstring& body,
                            unsigned long* out_pid) {
  static const wchar_t kKey[] = L"session.client_pid=";
  const size_t kKeyLen = sizeof(kKey) / sizeof(kKey[0]) - 1;
  bool found = false;
  unsigned long last_valid = 0;
  size_t pos = 0;
  while (pos <= body.size()) {
    size_t eol = body.find(L'\n', pos);
    const bool last_line = (eol == std::wstring::npos);
    if (last_line)
      eol = body.size();
    const std::wstring line = body.substr(pos, eol - pos);
    pos = eol + 1;
    if (line.size() >= kKeyLen &&
        std::equal(kKey, kKey + kKeyLen, line.begin())) {
      const std::wstring value = line.substr(kKeyLen);
      unsigned long parsed = 0;
      bool valid = !value.empty();
      for (wchar_t ch : value) {
        if (ch < L'0' || ch > L'9') {
          valid = false;
          break;
        }
        // Accumulate in 64 bits so the DWORD range check below is live:
        // 'unsigned long' is 32 bit on Windows, so accumulating there
        // would wrap around instead of rejecting an out of range value.
        const unsigned long long next =
            static_cast<unsigned long long>(parsed) * 10ull +
            static_cast<unsigned long long>(ch - L'0');
        if (next > 0xFFFFFFFFull) {  // out of DWORD range => malformed
          valid = false;
          break;
        }
        parsed = static_cast<unsigned long>(next);
      }
      if (valid) {
        last_valid = parsed;
        found = true;
      }
    }
    if (last_line)
      break;
  }
  if (found && out_pid)
    *out_pid = last_valid;
  return found;
}

// A reported owner pid may only be probed when it can be a real host
// process:
//   pid == 0        -> the client did not report a pid (unknown)
//   pid <= 4        -> 0..4 are never a host application (unknown)
//   pid == self_pid -> our own process (never reclaimable)
inline bool OwnerPidKnown(unsigned long pid, unsigned long self_pid) {
  return pid > 4 && pid != self_pid;
}

// The reclaim criteria itself.
//   owner_pid_known     : the client reported a usable pid
//   owner_process_alive : the liveness probe says the process runs
// Only "pid known AND process gone" may reclaim. An unknown pid is
// never reclaimed.
inline bool ShouldReclaimSession(bool owner_pid_known,
                                 bool owner_process_alive) {
  return owner_pid_known && !owner_process_alive;
}

// One row of the per session owner table, reduced to what the decision
// needs.
struct SessionOwner {
  unsigned long pid;   // 0 = unknown
  bool process_alive;  // result of the liveness probe
};

// Per row decision with the pid exemptions applied.
inline bool ShouldReclaimOwner(const SessionOwner& owner,
                               unsigned long self_pid) {
  if (!OwnerPidKnown(owner.pid, self_pid)) {
    return false;
  }
  return ShouldReclaimSession(true, owner.process_alive);
}

// Select the session ids that must be reclaimed out of a table of
// (id, owner) rows. The production lazy sweep and the unit test share
// this selection, so there is exactly one implementation of it.
//
//   table    : iterable of pairs (session id, SessionOwner)
//   self_pid : the pid of the server process itself
//   out      : cleared, then filled with the ids to reclaim
template <typename Table>
void SelectReclaimableSessionIds(const Table& table,
                                 unsigned long self_pid,
                                 std::vector<unsigned long>* out) {
  out->clear();
  for (const auto& row : table) {
    if (ShouldReclaimOwner(row.second, self_pid)) {
      out->push_back(row.first);
    }
  }
}

// ADR 0067: dropping an ORPHAN session is NOT a "the user left Fluxing"
// signal.
//
// The reclaim runs inside AddSession, right before a new live session is
// registered. A crashed host says nothing about where the user's
// attention is, and the caller is bringing up a live session right now.
// Flipping the presence state here would collapse the quick panel while
// the user is typing. Same principle as ADR 0032 (an own UI session end
// is not a user switch either).
inline bool MayFlipPresenceOnSessionEnd(bool is_orphan_reclaim) {
  return !is_orphan_reclaim;
}

// ADR 0067: the active session pointer.
//
// A normal session end always clears it (legacy behaviour, unchanged).
// An orphan reclaim must only clear it when the reclaimed session IS the
// active one; otherwise it would wipe the pointer of a live session and
// break every live-state lookup that depends on it.
inline bool ShouldClearActiveSessionOnEnd(bool is_orphan_reclaim,
                                          unsigned long active_session,
                                          unsigned long ended_session) {
  if (!is_orphan_reclaim)
    return true;
  return active_session == ended_session;
}

}  // namespace session_reclaim
}  // namespace fluxing
