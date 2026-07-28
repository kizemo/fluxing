// spec 042 - MaintenanceGuard (RAII for WeaselServer maintenance mode).
//
// Incident (L55): R4 in `Configurator.cpp` shows three maintenance
// intervals (UpdateWorkspace / DictManagement / SyncUserData) using
// naked `client.StartMaintenance()` ... `client.EndMaintenance()`
// pairs with no try/finally and no RAII. If the WeaselDeployer.exe
// process is killed mid-flight (taskkill /F, AV quarantine, access
// violation), the destructor of the caller function never runs, and
// EndMaintenance() is never called. The WeaselServer side stays in
// `m_disabled = true` state forever, and the user has to reboot to
// recover.
//
// Cure: an RAII guard that calls StartMaintenance on construction
// (if client.Connect() succeeds) and EndMaintenance on destruction
// (also after re-connecting; idempotent wrt WeaselServer's
// `if (m_disabled) Initialize()` guard).
//
// Design notes:
// - Templated on ClientT so the test (TestOrphanRecovery) can pass
//   a MockClient with the same 3-method interface. No virtual
//   functions, no inheritance: pure template duck-typing.
// - Destructor is `noexcept` and swallows all exceptions, so
//   throw-from-dtor does not call std::terminate.
// - Non-copyable, non-movable: single owner per maintenance interval.
// - entered() exposes the "did we actually enter maintenance" state
//   for test assertions and logging.

#pragma once

#include <future>
#include <thread>
#include <chrono>

namespace weasel {
namespace deployer {

// v0.68 fix (L##-PhaseL-7): wrapped StartMaintenance in std::thread +
// std::future wait_for(5s default). If WeaselServer.exe (PPL, cannot
// be taskkill'd) holds the pipe but does not respond to
// START_MAINTENANCE, the old code blocked in PipeChannel::_Receive's
// ReadFile(pipe, ..., NULL) forever, hanging WeaselDeployer.exe /install
// inside NSIS ExecWait (line 760 of install.nsi). Now: timeout -> detach
// thread -> entered_ stays false -> skip StartMaintenance, proceed with
// rime->deploy() anyway. Worst case: user sees a stale-schema prompt,
// recoverable by `WeaselDeployer.exe /deploy` from a clean shell.
template <typename ClientT>
class MaintenanceGuard {
 public:
  explicit MaintenanceGuard(ClientT& client, DWORD timeout_ms = 5000)
      : client_(client), entered_(false) {
    if (client_.Connect()) {
      // std::async's future destructor blocks on the still-running task
      // after timeout (C++14 spec); std::thread + std::promise gives
      // clean detach semantics on timeout.
      std::promise<bool> p;
      std::future<bool> fut = p.get_future();
      std::thread th([this, &p] {
        bool ok = false;
        try {
          client_.StartMaintenance();
          ok = true;
        } catch (...) {
          // swallow — guard semantics: never throw from a callback.
        }
        try {
          p.set_value(ok);
        } catch (...) {
          // promise may already be satisfied on timeout-detach path.
        }
      });
      if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) ==
          std::future_status::ready) {
        entered_ = fut.get();
        if (th.joinable()) {
          th.join();
        }
      } else {
        // Timeout: WeaselServer (PPL) holds pipe unresponsive. Detach
        // the thread so it can leak in ReadFile without blocking the
        // guard destructor. entered_ stays false -> no EndMaintenance.
        if (th.joinable()) {
          th.detach();
        }
      }
    }
  }

  ~MaintenanceGuard() noexcept {
    if (!entered_) {
      return;
    }
    try {
      if (client_.Connect()) {
        client_.EndMaintenance();
      }
      // If Connect() fails, WeaselServer is down; nothing to clean
      // up on the client side, and we should not throw from dtor.
    } catch (...) {
      // Swallow: never throw from a destructor. The user-visible
      // symptom is "librime stays disabled" which is bad but
      // recoverable (reboot); std::terminate is worse.
    }
  }

  // Non-copyable, non-movable: single owner per maintenance interval.
  MaintenanceGuard(const MaintenanceGuard&) = delete;
  MaintenanceGuard& operator=(const MaintenanceGuard&) = delete;
  MaintenanceGuard(MaintenanceGuard&&) = delete;
  MaintenanceGuard& operator=(MaintenanceGuard&&) = delete;

  bool entered() const noexcept { return entered_; }

 private:
  ClientT& client_;
  bool entered_;
};

}  // namespace deployer
}  // namespace weasel