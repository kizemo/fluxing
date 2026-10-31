// spec 036 + spec 038 → spec 092: TestQuickPanelDialog
//
// 历史沿革:
//   spec 036 基线(6 断言):面板创建 2 个原生 Win32 BUTTON
//   (ID_QUICKPANEL_BTN_ASCII / ID_QUICKPANEL_BTN_DEPLOY)+ 1 个原生关闭 X(IDCANCEL)。
//   spec 038 改造(15 断言 / 6 用例):原生按钮换成 4 个 Fluxing 控件
//   (Label + Panel + Toggle + Button)+ 1 个原生 X。
//   v0.18.29.0 (spec 045):控件创建从 OnCreate 移到 Show();面板本体随后又做过
//   GDI v3-rev3 整体重写(4 按钮自绘、零子控件、类名 FluxingQuickPanel_v3)。
//
// spec 092(本文件当前形态)—— 缺陷定性:"假绿"。
//   老断言依赖的"5 个子控件"布局在现行生产代码里早已不存在(spec 045 迁移 +
//   v3-rev3 重写),但本文件当时并未跟进,被一段 printf("SKIP: ...") 早退整体
//   短路:15 条老断言一次都没跑过,suite 却是绿的。
//   spec 092 的处置:
//   - 删掉 SKIP 早退 + 其后不可达的死代码区;
//   - 按**现行**生产契约重写为实跑断言(不沿用 TODO spec 046/047 的
//     "11 Fluxing + 1 native close" 预期 —— 那个布局已不存在);
//   - 老的 spec 038 T1–T6(WM_CREATE 造 5 个子控件 / WM_COMMAND 旧 ID /
//     WM_KEYDOWN / WM_KILLFOCUS / WM_TIMER / IDCANCEL)依赖已消失的控件树,
//     随 SKIP 一并移除;其现行契约由下面的 T17–T19 与 TestQuickPanelRefactor 覆盖。
//
// 本文件现在的分工(spec 092):
//   - 状态与持久化面: T8 / T11 / T12 / T13 / T14 / T15 / T16(全部实跑)
//   - 现行结构锚点:   T17 / T18 / T19(窗口类名 / 零子窗口 / Hide 不销毁 + Show 复用)
//   - 命中测试与点击路由的细粒度断言在 TestQuickPanelRefactor,本文件不重复。
//
// 生产代码按 L24 link-probe 模式直接链接 WeaselServer/QuickPanelDialog.cpp。

#include "stdafx.h"
#include "../../WeaselServer/QuickPanelDialog.h"
#include "../../include/QuickPanelAlpha.h"

#include <cstdio>
#include <fstream>

static int g_pass = 0;
static int g_fail = 0;

static void Check(bool cond, const char* label) {
  if (cond) {
    ++g_pass;
    std::printf("  PASS: %s\n", label);
  } else {
    ++g_fail;
    std::printf("  FAIL: %s\n", label);
  }
}

// Count ALL descendants (any class). EnumChildWindows is recursive.
static int CountAllDescendants(HWND parent) {
  int count = 0;
  struct Local {
    static BOOL CALLBACK Cb(HWND, LPARAM lParam) {
      ++(*reinterpret_cast<int*>(lParam));
      return TRUE;
    }
  };
  EnumChildWindows(parent, &Local::Cb, reinterpret_cast<LPARAM>(&count));
  return count;
}

