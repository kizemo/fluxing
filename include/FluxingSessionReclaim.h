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

#include <utility>
#include <vector>

namespace fluxing {
namespace session_reclaim {

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
