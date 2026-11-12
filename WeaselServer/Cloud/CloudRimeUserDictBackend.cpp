// spec 100 阶段 D · T305 用户词典后端（rime levers 实现）
//
// 只进 WeaselServer 工程，不进 test/TestFluxingCloud（单测用假实现）。
//
// ★
// 通道（真相源：docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md）
//   导出 = `backup_user_dict`（快照，含墓碑 / dee / 逐条 tick / /tick）
//   还原 = `restore_user_dict`（内部即 UserDbMerger：abs(commits) 择优、
//          dee 取 max、tick 抬升；幂等）
//   **不用** export/import：那对原语会丢 dee/tick、跳过墓碑、把 -1 放大成
//   -2、把 0 写成垃圾条目，还会给不存在的库静默建库。
//
// ★ 互斥（CLAUDE.md 硬约束 + leveldb LOCK）
//   备份 / 还原必须发生在「维护态」里。维护态必须经由
//   RimeWithWeaselHandler（它会一并清掉会话表），而 handler 归
//   WeaselServerApp 所有 ⇒ 这里通过 CloudUserDictIo 的维护态钩子回调。
//
// PCH 纪律：PrecompiledHeader=NotUsing + 自带 #define NOMINMAX。

#define NOMINMAX

#include "CloudUserDictIo.h"

#include <rime_api.h>
#include <rime_levers_api.h>

// spec 100 阶段 D：维护态宿主（WeaselServerApp 传入它的 handler）。
#include "CloudMaintenanceHost.h"
#include <RimeWithWeasel.h>

// 批次 AQ · rime 生命周期互斥体（ADR 0061）。
#include "FluxingRimeLifecycle.h"

#include <windows.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fluxing {
namespace cloud {

namespace {

// 维护态宿主（WeaselServerApp 注册）。
RimeWithWeaselHandler* g_maintenance_host = nullptr;

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty())
    return std::wstring();
  const int n =
      MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  if (n <= 0)
    return std::wstring();
  std::wstring w((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
  return w;
}

std::string WideToUtf8(const std::wstring& w) {
  if (w.empty())
    return std::string();
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                    nullptr, 0, nullptr, nullptr);
  if (n <= 0)
    return std::string();
  std::string s((size_t)n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr,
                      nullptr);
  return s;
}

bool ReadFileText(const std::wstring& path, std::string* out) {
  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in)
    return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  return true;
}

bool WriteFileText(const std::wstring& path, const std::string& text) {
  std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!out)
    return false;
  out.write(text.data(), (std::streamsize)text.size());
  out.flush();
  return out.good();
}

RimeLeversApi* GetLevers() {
  RimeModule* module = RimeFindModule("levers");
  if (module == nullptr || module->get_api == nullptr)
    return nullptr;
  return reinterpret_cast<RimeLeversApi*>(module->get_api());
}

// ---------------------------------------------------------------------------
// ★ SEH 护栏（ADR 0061）
//
// 崩溃根因：rime 一旦 finalize，Registry 被清空 ⇒ levers 里的
// UserDb::Require("userdb") 返回 nullptr ⇒ UserDictManager::GetUserDictList
// 里 component->extension() 空指针解引用 ⇒ 0xC0000005。
//
// 这里把每个 levers 调用都包进 __try/__except：访问冲突一律降级成
// 「本轮失败 + 中文错误」，**绝不让它变成进程崩溃**。
//
// ⚠ MSVC 规定：含 __try 的函数不能有需要对象展开的局部变量 ⇒ 这些函数
//   刻意只收指针 / 整数，不放任何 C++ 对象。
// ---------------------------------------------------------------------------