int main() {
  // ----------------------------------------------------------------
  // v0.19.0.30: SetOn* wiring end-to-end tests (spec 070 follow-up).
  // Investigator 报告 8 项静态检查全 PASS 但 0 test 覆盖 callback 真的执行。
  // 这里直接 invoke s_onUserDict / s_onShortcut 验证 setter 存进去了。
  // ----------------------------------------------------------------
  std::printf("\n[v0.19.0.30] SetOn* wiring end-to-end\n");

  // T7: SetOnUserDict 注入 → 静态字段 s_onUserDict 持有
  // v0.19.0.60 (Phase L 调整 1): SetOnUserDict / s_onUserDict 整体下线, T7 跳。
#if 0
  {
    int counter = 0;
    QuickPanelDialog::SetOnUserDict([&counter]() { ++counter; });
    if (QuickPanelDialog::s_onUserDict) {
      QuickPanelDialog::s_onUserDict();
    }
    Check(counter == 1,
          "T7a: SetOnUserDict 注入 + invoke 一次 -> counter==1");
    if (QuickPanelDialog::s_onUserDict) {
      QuickPanelDialog::s_onUserDict();
    }
    Check(counter == 2,
          "T7b: 二次 invoke -> counter==2 (function 真的被持有)");
    QuickPanelDialog::SetOnUserDict(nullptr);
    Check(QuickPanelDialog::s_onUserDict == nullptr,
          "T7c: SetOnUserDict(nullptr) 复位,字段清空");
  }
#endif

  // T8: SetOnShortcut 注入 -> s_onShortcut 持有
  {
    int counter = 0;
    QuickPanelDialog::SetOnShortcut([&counter]() { ++counter; });
    if (QuickPanelDialog::s_onShortcut) {
      QuickPanelDialog::s_onShortcut();
    }
    Check(counter == 1, "T8a: SetOnShortcut 注入 + invoke -> counter==1");
    if (QuickPanelDialog::s_onShortcut) {
      QuickPanelDialog::s_onShortcut();
    }
    Check(counter == 2, "T8b: 二次 invoke -> counter==2");
    QuickPanelDialog::SetOnShortcut(nullptr);
    Check(QuickPanelDialog::s_onShortcut == nullptr,
          "T8c: SetOnShortcut(nullptr) 复位,字段清空");
  }

  // T9: Setter 反复注入,后一次胜出
  // v0.19.0.60 (Phase L 调整 1): SetOnUserDict 已下线, T9 跳。
#if 0
  {
    int counter1 = 0;
    int counter2 = 0;
    QuickPanelDialog::SetOnUserDict([&counter1]() { ++counter1; });
    QuickPanelDialog::SetOnUserDict([&counter2]() { ++counter2; });
    if (QuickPanelDialog::s_onUserDict) {
      QuickPanelDialog::s_onUserDict();
    }
    Check(counter1 == 0,
          "T9a: 第二次 SetOnUserDict 覆盖 -> counter1==0 (旧 cb 未 invoke)");
    Check(counter2 == 1, "T9b: 新 cb invoke -> counter2==1");
    QuickPanelDialog::SetOnUserDict(nullptr);
  }
#endif

  // T10: 模拟 WM_LBUTTONUP click 路径 hit 路由(hit==2 -> UserDict,hit==3 ->
  // Shortcut) v0.19.0.60 (Phase L 调整 1): hit==2 路径已 no-op, T10 仅保留
  // hit==3 Shortcut 部分。
#if 0
  {
    int userDictCount = 0;
    int shortcutCount = 0;
    QuickPanelDialog::SetOnUserDict(
        [&userDictCount]() { ++userDictCount; });
    QuickPanelDialog::SetOnShortcut(
        [&shortcutCount]() { ++shortcutCount; });
    // 模拟 cpp:564-567 路由逻辑
    // 第一次 click: hit=2 (UserDict button)
    int hit = 2;
    if (hit == 2 && QuickPanelDialog::s_onUserDict)
      QuickPanelDialog::s_onUserDict();
    // 第二次 click: hit=3 (Shortcut button)
    hit = 3;
    if (hit == 3 && QuickPanelDialog::s_onShortcut)
      QuickPanelDialog::s_onShortcut();
    // 第三次 click: hit=2 (UserDict button again)
    hit = 2;
    if (hit == 2 && QuickPanelDialog::s_onUserDict)
      QuickPanelDialog::s_onUserDict();
    Check(userDictCount == 2,
          "T10a: hit==2 路由 2 次 -> s_onUserDict invoke 2 次 -> userDictCount==2");
    Check(shortcutCount == 1,
          "T10b: hit==3 路由 1 次 -> s_onShortcut invoke 1 次 -> shortcutCount==1");
    QuickPanelDialog::SetOnUserDict(nullptr);
    QuickPanelDialog::SetOnShortcut(nullptr);
  }
#endif

  // T11: kShowGraceMs 常量 sanity (Bug 1 grace guard fix verification)
  {
    Check(QuickPanelDialog::kShowGraceMs == 2000,
          "T11a: QuickPanelDialog::kShowGraceMs == 2000 (v0.19.0.24 spec)");
    Check(QuickPanelDialog::kShowGraceMs > 0,
          "T11b: kShowGraceMs > 0 (grace 期内必不 Hide)");
  }

  // ----------------------------------------------------------------
  // spec 084 / ADR 0013: 长显模式的不透明度系数演算(不开窗口也能断言)
  // ----------------------------------------------------------------
  std::printf("\n[spec 084] QuickPanel alpha factor\n");
  {
    // T12a: 满值系数 => 逐像素 alpha 不变(零回归保证,ADR 0013 D2)
    Check(fluxing::ScaleAlpha(220, fluxing::kAlphaFactorFull) == 220,
          "T12a: ScaleAlpha(220, 255) == 220 (满值系数逐像素不变)");
    Check(fluxing::ScaleAlpha(80, fluxing::kAlphaFactorFull) == 80,
          "T12b: ScaleAlpha(80, 255) == 80 (满值系数逐像素不变)");
    Check(fluxing::ScaleAlpha(255, fluxing::kAlphaFactorFull) == 255,
          "T12c: ScaleAlpha(255, 255) == 255 (彩色像素不透明度不变)");
    // T12d/T12e: ScaleAlpha 数学钉死(spec 089 把 idle 由 153 提到 217 = 85%)
    Check(fluxing::ScaleAlpha(255, fluxing::kAlphaFactorIdle) == 217,
          "T12d: ScaleAlpha(255, 217) == 217 (spec 089: 85% 系数)");
    Check(fluxing::ScaleAlpha(220, fluxing::kAlphaFactorIdle) == 187,
          "T12e: ScaleAlpha(220, 217) == 187 (ScaleAlpha 数学与面板常量解耦)");
    // T12f: 渐变步进不越过目标(ADR 0013 D3)
    Check(fluxing::StepAlpha(255, 51, fluxing::kAlphaFadeStep) == 231,
          "T12f: StepAlpha(255 -> 51, 24) == 231 (一次退 24)");
    Check(fluxing::StepAlpha(51, 255, fluxing::kAlphaFadeStep) == 75,
          "T12g: StepAlpha(51 -> 255, 24) == 75 (一次进 24)");
    Check(fluxing::StepAlpha(60, 51, fluxing::kAlphaFadeStep) == 51,
          "T12h: StepAlpha(60 -> 51, 24) == 51 (不越过目标)");
    Check(fluxing::StepAlpha(51, 51, fluxing::kAlphaFadeStep) == 51,
          "T12i: StepAlpha(51 -> 51, 24) == 51 (已到位不动)");
  }

  // ----------------------------------------------------------------
  // spec 084 / ADR 0013 D4: 长显开关往返
  // ----------------------------------------------------------------
  // ADR 0019: 本批测试的持久化落点改道临时文件 —— 勾选开关现在会落盘，
  // 单元测试绝不能污染真实 %APPDATA%\Rime\quickpanel.state。T14 末尾恢复默认路径。
  static std::wstring g_qpStateTestPath;
  {
    wchar_t tmpDir[MAX_PATH] = {0};
    DWORD nDir = ::GetTempPathW(MAX_PATH, tmpDir);
    if (nDir > 0 && nDir < MAX_PATH) {
      g_qpStateTestPath = std::wstring(tmpDir) + L"fluxing_qp_state_test.state";
      ::DeleteFileW(g_qpStateTestPath.c_str());
      QuickPanelDialog::SetStatePath(g_qpStateTestPath);
    }
    Check(!g_qpStateTestPath.empty(), "T13a0: 测试状态路径可用(落点已重定向)");
  }
  {
    Check(!QuickPanelDialog::AlwaysShowMode(),
          "T12j: 默认不是长显模式(长显默认关)");
    QuickPanelDialog::SetAlwaysShowMode(true);
    Check(QuickPanelDialog::AlwaysShowMode(),
          "T12k: SetAlwaysShowMode(true) 后 AlwaysShowMode() == true");
    Check(
        QuickPanelDialog::CurrentMode() == QuickPanelDialog::Mode::kAlwaysShow,
        "T12l: CurrentMode() == kAlwaysShow");
    QuickPanelDialog::SetAlwaysShowMode(false);
    Check(!QuickPanelDialog::AlwaysShowMode(),
          "T12m: SetAlwaysShowMode(false) 后 AlwaysShowMode() == false");
    Check(QuickPanelDialog::CurrentMode() == QuickPanelDialog::Mode::kHidden,
          "T12n: CurrentMode() == kHidden");
  }

  // ----------------------------------------------------------------
  // T13: ADR 0019 — 常驻勾选态跨进程持久化。根因回归: v1.1.1 装包重启后
  //      勾选丢失退回瞬时模式, 用户观察到"调出后几秒自动消失"。
  // ----------------------------------------------------------------
  {
    Check(!QuickPanelDialog::AlwaysShowMode(),
          "T13a: 默认启动不长显(勾选清零是旧缺陷本体)");
    DeleteFileW(g_qpStateTestPath.c_str());
    Check(QuickPanelDialog::LoadPersistedAlwaysShowMode(),
          "T13b: 状态文件缺失 => 默认常驻(spec 089 / ADR 0022), 不报错不崩溃");
    // 1) 模拟"上一会话勾选过"留下的旧文件 => Restore 复活勾选
    { std::ofstream f(g_qpStateTestPath.c_str(), std::ios::binary);
      f << "always_show=1\r\n"; }
    Check(QuickPanelDialog::LoadPersistedAlwaysShowMode(),
          "T13c: CRLF 旧文件也能读回(容忍尾部 \\r)");
    QuickPanelDialog::RestoreAlwaysShowMode();
    Check(QuickPanelDialog::AlwaysShowMode(),
          "T13d: 重启后勾选自动恢复 —— 面板不再几秒后消失(装机验收本体)");
    // 2) Restore 只恢复模式, 绝不 auto-show 面板(spec 055 不变量保留)
    HWND afterRestore = QuickPanelDialog::ActiveHwnd();
    Check(afterRestore == NULL || !IsWindowVisible(afterRestore),
          "T13e: Restore 不 auto-show 面板");
    // 3) 勾选/取消即时落盘(往返), 内容逐字节可断言
    QuickPanelDialog::SetAlwaysShowMode(false);
    { std::ifstream f(g_qpStateTestPath.c_str(), std::ios::binary);
      std::string body((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
      Check(body == "always_show=0\n", "T13f: 取消勾选即时落盘 always_show=0"); }
    QuickPanelDialog::SetAlwaysShowMode(true);
    { std::ifstream f(g_qpStateTestPath.c_str(), std::ios::binary);
      std::string body((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
      Check(body == "always_show=1\n", "T13g: 勾选即时落盘 always_show=1"); }
    // 4) 坏文件容错 => 默认关
    { std::ofstream f(g_qpStateTestPath.c_str(), std::ios::binary);
      f << "garbage!"; }
    Check(
        !QuickPanelDialog::LoadPersistedAlwaysShowMode(),
        "T13h: 文件存在但内容非 always_show=1 => 关(与「缺失=>常驻」语义不同)");
    QuickPanelDialog::SetAlwaysShowMode(false);  // 收尾复位默认态
    DeleteFileW(g_qpStateTestPath.c_str());
  }

  // ----------------------------------------------------------------
  // T14: ADR 0019 — ESC 不再隐藏快捷设置栏(显隐只靠 Alt+, 快捷键)。
  //      场景刻意选 常驻=false —— 与修复前"ESC 收起"的分支一致。
  // ----------------------------------------------------------------
  {
    Check(!QuickPanelDialog::AlwaysShowMode(), "T14a: 前置: 当前非长显模式");
    QuickPanelDialog::Show(false, [](){}, [](){}, [](){},
                           [](bool){}, [](){}, [](){});
    HWND panel = QuickPanelDialog::ActiveHwnd();
    Check(panel != NULL, "T14b: 面板已 Show()");
    Check(panel != NULL && IsWindowVisible(panel), "T14c: Show 后面板可见");
    LRESULT escRet = QuickPanelDialog::WndProc(panel, WM_KEYDOWN, VK_ESCAPE, 0);
    Check(escRet == 0, "T14d: ESC 被消费(返回 0, 不落到系统提示音路径)");
    Check(IsWindowVisible(panel),
          "T14e: ESC 不再隐藏面板 —— 用户口径『显隐只靠 Alt+,』");
    QuickPanelDialog::Hide();
    Check(!IsWindowVisible(panel), "T14f: Alt+, 等价路径 Hide() 收起仍有效");
    QuickPanelDialog::SetStatePath(L"");  // 恢复默认落点(测试不再写任何文件)
  }

  // ----------------------------------------------------------------
  // T17–T19(spec 092): 现行面板结构锚点。
  //   v3-rev3 整体重写后的现行生产契约:
  //   - 窗口类名固定 FluxingQuickPanel_v3;
  //   - 面板是 WS_POPUP 自绘顶层窗,零子控件(4 个按钮全部 GDI 直绘);
  //   - Hide() 只 KillTimer + ShowWindow(SW_HIDE),**不销毁**窗口;
  //     再次 Show() 复用同一个 HWND(不重建)。
  //   这三条此前只活在"老 spec 038 断言"的注释里,没有任何测试钉住 ——
  //   于是设计漂移可以静默发生(spec 045 漂移过一次,suite 仍然绿)。
  // ----------------------------------------------------------------
  std::printf("\n[spec 092] 现行面板结构锚点\n");
  {
    HWND panel = QuickPanelDialog::ActiveHwnd();
    Check(panel != NULL, "T17a: 前置: 面板 HWND 存在(T14 已 Show 过)");

    wchar_t cls[64] = {0};
    GetClassNameW(panel, cls, 64);
    Check(wcscmp(cls, L"FluxingQuickPanel_v3") == 0,
          "T17b: 窗口类名 == FluxingQuickPanel_v3");

    Check(CountAllDescendants(panel) == 0,
          "T18a: 面板零子控件(4 按钮 GDI 直绘,不是子窗口)");

    QuickPanelDialog::Hide();
    Check(IsWindow(panel), "T19a: Hide() 后窗口句柄仍有效(未销毁)");
    Check(!IsWindowVisible(panel), "T19b: Hide() 后窗口不可见");

    QuickPanelDialog::Show(false, [](){}, [](){}, [](){},
                           [](bool){}, [](){}, [](){});
    Check(QuickPanelDialog::ActiveHwnd() == panel,
          "T19c: 再次 Show() 复用同一 HWND(未重建窗口)");
    Check(IsWindowVisible(panel), "T19d: 再次 Show() 后窗口可见");
    QuickPanelDialog::Hide();
  }

  // ----------------------------------------------------------------
  // T15 / T16 (ADR 0020): 勾选态介质必须落在用户数据目录里。
  //   1.1.2 把落点硬编码成 %APPDATA%\Rime\quickpanel.state,与
  //   WeaselUserDataPath() 分叉(用户数据目录被指到 D 盘时状态会单独住一份)。
  //   T15 对旧实现必红;T16 覆盖旧落点只读兜底(存量勾选不丢)。
  // ----------------------------------------------------------------
  std::printf("\n[ADR 0020] quickpanel.state 落点跟随用户数据目录\n");
  {
    auto trim_sep = [](std::wstring s) {
      while (!s.empty() && (s.back() == L'\\' || s.back() == L'/'))
        s.pop_back();
      return s;
    };
    auto ieq = [](const std::wstring& a, const std::wstring& b) {
      if (a.size() != b.size())
        return false;
      for (size_t i = 0; i < a.size(); ++i) {
        wchar_t ca = a[i];
        wchar_t cb = b[i];
        if (ca >= L'A' && ca <= L'Z')
          ca = (wchar_t)(ca - L'A' + L'a');
        if (cb >= L'A' && cb <= L'Z')
          cb = (wchar_t)(cb - L'A' + L'a');
        if (ca != cb)
          return false;
      }
      return true;
    };
    auto same_path = [&](const std::wstring& a, const std::wstring& b) {
      return ieq(trim_sep(a), trim_sep(b));
    };
    auto parent_of = [](const std::wstring& p) {
      const size_t sep = p.find_last_of(L"\\/");
      return (sep == std::wstring::npos) ? std::wstring() : p.substr(0, sep);
    };

    const std::wstring def = QuickPanelDialog::DefaultStatePath();
    const std::wstring legacy = QuickPanelDialog::LegacyStatePath();
    const std::wstring expected_dir = ResolveFluxingUserDataPath().wstring();
    wchar_t appdata[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(L"%AppData%", appdata, _countof(appdata));
    const std::wstring old_dir = std::wstring(appdata) + L"\\Rime";

    const size_t sep = def.find_last_of(L"\\/");
    const std::wstring fname =
        (sep == std::wstring::npos) ? def : def.substr(sep + 1);
    Check(fname == L"quickpanel.state",
          "T15a: 默认落点文件名 == quickpanel.state");
    Check(same_path(parent_of(def), expected_dir),
          "T15b: 默认落点父目录 == WeaselUserDataPath() 解析出的用户数据目录");
    Check(!same_path(parent_of(def), old_dir),
          "T15c: 默认落点不再落在 %APPDATA%\\Rime(旧实现的精确特征)");
    Check(!same_path(def, legacy),
          "T15d: 默认落点 != 旧落点(旧路径只作只读兜底)");
  }
  {
    wchar_t tmpDir[MAX_PATH] = {0};
    ::GetTempPathW(MAX_PATH, tmpDir);
    const std::wstring primary =
        std::wstring(tmpDir) + L"fluxing_qp_primary_test.state";
    const std::wstring legacy =
        std::wstring(tmpDir) + L"fluxing_qp_legacy_test.state";
    ::DeleteFileW(primary.c_str());
    ::DeleteFileW(legacy.c_str());

    Check(QuickPanelDialog::LoadPersistedAlwaysShowModeFrom(primary, legacy),
          "T16a: 两个落点都没有 => 默认常驻(spec 089)");
    {
      std::ofstream f(legacy.c_str(), std::ios::binary);
      f << "always_show=1\n";
    }
    Check(QuickPanelDialog::LoadPersistedAlwaysShowModeFrom(primary, legacy),
          "T16b: 新落点缺失 => 读旧落点兜底(存量勾选不丢)");
    {
      std::ofstream f(primary.c_str(), std::ios::binary);
      f << "always_show=0\n";
    }
    Check(!QuickPanelDialog::LoadPersistedAlwaysShowModeFrom(primary, legacy),
          "T16c: 新落点优先(新=0 旧=1 => 结果为关)");
    {
      std::ofstream f(primary.c_str(), std::ios::binary);
      f << "always_show=1\n";
    }
    Check(QuickPanelDialog::LoadPersistedAlwaysShowModeFrom(primary, legacy),
          "T16d: 新落点 always_show=1 => 开");
    // T16e: 只剩旧落点、且明确不兜底 => 不读 legacy; 结果仍是默认常驻(spec 089)
    ::DeleteFileW(primary.c_str());
    Check(QuickPanelDialog::LoadPersistedAlwaysShowModeFrom(primary, L""),
          "T16e: legacy 传空串 + 新落点缺失 => 默认常驻(不兜底旧落点)");
    ::DeleteFileW(primary.c_str());
    ::DeleteFileW(legacy.c_str());
  }

  std::printf("\n=== %d / %d assertions passed ===\n", g_pass, g_pass + g_fail);
  std::fflush(stdout);
  // L48: ExitProcess bypasses atexit static destructors. The
  // FluxingD2DRenderer singleton has a destructor crash on
  // TestQuickPanelDialog specifically because the Fluxing control
  // HWNDs were nested under a shared host whose OnDestroy fires
  // before the singleton's static destructor; ComPtr<ID2D1Factory>
  // release then races with the HWND cleanup. TestFluxingComponents
  // does not exhibit this because each test owns its own host +
  // control and resets within the test function body. See
  // lessons-learned L48 for the full root-cause post-mortem.
  ExitProcess(g_fail == 0 ? 0 : 1);
}
