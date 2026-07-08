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

namespace weasel {
namespace deployer {

template <typename ClientT>
class MaintenanceGuard {
 public:
  explicit MaintenanceGuard(ClientT& client)
      : client_(client), entered_(false) {
    if (client_.Connect()) {
      client_.StartMaintenance();
      entered_ = true;
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