// 返回 1 = 初始化成功，0 = 正常返回「没有词典」，-1 = 访问冲突。
int SafeUserDictIteratorInit(RimeLeversApi* levers, RimeUserDictIterator* iter) {
  __try {
    return levers->user_dict_iterator_init(iter) ? 1 : 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

// 返回 1 = 取到名字，0 = 枚举结束，-1 = 访问冲突。
int SafeNextUserDict(RimeLeversApi* levers,
                      RimeUserDictIterator* iter,
                      const char** name) {
  __try {
    const char* n = levers->next_user_dict(iter);
    if (name != nullptr)
      *name = n;
    return n != nullptr ? 1 : 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    if (name != nullptr)
      *name = nullptr;
    return -1;
  }
}

void SafeUserDictIteratorDestroy(RimeLeversApi* levers,
                                 RimeUserDictIterator* iter) {
  __try {
    levers->user_dict_iterator_destroy(iter);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // 访问冲突：忽略（迭代器已经不可用）。
  }
}

bool SafeBackupUserDict(RimeLeversApi* levers, const char* dict) {
  __try {
    return levers->backup_user_dict(dict) ? true : false;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool SafeRestoreUserDict(RimeLeversApi* levers, const char* snapshot) {
  __try {
    return levers->restore_user_dict(snapshot) ? true : false;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

std::string RimeUserId() {
  RimeApi* rime = rime_get_api();
  if (rime == nullptr || rime->get_user_id == nullptr)
    return std::string();
  const char* uid = rime->get_user_id();
  if (uid == nullptr)
    return std::string();
  return std::string(uid);
}

// 快照的候选落点。librime 的 sync_dir 在「跑过部署」与「没跑过部署」两种
// 装机状态下指向不同位置（T102 §3.4）⇒ 逐个候选找第一个存在的。
std::vector<std::wstring> SnapshotCandidates(const std::string& dict) {
  std::vector<std::wstring> out;
  const std::wstring file = Utf8ToWide(dict) + L".userdb.txt";
  RimeApi* rime = rime_get_api();

  if (rime != nullptr) {
    char buf[1024];
    buf[0] = '\0';
    if (rime->get_user_data_sync_dir != nullptr) {
      rime->get_user_data_sync_dir(buf, sizeof(buf));
      if (buf[0] != '\0')
        out.push_back(Utf8ToWide(buf) + L"\\" + file);
    }
    const std::string uid = RimeUserId();
    if (!uid.empty()) {
      char udir[1024];
      udir[0] = '\0';
      if (rime->get_user_data_dir_s != nullptr) {
        rime->get_user_data_dir_s(udir, sizeof(udir));
        if (udir[0] != '\0')
          out.push_back(Utf8ToWide(udir) + L"\\sync\\" + Utf8ToWide(uid) +
                        L"\\" + file);
      }
    }
  }

  wchar_t cwd[MAX_PATH];
  cwd[0] = L'\0';
  if (GetCurrentDirectoryW(MAX_PATH, cwd) != 0) {
    std::wstring base = std::wstring(cwd) + L"\\sync";
    const std::string uid = RimeUserId();
    if (!uid.empty())
      base += L"\\" + Utf8ToWide(uid);
    out.push_back(base + L"\\" + file);
  }
  return out;
}

std::wstring FindSnapshotFile(const std::string& dict) {
  const std::vector<std::wstring> candidates = SnapshotCandidates(dict);
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (GetFileAttributesW(candidates[i].c_str()) != INVALID_FILE_ATTRIBUTES)
      return candidates[i];
  }
  return std::wstring();
}

// rime 生命周期锁（进程内唯一）：云同步轮之间、以及云同步轮与部署器维护之间
// 互斥（ADR 0061）。拿不到就跳过本轮，绝不阻塞用户输入。
constexpr DWORD kLifecycleWaitMs = 2000;
fluxing::rime_lifecycle::ScopedRimeLifecycleLock g_rime_lifecycle_lock;

std::wstring TempSnapshotPath(const std::string& dict) {
  wchar_t tmp[MAX_PATH];
  tmp[0] = L'\0';
  if (GetTempPathW(MAX_PATH, tmp) == 0)
    return std::wstring();
  std::wstring path(tmp);
  if (!path.empty() && path[path.size() - 1] != L'\\')
    path += L'\\';
  path += L"fluxing_cloud_" + Utf8ToWide(dict) + L"_" +
          std::to_wstring(GetCurrentProcessId()) + L".userdb.txt";
  return path;
}

class RimeUserDictBackend : public IUserDictBackend {
 public:
  bool BeginMaintenance(std::string* err) override {
    // ★ 修复（ADR 0061 · 崩溃根因）：
    //   旧实现只调 StartMaintenance()（= rime_api->finalize()，会清空 Registry）
    //   就去用 levers ⇒ UserDb::Require("userdb") 返回 nullptr ⇒ 0xC0000005。
    //   现在 Start 之后**立刻 End**：EndMaintenance 在 m_disabled 时会
    //   Initialize() ⇒ 组件重新注册、rime 回到可用态 ⇒ levers 拿得到有效的
    //   userdb 组件；而且此刻没有会话，用户库处于「无人占用」的独占态，
    //   leveldb 的 LOCK 不会与我们自己打开的实例冲突。
    //
    // ★ 另加一把 rime 生命周期锁：同一时刻只允许一轮「需要独占用户库」的操作。
    if (!g_rime_lifecycle_lock.Acquire(
            fluxing::rime_lifecycle::DefaultLockName(), kLifecycleWaitMs)) {
      if (err)
        *err = "输入法引擎正忙（可能正在部署或上一轮同步尚未结束），本轮跳过用户词典同步。";
      return false;
    }
    std::string begin_err;
    if (!CallCloudBeginMaintenance(&begin_err)) {
      g_rime_lifecycle_lock.Release();
      if (err)
        *err = begin_err.empty() ? "无法进入用户词典维护模式。" : begin_err;
      return false;
    }
    std::string end_err;
    if (!CallCloudEndMaintenance(&end_err)) {
      g_rime_lifecycle_lock.Release();
      if (err)
        *err = end_err.empty() ? "输入法引擎未能回到可用状态。" : end_err;
      return false;
    }
    return true;
  }

  bool EndMaintenance(std::string* err) override {
    // rime 已在 Begin 里回到可用态，这里只释放生命周期锁。
    if (!g_rime_lifecycle_lock.held()) {
      if (err)
        *err = "用户词典维护态未持有。";
      return false;
    }
    g_rime_lifecycle_lock.Release();
    return true;
  }

  bool ExportUserDict(std::string* out, std::string* err) override {
    if (out == nullptr) {
      if (err)
        *err = "内部错误：词典导出目标为空。";
      return false;
    }
    out->clear();

    RimeLeversApi* levers = GetLevers();
    if (levers == nullptr) {
      if (err)
        *err = "rime 的词典管理模块不可用。";
      return false;
    }
    if (levers->user_dict_iterator_init == nullptr ||
        levers->next_user_dict == nullptr ||
        levers->user_dict_iterator_destroy == nullptr ||
        levers->backup_user_dict == nullptr) {
      if (err)
        *err = "rime 的词典备份接口不可用。";
      return false;
    }

    RimeUserDictIterator iter;
    const int init = SafeUserDictIteratorInit(levers, &iter);
    if (init < 0) {
      // 访问冲突：rime 刚被 finalize（Registry 已清空）⇒ 本轮放弃，绝不崩进程。
      if (err)
        *err = "输入法引擎当前不可用（可能正在部署），本轮跳过用户词典同步。";
      return false;
    }
    if (init == 0) {
      if (err)
        *err = "没有找到可同步的用户词典。";
      return false;
    }
    std::vector<std::string> dicts;
    const char* name = nullptr;
    for (;;) {
      const int step = SafeNextUserDict(levers, &iter, &name);
      if (step < 0) {
        SafeUserDictIteratorDestroy(levers, &iter);
        if (err)
          *err = "输入法引擎当前不可用（可能正在部署），本轮跳过用户词典同步。";
        return false;
      }
      if (step == 0)
        break;
      dicts.push_back(std::string(name));
    }
    SafeUserDictIteratorDestroy(levers, &iter);

    int exported = 0;
    for (size_t i = 0; i < dicts.size(); ++i) {
      // 备份失败的词典（不存在 / 锁不住）直接跳过，不算整体失败。
      const bool backup_ok = SafeBackupUserDict(levers, dicts[i].c_str());
      if (!backup_ok)
        continue;
      const std::wstring path = FindSnapshotFile(dicts[i]);
      if (path.empty())
        continue;
      std::string text;
      if (!ReadFileText(path, &text))
        continue;
      out->append(text);
      if (!out->empty() && out->at(out->size() - 1) != '\n')
        out->append("\n");
      ++exported;
    }

    if (exported == 0) {
      if (err)
        *err = "用户词典备份失败：没有可导出的词典。";
      return false;
    }
    return true;
  }

  bool ImportUserDict(const std::string& text,
                      int* imported,
                      std::string* err) override {
    if (imported != nullptr)
      *imported = 0;

    RimeLeversApi* levers = GetLevers();
    if (levers == nullptr) {
      if (err)
        *err = "rime 的词典管理模块不可用。";
      return false;
    }
    if (levers->restore_user_dict == nullptr) {
      if (err)
        *err = "rime 的词典还原接口不可用。";
      return false;
    }

    std::vector<std::string> sections;
    std::string split_err;
    if (!SplitUserDictSnapshotSections(text, &sections, &split_err)) {
      if (err)
        *err = split_err;
      return false;
    }

    int count = 0;
    std::string last_err;
    for (size_t i = 0; i < sections.size(); ++i) {
      // 段首的 "#@/db_name" 决定这一段还原到哪个词典。
      std::string dict;
      const size_t nl = sections[i].find('\n');
      const std::string first = sections[i].substr(
          0, nl == std::string::npos ? sections[i].size() : nl);
      const size_t tab = first.find('\t');
      if (tab != std::string::npos)
        dict = first.substr(tab + 1);

      const std::wstring path = TempSnapshotPath(dict);
      if (path.empty()) {
        last_err = "无法定位临时目录。";
        continue;
      }
      if (!WriteFileText(path, sections[i])) {
        last_err = "写入临时快照失败。";
        continue;
      }
      // 批次 AQ · ADR 0061：restore 同样走 SEH 护栏 ⇒ rime 被 finalize
      // 期间访问冲突只会让本段失败，绝不崩进程。
      const bool ok = SafeRestoreUserDict(levers, WideToUtf8(path).c_str());
      DeleteFileW(path.c_str());
      if (!ok) {
        last_err = "还原词典「" + dict + "」失败。";
        continue;
      }
      ++count;
    }

    if (count == 0) {
      if (err)
        *err = last_err.empty() ? "词典还原失败。" : last_err;
      return false;
    }
    if (imported != nullptr)
      *imported = count;
    return true;
  }
};

}  // namespace

std::unique_ptr<IUserDictBackend> CreateRimeUserDictBackend() {
  return std::unique_ptr<IUserDictBackend>(new RimeUserDictBackend());
}

// 实现体留在 fluxing::cloud 内（要访问匿名命名空间里的 g_maintenance_host）；
// 全局入口见文件末尾，必须与 CloudMaintenanceHost.h 的声明严格一致。
void RegisterCloudMaintenanceHostImpl(RimeWithWeaselHandler* handler) {
  g_maintenance_host = handler;
  fluxing::cloud::SetCloudMaintenanceHooks(
      [](std::string* err) -> bool {
        if (g_maintenance_host == nullptr) {
          if (err)
            *err = "输入法引擎尚未就绪，无法进入维护模式。";
          return false;
        }
        g_maintenance_host->StartMaintenance();
        return true;
      },
      [](std::string* err) -> bool {
        if (g_maintenance_host == nullptr) {
          if (err)
            *err = "输入法引擎尚未就绪，无法退出维护模式。";
          return false;
        }
        g_maintenance_host->EndMaintenance();
        return true;
      });
}

}  // namespace cloud
}  // namespace fluxing

// 全局入口：WeaselServerApp 在构造期调用它注册维护态宿主。
// ★ 必须放在命名空间外 —— 声明在 CloudMaintenanceHost.h 里是全局作用域，
//   定义若留在 fluxing::cloud 内会得到不同的名字修饰 ⇒ LNK2001。
void RegisterCloudMaintenanceHost(RimeWithWeaselHandler* handler) {
  fluxing::cloud::RegisterCloudMaintenanceHostImpl(handler);
}
