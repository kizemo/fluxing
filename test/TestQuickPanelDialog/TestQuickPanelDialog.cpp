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
// spec 099 P0-3: 槽 0(切换到下一个输入方案)的纯逻辑 + 关于/账号说明页
// (版本与产品名的单一真相源 = PE 版本资源)。
#include "../../include/FluxingSchemaCycle.h"
#include "../../include/FluxingPanelPosition.h"
// spec 099 P0-4/P0-5/P0-6: 状态回显 / 键盘可达性 / tooltip / DPI 的
// **纯逻辑单一真相源**(生产与测试读同一份,constitution R7)。
// 测试**不复制**这些公式 —— 复制即漂移(spec 092 记的「假绿」死法)。
#include "../../include/FluxingPanelDpi.h"
#include "../../include/FluxingPanelStatus.h"
#include "../../include/FluxingPanelKeyboard.h"
#include "../../include/FluxingPanelTooltip.h"
#include "../../include/FluxingPanelLogo.h"
#include "../../WeaselServer/AboutDialog.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

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

// ===== 需求⑦ 回归护栏辅助 (T31) =====
namespace {

// 感知亮度 (299R+587G+114B)/1000 —— 与素材分析脚本同一口径。
int LogoLuma(int r, int g, int b) {
  return (299 * r + 587 * g + 114 * b) / 1000;
}

// 从当前目录向上最多 6 层找 output/fluxing-logo_small.png。
std::wstring FindLogoAsset() {
  wchar_t cwd[MAX_PATH] = {0};
  if (!GetCurrentDirectoryW(MAX_PATH, cwd))
    return L"";
  std::wstring dir(cwd);
  for (int i = 0; i < 6; ++i) {
    std::wstring cand = dir + L"\\output\\fluxing-logo_small.png";
    if (GetFileAttributesW(cand.c_str()) != INVALID_FILE_ATTRIBUTES)
      return cand;
    size_t pos = dir.find_last_of(L'\\');
    if (pos == std::wstring::npos || pos == 0)
      break;
    dir = dir.substr(0, pos);
  }
  return L"";
}

// WIC 解码 PNG → 32bppBGRA(与生产 LoadLogoWIC 同一解码器)。
bool DecodePngBgra(const std::wstring& path, std::vector<BYTE>& buf,
                   UINT& w, UINT& h) {
  Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&wic))))
    return false;
  Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
  if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                           WICDecodeMetadataCacheOnLoad,
                                           &dec)))
    return false;
  Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
  if (FAILED(dec->GetFrame(0, &frame)))
    return false;
  Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
  if (FAILED(wic->CreateFormatConverter(&conv)))
    return false;
  if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom)))
    return false;
  if (FAILED(conv->GetSize(&w, &h)) || w == 0 || h == 0)
    return false;
  const UINT stride = w * 4;
  buf.assign((size_t)stride * h, 0);
  return SUCCEEDED(conv->CopyPixels(nullptr, stride, stride * h, buf.data()));
}

}  // namespace

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
  //   - 面板是 WS_POPUP 自绘顶层窗,零子控件(6 个按钮全部 GDI 直绘);
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
          "T18a: 面板零子控件(6 按钮 GDI 直绘,不是子窗口)");

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

  // ----------------------------------------------------------------
  // spec 099 P0-3:「每个可见槽位点击必产生确定的用户可见效果」
  //
  //   槽 0 Schema  → 切换到下一个输入方案(此前是死槽,注释写着 no-op)
  //   槽 1 Phrase  → 短语库(既有)
  //   槽 2 Settings→ 快捷键设置(既有,走 SetOnShortcut)
  //   槽 3 Account → 关于/账号说明页(此前是死槽)
  //   槽 4 中英文切换 / 槽 5 全半角切换 (Fluxing v4 需求③④⑤)
  //
  // 判别性:改生产代码之前槽 0/3 的回调触发次数恒为 0 ⇒ T20g/T20j 必红。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-3] 6 槽点击路由\n");
  {
    int n_schema = 0, n_phrases = 0, n_shortcut = 0, n_login = 0, n_userfolder = 0;
    QuickPanelDialog::SetOnShortcut([&n_shortcut]() { ++n_shortcut; });
    QuickPanelDialog::Show(
        false,
        [&n_schema]() { ++n_schema; },
        [&n_userfolder]() { ++n_userfolder; },
        [&n_phrases]() { ++n_phrases; },
        [](bool) {},
        []() {},
        [&n_login]() { ++n_login; });
    HWND panel = QuickPanelDialog::ActiveHwnd();
    Check(panel != nullptr, "T20a: Show() 后面板窗口句柄非空");
    Check(IsWindow(panel) && IsWindowVisible(panel),
          "T20b: 面板窗口真实存在且可见(不是空壳指针)");

    // 判别性:槽位中心点必须被 HitTest 判为该槽位(HitTest 与 SlotRect 同源,
    // 复制布局公式才是漂移的来源)。
    bool hit_ok = true;
    for (int idx = 0; idx < QuickPanelDialog::kButtonCount; ++idx) {
      int sx = 0, sy = 0, sw = 0, sh = 0;
      QuickPanelDialog::SlotRect(idx, &sx, &sy, &sw, &sh);
      if (QuickPanelDialog::HitTest(sx + sw / 2, sy + sh / 2) != idx)
        hit_ok = false;
    }
    Check(hit_ok, "T20p: 全部槽位中心点都被 HitTest 判为对应槽位");

    // 合成点击的**确定性**:面板的「拖拽 vs 点击」判定读的是**真实光标位置**
    // (WM_LBUTTONUP 里 GetCursorPos,阈值 4 物理像素 —— 见
    // QuickPanelDialog.cpp)。真实光标一旦发生位移 ⇒ 被判成拖拽 ⇒ 回调不触发。
    //
    // 合成点击要过三道真实光标关:
    //  (a) **钉住**:按下前 / 抬起前各把光标钉一次到被点槽位,并**读回自证**
    //      (SetCursorPos 之后位置可能被指针平滑 / 外部鼠标移动带偏);
    //  (b) **DOWN 与 UP 之间不得有慢 I/O**:原来这里夹着一个 Check(printf),
    //      把「真实光标可能漂移」的时间窗从微秒拉到毫秒,是偶发红的放大器;
    //  (c) **环境干扰重试**:外部鼠标在按下与抬起之间移动时,生产侧(正确地)
    //      判成拖拽而不派发 —— 此时重新钉住再试。
    //      注意 (c) **不削弱判别性**:若生产代码回归(本该派发却不派发),
    //      光标并没有漂移,重试不会让它变绿,重试用尽后断言仍然红。
    // 若 SetCursorPos/GetCursorPos 不可用(无输入设备的非交互 window station),
    // 生产代码侧的 L178-fix 会把「取不到光标」按「无位移」处理,行为同样确定。
    auto pin_cursor = [](POINT target) {
      for (int i = 0; i < 4; ++i) {
        if (!SetCursorPos(target.x, target.y))
          return false;
        POINT now = {0, 0};
        if (!GetCursorPos(&now))
          return false;
        if (now.x == target.x && now.y == target.y)
          return true;  // 读回自证:光标确实落在目标点
      }
      return true;
    };
    auto click_slot = [&](int idx, int& counter) {
      int x = 0, y = 0, w = 0, h = 0;
      if (!QuickPanelDialog::SlotRect(idx, &x, &y, &w, &h))
        return false;
      const LPARAM lp = MAKELPARAM(x + w / 2, y + h / 2);
      POINT screen = {x + w / 2, y + h / 2};
      ClientToScreen(panel, &screen);
      constexpr int kMaxAttempt = 5;
      int active_after_down = -1;
      for (int attempt = 0; attempt < kMaxAttempt; ++attempt) {
        const int before = counter;
        pin_cursor(screen);
        QuickPanelDialog::WndProc(panel, WM_LBUTTONDOWN, 1, lp);
        // 判别性:按下后 active 索引必须落在该槽位 —— 否则点击根本不会被路由
        // (HitTest 没命中 ⇒ s_activeIdx 保持 -1 ⇒ 两条抬起分支都不派发)。
        // 必须在 UP 之前取样(UP 会把 s_activeIdx 复位成 -1)。
        active_after_down = QuickPanelDialog::s_activeIdx;
        pin_cursor(screen);  // 抵消 DOWN 期间的真实光标位移
        QuickPanelDialog::WndProc(panel, WM_LBUTTONUP, 0, lp);
        if (counter > before)
          break;  // 本次尝试已派发回调
        // 没派发 ⇒ 生产侧判成了拖拽(见 (c)):重新钉住再试。
      }
      Check(active_after_down == idx,
            "T20q: WM_LBUTTONDOWN 后 active 索引落在被点槽位");
      return true;
    };

    Check(click_slot(0, n_schema), "T20c: 槽 0 (Schema) 矩形可解析");
    Check(click_slot(1, n_phrases), "T20d: 槽 1 (Phrase) 矩形可解析");
    Check(click_slot(2, n_shortcut), "T20e: 槽 2 (Settings) 矩形可解析");
    Check(click_slot(3, n_login), "T20f: 槽 3 (Account) 矩形可解析");
    Check(n_schema == 1, "T20g: 槽 0 点击 → 方案切换回调恰好 1 次");
    Check(n_phrases == 1, "T20h: 槽 1 点击 → 短语库回调恰好 1 次");
    Check(n_shortcut == 1, "T20i: 槽 2 点击 → 快捷键设置回调恰好 1 次");
    Check(n_login == 1, "T20j: 槽 3 点击 → 关于页回调恰好 1 次");
    Check(n_userfolder == 0, "T20k: 未接槽位的回调不得被误触发");

    int bx = 0, by = 0, bw = 0, bh = 0;
    Check(!QuickPanelDialog::SlotRect(-1, &bx, &by, &bw, &bh),
          "T20l: 槽位索引 -1 无效");
    Check(!QuickPanelDialog::SlotRect(QuickPanelDialog::kButtonCount, &bx, &by,
                                       &bw, &bh),
          "T20m: 槽位索引 == kButtonCount 无效");

    // Fluxing v4 需求③④⑤: 槽 4(中英文切换)/ 槽 5(全半角切换) 必须真的可点、
    // 且回调收到的是**目标值**而不是「翻转」标记。
    {
      int n_ascii = 0;
      int n_full = 0;
      bool ascii_target = false;
      const bool expect_target =
          !QuickPanelDialog::CurrentInputState().ascii_mode;
      QuickPanelDialog::SetOnAscii([&n_ascii, &ascii_target](bool v) {
        ++n_ascii;
        ascii_target = v;
      });
      QuickPanelDialog::Show(false, nullptr, nullptr, nullptr,
                             [&n_full](bool) { ++n_full; }, nullptr, nullptr);
      Check(click_slot(4, n_ascii), "T20n: 槽 4 (中英文) 矩形可解析");
      Check(n_ascii == 1 && ascii_target == expect_target,
            "T20n2: 槽 4 点击 → 中英文回调恰好 1 次且收到目标值");
      Check(click_slot(5, n_full), "T20p2: 槽 5 (全半角) 矩形可解析");
      Check(n_full == 1, "T20p3: 槽 5 点击 → 全半角回调恰好 1 次");
      QuickPanelDialog::SetOnAscii(nullptr);
    }
    // 复位注入的回调,避免悬垂引用影响后续用例。
    QuickPanelDialog::SetOnShortcut([]() {});
    QuickPanelDialog::Hide();
  }

  // ----------------------------------------------------------------
  // spec 099 P0-3: 关于页(槽 3 的落点)+ 版本/产品名的单一真相源。
  //   版本必须来自宿主 exe 的 PE 版本资源,不得硬编码第二份字面量。
  //   探针用系统 DLL(自带版本资源),与构建产物路径解耦。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-3] 关于页 + PE 版本读取\n");
  {
    wchar_t sys_dir[MAX_PATH] = {0};
    GetSystemDirectoryW(sys_dir, _countof(sys_dir));
    const std::wstring probe = std::wstring(sys_dir) + L"\\kernel32.dll";

    std::wstring product, version;
    Check(AboutDialog::ReadVersionInfo(probe, &product, &version),
          "T21a: 能从 PE 版本资源读出 kernel32.dll 的版本信息");
    Check(!version.empty() && version.find(L'.') != std::wstring::npos,
          "T21b: 读到的版本号非空且是点分格式(L175 空串陷阱的判据)");
    Check(!product.empty(), "T21c: 读到的产品名非空");

    std::wstring p2, v2;
    Check(!AboutDialog::ReadVersionInfo(L"C:\\__fluxing_no_such_file__.dll", &p2, &v2),
          "T21d: 文件不存在 → 读版本失败(不返回假值)");

    AboutDialog::InitializeFromPath(probe);
    const std::wstring shown_version = AboutDialog::VersionText();
    Check(!shown_version.empty() &&
              shown_version.find(version) != std::wstring::npos,
          "T21e: 关于页版本号与 PE 资源读到的同源(非硬编码)");

    HWND about = AboutDialog::Show();
    Check(about != nullptr, "T21f: AboutDialog::Show() 返回非空 HWND");
    Check(IsWindowVisible(about), "T21g: 关于页窗口可见");

    std::wstring caption(256, L'\0');
    GetWindowTextW(about, &caption[0], 256);
    Check(caption.find(L"关于") != std::wstring::npos,
          "T21h: 窗口标题含「关于」");

    // 同进程内读子控件文本(EnumChildWindows 递归)。
    auto find_child = [](HWND parent, const wchar_t* needle) -> HWND {
      struct Local {
        static BOOL CALLBACK Cb(HWND h, LPARAM lp) {
          Local* self = reinterpret_cast<Local*>(lp);
          wchar_t buf[1024] = {0};
          GetWindowTextW(h, buf, _countof(buf));
          if (std::wstring(buf).find(self->needle) != std::wstring::npos) {
            self->found = h;
            return FALSE;
          }
          return TRUE;
        }
        const wchar_t* needle;
        HWND found;
      } local{needle, nullptr};
      EnumChildWindows(parent, &Local::Cb, reinterpret_cast<LPARAM>(&local));
      return local.found;
    };

    Check(find_child(about, L"本机") != nullptr,
          "T21i: 关于页含隐私说明(数据保存在本机)");
    Check(find_child(about, L"不上传") != nullptr,
          "T21j: 关于页明确「不上传」");
    Check(find_child(about, shown_version.c_str()) != nullptr,
          "T21k: 关于页显示版本号");
    const std::wstring data_dir = ResolveFluxingUserDataPath().wstring();
    Check(find_child(about, L"数据目录：") != nullptr,
          "T21l: 关于页含「数据目录：」标签");
    Check(find_child(about, data_dir.c_str()) != nullptr,
          "T21m: 关于页显示的用户数据目录 == ResolveFluxingUserDataPath()");
    Check(find_child(about, L"打开数据目录") != nullptr,
          "T21n: 关于页有「打开数据目录」按钮");
    Check(find_child(about, L"关闭") != nullptr,
          "T21o: 关于页有「关闭」按钮");
  }

  // ----------------------------------------------------------------
  // spec 099 P0-3: 槽 0 的可测纯逻辑(在可用方案列表里循环到下一个)。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-3] 方案循环(槽 0 的纯逻辑)\n");
  {
    Check(fluxing::NextSchemaId({"a", "b", "c"}, "a") == "b",
          "T23a: 当前=首 ⇒ 下一个");
    Check(fluxing::NextSchemaId({"a", "b", "c"}, "b") == "c",
          "T23b: 当前=中 ⇒ 下一个");
    Check(fluxing::NextSchemaId({"a", "b", "c"}, "c") == "a",
          "T23c: 当前=尾 ⇒ 绕回第一个");
    Check(fluxing::NextSchemaId({"a", "b", "c"}, "zz") == "a",
          "T23d: 当前不在列表 ⇒ 取第一个");
    Check(fluxing::NextSchemaId({}, "a").empty(),
          "T23e: 列表为空 ⇒ 不切(空串)");
  }

  // ----------------------------------------------------------------
  // spec 099 P0-7: 面板位置持久化 —— 跨重启保留 + 分辨率/DPI 变化后钳回可视区。
  //   落点 = <用户数据目录>\quickpanel.position(独立文件,ADR 0035);
  //   quickpanel.state 是 always_show 的单一真相源(R7)⇒ 位置不得污染它。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-7] 面板位置持久化\n");
  {
    namespace pp = fluxing::panel_position;

    // ===== 落盘格式(纯逻辑) =====
    Check(pp::FormatPosition(1234, -567) == "pos_x=1234\npos_y=-567\n",
          "T24a: 落盘格式 = pos_x / pos_y 两行 ASCII");
    auto parsed = pp::ParsePosition("pos_x=1234\npos_y=-567\n");
    Check(parsed.has_value() && parsed->x == 1234 && parsed->y == -567,
          "T24b: 往返解析(含负坐标)");
    Check(!pp::ParsePosition("pos_x=1234\n").has_value(),
          "T24c: 缺 pos_y ⇒ 不认");
    Check(!pp::ParsePosition("always_show=1\n").has_value(),
          "T24d: 不是位置文件 ⇒ 不认");
    Check(!pp::ParsePosition("pos_x=abc\npos_y=1\n").has_value(),
          "T24e: 非数字 ⇒ 不认");
    Check(!pp::ParsePosition("pos_x=999999\npos_y=1\n").has_value(),
          "T24f: 坐标越界 ⇒ 不认");
    Check(!pp::ParsePosition("").has_value(), "T24g: 空内容 ⇒ 不认");
    Check(!pp::ParsePosition("pos_x=12x\npos_y=1\n").has_value(),
          "T24h: 尾巴有垃圾字符 ⇒ 不认");

    // ===== 读写 =====
    wchar_t tmpDir[MAX_PATH] = {0};
    ::GetTempPathW(MAX_PATH, tmpDir);
    const std::wstring tmpDirW(tmpDir);
    const std::wstring posFile =
        tmpDirW + L"fluxing_qp_position_test.position";
    const std::wstring stateFile =
        tmpDirW + L"fluxing_qp_state_guard.state";
    const std::wstring missingFile =
        tmpDirW + L"fluxing_qp_position_missing.position";
    ::DeleteFileW(posFile.c_str());
    ::DeleteFileW(stateFile.c_str());
    ::DeleteFileW(missingFile.c_str());

    Check(pp::WritePositionFile(posFile, 321, -654), "T24i: 位置写盘成功");
    {
      std::ifstream probe(posFile, std::ios::binary);
      const std::string content((std::istreambuf_iterator<char>(probe)),
                                std::istreambuf_iterator<char>());
      Check(content == "pos_x=321\npos_y=-654\n", "T24j: 落盘内容符合格式");
    }
    Check(!pp::WritePositionFile(tmpDirW, 1, 2),
          "T24k: 写盘目标不可写 ⇒ 失败(不静默成功)");

    // ===== 跨重启:文件已落盘,读走生产读盘路径 =====
    QuickPanelDialog::SetPositionPath(posFile);
    Check(QuickPanelDialog::RestorePosition(), "T24l: 启动期读回持久化位置");
    Check(QuickPanelDialog::HasSavedPosition() &&
              QuickPanelDialog::SavedPosition().x == 321 &&
              QuickPanelDialog::SavedPosition().y == -654,
          "T24m: 跨重启保留位置(含负坐标)");

    // ===== 损坏文件 ⇒ 保持现状(右下角默认锚点) =====
    {
      std::ofstream bad(posFile, std::ios::binary | std::ios::trunc);
      bad << "garbage\n";
    }
    Check(!QuickPanelDialog::RestorePosition(),
          "T24n: 位置文件损坏 ⇒ 读盘失败(不认)");
    Check(!QuickPanelDialog::HasSavedPosition(),
          "T24n2: 损坏 ⇒ 运行期缓存清空(面板走默认锚点)");

    // ===== 写盘路径 =====
    Check(QuickPanelDialog::SavePositionAt(100, 200), "T24o: SavePositionAt 写盘成功");
    Check(QuickPanelDialog::SavedPosition().x == 100 &&
              QuickPanelDialog::SavedPosition().y == 200,
          "T24o2: 写盘后运行期缓存同步");

    // ===== R7 守卫:位置落盘不得动 quickpanel.state =====
    {
      std::ofstream st(stateFile, std::ios::binary | std::ios::trunc);
      st << "always_show=1\n";
    }
    std::string stateBefore;
    {
      std::ifstream f(stateFile, std::ios::binary);
      stateBefore.assign((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    }
    Check(stateBefore == "always_show=1\n", "T24p0: always_show 状态文件已就位");
    QuickPanelDialog::SavePositionAt(111, 222);
    std::string stateAfter;
    {
      std::ifstream f(stateFile, std::ios::binary);
      stateAfter.assign((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    }
    Check(stateBefore == stateAfter,
          "T24p1: 写位置不动 quickpanel.state(R7 单一真相源不被污染)");

    // ===== 钳制:分辨率 / DPI 变化后钳回可视区 =====
    std::vector<pp::Rect> areas;
    areas.push_back(pp::Rect{0, 0, 1920, 1040});     // 主屏
    areas.push_back(pp::Rect{-1920, 0, -1, 1000});  // 左侧副屏(负坐标)
    const int W = QuickPanelDialog::kPanelW;
    const int H = QuickPanelDialog::kPanelH;
    {
      pp::Point p = pp::ClampToVisibleArea(1600, 900, W, H, areas);
      Check(p.x == 1600 && p.y == 900,
            "T24q: 完全在主屏工作区内 ⇒ 原样保留");
    }
    {
      pp::Point p = pp::ClampToVisibleArea(-1500, 500, W, H, areas);
      Check(p.x == -1500 && p.y == 500,
            "T24r: 左侧副屏(负坐标)工作区内 ⇒ 原样保留(多屏)");
    }
    {
      // 规则 1 必须与规则 2 **可区分**:这里两块工作区互相重叠(Windows 允许
      // 副屏与主屏部分重叠),面板完整落在主屏内,但「中心最近」的小屏离面板
      // 中心更近 —— 若实现里没有「完整落在某个工作区内 ⇒ 原样保留」这条规则,
      // 面板会被吸到小屏里(100,500 ⇒ 222,524)。
      // 这条用例是规则 1 唯一的判别点:T24q / T24r 在规则 1 被删掉时仍全绿。
      std::vector<pp::Rect> overlap;
      overlap.push_back(pp::Rect{0, 0, 1920, 1040});   // 主屏
      overlap.push_back(pp::Rect{200, 400, 500, 600});  // 与主屏重叠的小屏
      pp::Point p = pp::ClampToVisibleArea(100, 500, W, H, overlap);
      Check(p.x == 100 && p.y == 500,
            "T24r2: 重叠多屏下完整落在某屏内 ⇒ 原样保留(不被最近屏吸走)");
    }
    {
      pp::Point p = pp::ClampToVisibleArea(1900, 900, W, H, areas);
      Check(p.x == 1920 - W && p.y == 900,
            "T24s: 超出右边界(分辨率变小)⇒ 钳回可视区");
    }
    {
      pp::Point p = pp::ClampToVisibleArea(1600, 1200, W, H, areas);
      Check(p.x == 1600 && p.y == 1040 - H,
            "T24t: 超出下边界(DPI 变化)⇒ 钳回可视区");
    }
    {
      pp::Point p = pp::ClampToVisibleArea(5000, 5000, W, H, areas);
      Check(p.x == 1920 - W && p.y == 1040 - H,
            "T24u: 完全在屏外 ⇒ 吸附回最近工作区");
    }
    {
      pp::Point p =
          pp::ClampToVisibleArea(-1500, 500, W, H, std::vector<pp::Rect>());
      Check(p.x == 0 && p.y == 0, "T24v: 没有工作区 ⇒ (0,0)");
    }
    {
      std::vector<pp::Rect> tiny;
      tiny.push_back(pp::Rect{0, 0, 100, 50});  // 容不下整块面板
      pp::Point p = pp::ClampToVisibleArea(10, 10, W, H, tiny);
      Check(p.x == 0 && p.y == 0, "T24w: 工作区容不下面板 ⇒ (0,0)");
    }

    // ===== D0176 (2026-11-06): 可见性判据 RectIntersectsVisibleArea =====
    // 这条纯函数是「拖动落盘守卫」与「Show() 复用路径自愈」的**唯一**判据,
    // 所以它自己必须有判别性:
    //   T24w5 是污染值回归 —— 2560x1440 单屏下被写坏的 3296/1983;
    //   T24w6 是边界判别 —— 「边缘相接」不算可见,用 >= / <= 实现的版本在此翻绿;
    //   T24w7 是未初始化矩形(建窗前 GetWindowRect 拿到 0 宽)不得被判可见。
    {
      const int PW = QuickPanelDialog::kPanelW;
      const int PH = QuickPanelDialog::kPanelH;
      Check(pp::RectIntersectsVisibleArea(1600, 900, PW, PH, areas),
            "T24w2: 完整落在主屏工作区内 ⇒ 相交");
      Check(pp::RectIntersectsVisibleArea(-1500, 500, PW, PH, areas),
            "T24w3: 左侧副屏(负坐标)⇒ 相交(多屏)");
      Check(pp::RectIntersectsVisibleArea(1900, 900, PW, PH, areas),
            "T24w4: 骑在右边界上(部分可见)⇒ 相交(不误杀用户的半屏摆放)");
      Check(!pp::RectIntersectsVisibleArea(3296, 1983, 368, 72, areas),
            "T24w5: 污染值回归(1920x1040 下的 3296,1983)⇒ 零交集");
      Check(!pp::RectIntersectsVisibleArea(1920, 900, PW, PH, areas),
            "T24w6: 恰好贴着右边界 ⇒ 零交集(零面积不算可见)");
      Check(!pp::RectIntersectsVisibleArea(100, 100, 0, PH, areas),
            "T24w7: 零宽矩形 ⇒ 不相交");
      Check(!pp::RectIntersectsVisibleArea(100, 100, PW, PH,
                                          std::vector<pp::Rect>()),
            "T24w8: 没有工作区 ⇒ 不相交(枚举失败不猜)");
    }

    // ===== 默认落点 =====
    {
      const std::wstring defPos = pp::DefaultPositionPath();
      const std::wstring defState = QuickPanelDialog::DefaultStatePath();
      const std::wstring posName = L"quickpanel.position";
      Check(defPos.size() >= posName.size() &&
                defPos.compare(defPos.size() - posName.size(), posName.size(),
                               posName) == 0,
            "T24x: 默认落点文件名 = quickpanel.position");
      Check(defPos != defState,
            "T24y: 默认落点 != quickpanel.state(独立文件,R7)");
      const size_t sp = defState.find_last_of(L"\\");
      const size_t dp = defPos.find_last_of(L"\\");
      Check(sp != std::wstring::npos && dp != std::wstring::npos &&
                defState.substr(0, sp) == defPos.substr(0, dp),
            "T24z: 位置与状态落在同一个用户数据目录");
    }

    // ===== 收尾:恢复默认落点 + 清临时文件 =====
    QuickPanelDialog::SetPositionPath(missingFile);
    Check(!QuickPanelDialog::RestorePosition(),
          "T24zz: 文件缺失 ⇒ 无位置(保持右下角默认锚点)");
    Check(!QuickPanelDialog::HasSavedPosition(),
          "T24zz2: 缺失 ⇒ 运行期缓存清空");
    QuickPanelDialog::SetPositionPath(L"");
    ::DeleteFileW(posFile.c_str());
    ::DeleteFileW(stateFile.c_str());
    ::DeleteFileW(missingFile.c_str());
  }

  // ----------------------------------------------------------------
  // T25 (spec 099 P0-6): DPI 缩放与物理布局。
  //   核心判据:150% ⇒ 359×54。
  //   测试直接调 fluxing::panel_dpi::* 与 QuickPanelDialog::SlotRectForSize /
  //   StatusRectForSize(与生产**同一份**公式),不复制任何布局算式(R7)。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-6] DPI 缩放与物理布局\n");
  {
    using fluxing::panel_dpi::PanelPhysicalSize;
    using fluxing::panel_dpi::ScaleLogicalByDpi;

    int w = 0, h = 0;
    PanelPhysicalSize(QuickPanelDialog::kPanelW, QuickPanelDialog::kPanelH, 96,
                      96, &w, &h);
    Check(w == 239 && h == 36, "T25a: 100% ⇒ 239×36");

    PanelPhysicalSize(QuickPanelDialog::kPanelW, QuickPanelDialog::kPanelH, 144,
                      144, &w, &h);
    Check(w == 359 && h == 54, "T25b: 150% ⇒ 359×54(P0-6 核心判据)");

    PanelPhysicalSize(QuickPanelDialog::kPanelW, QuickPanelDialog::kPanelH, 192,
                      192, &w, &h);
    Check(w == 478 && h == 72, "T25c: 200% ⇒ 478×72");

    // dpi <= 0 ⇒ 退化到 96(=100%),不塌成 0。
    Check(ScaleLogicalByDpi(QuickPanelDialog::kPanelW, 0) ==
                  QuickPanelDialog::kPanelW &&
              ScaleLogicalByDpi(QuickPanelDialog::kPanelW, -1) ==
                  QuickPanelDialog::kPanelW,
          "T25d: dpi<=0 退化到 96(=100%)");
    Check(ScaleLogicalByDpi(QuickPanelDialog::kPanelH, 0) ==
              QuickPanelDialog::kPanelH,
          "T25e: dpi<=0 时高度同样退化");
    Check(ScaleLogicalByDpi(0, 144) == 0,
          "T25f: logical=0 ⇒ 0(不产生负数)");

    // 就近舍入(.5 向上)—— 变异点:改成整除(367 / 72→72)会红。
    Check(ScaleLogicalByDpi(QuickPanelDialog::kPanelW, 144) == 359,
          "T25g: 239@144% 就近舍入 = 359(截断成 358 会红)");
    Check(ScaleLogicalByDpi(QuickPanelDialog::kPanelH, 144) == 54,
          "T25h: 36@144% = 54");
    Check(ScaleLogicalByDpi(QuickPanelDialog::kPanelW, 192) == 478,
          "T25i: 239@200% = 478");
    Check(ScaleLogicalByDpi(QuickPanelDialog::kPanelH, 192) == 72,
          "T25j: 36@200% = 72");

    // 三档 DPI 下:全部槽位都在面板内、互不重叠(命中区保持可分)。
    const int dpi_list[3] = {96, 144, 192};
    for (int k = 0; k < 3; ++k) {
      PanelPhysicalSize(QuickPanelDialog::kPanelW, QuickPanelDialog::kPanelH,
                        dpi_list[k], dpi_list[k], &w, &h);
      bool ok = true;
      int prev_right = -1;
      for (int i = 0; i < QuickPanelDialog::kButtonCount; ++i) {
        int sx = 0, sy = 0, sw = 0, sh = 0;
        if (!QuickPanelDialog::SlotRectForSize(w, h, i, &sx, &sy, &sw, &sh)) {
          ok = false;
          break;
        }
        if (sx < 0 || sy < 0 || sx + sw > w || sy + sh > h)
          ok = false;
        if (prev_right >= 0 && sx < prev_right)
          ok = false;  // 与前一个槽位重叠
        prev_right = sx + sw;
      }
      const std::string lbl = "T25k" + std::to_string(k) + ": " +
                              std::to_string(dpi_list[k]) +
                              "% 下 6 槽位都在面板内且互不重叠";
      Check(ok, lbl.c_str());
    }
  }

  // ----------------------------------------------------------------
  // T26 (spec 099 P0-4 → QuickPanel v4): 面板几何 + 状态文字映射。
  //   v4 需求⑥ 已删除右侧竖排状态回显, 状态改由槽 4/5 键帽单字显示。
  //   新几何 kPanelPadding + kBrandSize + kBrandGap
  //     + kButtonCount * kBtnSize + (kButtonCount-1) * kBtnGap + kPanelPadding
  //     = kPanelW 精确闭合 ⇒ 状态区在任意 DPI 下都推演不出矩形
  //   (StatusRectForSize 返回 false), 状态像素因此不可能压到槽位上。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-4] 面板几何与状态文字映射\n");
  {
    const int dpi_list[3] = {96, 144, 192};
    // T26a: v4 不再保留状态回显区 —— 100/150/200% 都必须推演不出矩形。
    bool status_area_gone = true;
    for (int k = 0; k < 3; ++k) {
      int w = 0, h = 0;
      fluxing::panel_dpi::PanelPhysicalSize(QuickPanelDialog::kPanelW,
                                            QuickPanelDialog::kPanelH,
                                            dpi_list[k], dpi_list[k], &w, &h);
      int sx = 0, sy = 0, sw = 0, sh = 0;
      if (QuickPanelDialog::StatusRectForSize(w, h, &sx, &sy, &sw, &sh))
        status_area_gone = false;
    }
    Check(status_area_gone, "T26a: v4 面板不再保留状态回显区(100/150/200%)");

    // T26b: 100% 下 brand + 槽位行 + 左右 padding 必须精确铺满 kPanelW,
    //       且末槽不得越界(几何单一事实源, 本测试不复制布局公式 —— R7)。
    {
      int w = 0, h = 0;
      fluxing::panel_dpi::PanelPhysicalSize(QuickPanelDialog::kPanelW,
                                            QuickPanelDialog::kPanelH,
                                            96, 96, &w, &h);
      int bx = 0, by = 0, bw = 0, bh = 0;
      const bool tail = QuickPanelDialog::SlotRectForSize(
          w, h, QuickPanelDialog::kButtonCount - 1, &bx, &by, &bw, &bh);
      const bool fits = tail && by >= 0 && bx >= 0 && bx + bw <= w &&
                        bx + bw + QuickPanelDialog::kPanelPadding == w &&
                        w == QuickPanelDialog::kPanelW;
      Check(fits, "T26b: 100% 下槽位行与左右 padding 精确铺满 kPanelW");
    }

    // T26c: 三档 DPI 下全部槽位都在面板内且互不重叠(命中区不变)。
    {
      bool slots_ok = true;
      for (int k = 0; k < 3; ++k) {
        int w = 0, h = 0;
        fluxing::panel_dpi::PanelPhysicalSize(QuickPanelDialog::kPanelW,
                                              QuickPanelDialog::kPanelH,
                                              dpi_list[k], dpi_list[k], &w, &h);
        int prev_right = -1;
        for (int i = 0; i < QuickPanelDialog::kButtonCount; ++i) {
          int bx = 0, by = 0, bw = 0, bh = 0;
          if (!QuickPanelDialog::SlotRectForSize(w, h, i, &bx, &by, &bw, &bh)) {
            slots_ok = false;
            break;
          }
          if (bx < 0 || by < 0 || bx + bw > w || by + bh > h)
            slots_ok = false;
          if (prev_right >= 0 && bx < prev_right)
            slots_ok = false;
          prev_right = bx + bw;
        }
      }
      Check(slots_ok, "T26c: 100/150/200% 下全部槽位在面板内且互不重叠");
    }

    // 文字映射(用户可见,简体中文 —— constitution P5)。
    Check(wcscmp(fluxing::panel_status::AsciiModeText(false), L"中") == 0,
          "T26e: 中文态显示「中」");
    Check(wcscmp(fluxing::panel_status::AsciiModeText(true), L"英") == 0,
          "T26f: 英文态显示「英」");
    Check(wcscmp(fluxing::panel_status::ShapeModeText(true), L"全") == 0,
          "T26g: 全角显示「全」");
    Check(wcscmp(fluxing::panel_status::ShapeModeText(false), L"半") == 0,
          "T26h: 半角显示「半」");

    // 生产回显通道:SetInputState → CurrentInputState。
    QuickPanelDialog::SetInputState(true, true);
    const fluxing::panel_status::PanelState st =
        QuickPanelDialog::CurrentInputState();
    Check(st.ascii_mode && st.fullwidth,
          "T26i: SetInputState 回读一致(英 + 全角)");
    Check(wcscmp(fluxing::panel_status::AsciiModeText(st.ascii_mode), L"英") ==
                  0 &&
              wcscmp(fluxing::panel_status::ShapeModeText(st.fullwidth), L"全") ==
                  0,
          "T26j: 状态快照映射出「英」「全」");
    QuickPanelDialog::SetInputState(false, false);
  }

  // ----------------------------------------------------------------
  // T30 (spec 100 / ADR 0038): 品牌图标右键命中区。
  //   品牌图标右键弹菜单 ⇒ 命中区必须与 HitTest 的 -2(brand)判定**逐像素**重合,
  //   否则用户右键品牌图标却弹不出菜单(肉眼极难发现)。BrandRectForSize 与
  //   生产绘制/命中走**同一份** ComputeLayout(单一事实源,R7);本测试**不复制**
  //   布局算式,只断言可判别的**不变量**:
  //     ① 返回 true;② 矩形在面板内;③ 宽=高(正方形)且非空;
  //     ④ 与槽位 0 **不相交**(否则品牌区会吃掉第一个按钮的命中区);
  //     ⑤ 左上角落在 padding(与面板 padding 对齐,容差 1 物理像素);
  //     ⑥ 非法尺寸(≤0)必须返回 false(否则运行期会算出垃圾矩形)。
  // ----------------------------------------------------------------
  std::printf("\n[spec 100 / ADR 0038] 品牌图标右键命中区\n");
  {
    const int dpi_list[3] = {96, 144, 192};
    for (int k = 0; k < 3; ++k) {
      int w = 0, h = 0;
      fluxing::panel_dpi::PanelPhysicalSize(QuickPanelDialog::kPanelW,
                                            QuickPanelDialog::kPanelH,
                                            dpi_list[k], dpi_list[k], &w, &h);
      int bx = 0, by = 0, bw = 0, bh = 0;
      const bool got =
          QuickPanelDialog::BrandRectForSize(w, h, &bx, &by, &bw, &bh);
      const std::string tag = std::to_string(dpi_list[k]) + "%";
      Check(got, ("T30a" + tag + ": BrandRectForSize 返回 true").c_str());
      Check(got && bx >= 0 && by >= 0 && bx + bw <= w && by + bh <= h,
            ("T30b" + tag + ": 品牌矩形在面板内").c_str());
      Check(got && bw == bh && bw > 0,
            ("T30c" + tag + ": 品牌矩形是正方形且非空").c_str());
      int s0x = 0, s0y = 0, s0w = 0, s0h = 0;
      const bool slot0 = QuickPanelDialog::SlotRectForSize(w, h, 0, &s0x,
                                                           &s0y, &s0w, &s0h);
      Check(got && slot0 && bx + bw <= s0x,
            ("T30d" + tag + ": 品牌矩形与槽位 0 不相交").c_str());
      const int exp_pad = fluxing::panel_dpi::ScaleLogicalByDpi(
          QuickPanelDialog::kPanelPadding, dpi_list[k]);
      const int dx = bx - exp_pad;
      const int dy = by - exp_pad;
      Check(got && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1,
            ("T30e" + tag + ": 品牌左上角落在 padding").c_str());
    }
    int bx = 0, by = 0, bw = 0, bh = 0;
    Check(!QuickPanelDialog::BrandRectForSize(0, 0, &bx, &by, &bw, &bh),
          "T30f: 非法尺寸(0×0)返回 false");
    Check(!QuickPanelDialog::BrandRectForSize(-1, -1, &bx, &by, &bw, &bh),
          "T30g: 非法尺寸(-1×-1)返回 false");
  }

  // ----------------------------------------------------------------
  // T27 (spec 099 P0-5): 键盘可达性状态机(纯逻辑)。
  //   Esc 语义调和:未交互 ⇒ 只吞掉(ADR 0019 D2);已交互 ⇒ 收起面板
  //   自身、不触碰其他窗口(D0032)。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-5] 键盘可达性状态机(纯逻辑)\n");
  {
    using namespace fluxing::panel_keyboard;
    State st;
    st.slot_count = QuickPanelDialog::kButtonCount;
    st.focused = -1;
    st.interactive = false;

    Check(HandleKey(st, vk::kReturn, false) == Action::kSwallow,
          "T27a: 无焦点槽位时 Enter 只吞掉(不误触发槽 0)");
    Check(st.focused == -1, "T27b: 无焦点槽位时 Enter 不产生焦点");

    Check(HandleKey(st, vk::kTab, false) == Action::kMoveFocus, "T27c: Tab 移焦");
    Check(st.focused == 0, "T27d: Tab 后焦点 = 槽 0");
    Check(st.interactive, "T27e: Tab 后进入键盘交互态");

    for (int i = 0; i < QuickPanelDialog::kButtonCount; ++i)
      HandleKey(st, vk::kTab, false);
    Check(st.focused == 0,
          "T27f: 连按 6 次 Tab 绕回槽 0(6 槽循环)");

    HandleKey(st, vk::kTab, false);
    Check(st.focused == 1, "T27g: Tab 前进一步 ⇒ 槽 1");
    HandleKey(st, vk::kTab, true);
    Check(st.focused == 0, "T27h: Shift+Tab 后退一步 ⇒ 槽 0");

    HandleKey(st, vk::kLeft, false);
    Check(st.focused == 5, "T27i: ← 从槽 0 回绕到槽 5");
    HandleKey(st, vk::kRight, false);
    Check(st.focused == 0, "T27j: → 从槽 3 回到槽 0");

    HandleKey(st, vk::kEnd, false);
    Check(st.focused == 5, "T27k: End ⇒ 末槽");
    HandleKey(st, vk::kHome, false);
    Check(st.focused == 0, "T27l: Home ⇒ 首槽");

    Check(HandleKey(st, vk::kReturn, false) == Action::kActivate,
          "T27m: Enter 激活焦点槽");
    Check(HandleKey(st, vk::kSpace, false) == Action::kActivate,
          "T27n: Space 激活焦点槽");

    State fresh;
    fresh.slot_count = QuickPanelDialog::kButtonCount;
    fresh.focused = -1;
    fresh.interactive = false;
    Check(HandleKey(fresh, vk::kEscape, false) == Action::kSwallow,
          "T27o: 未进入键盘交互态时 Esc 只吞掉(守 ADR 0019 D2)");
    Check(HandleKey(st, vk::kEscape, false) == Action::kClose,
          "T27p: 已进入键盘交互态时 Esc 收起面板自身(守 D0032)");

    State none;
    none.slot_count = 0;
    none.focused = -1;
    none.interactive = false;
    Check(HandleKey(none, vk::kTab, false) == Action::kSwallow,
          "T27q: 无槽位时 Tab 只吞掉");
    Check(HandleKey(none, vk::kReturn, false) == Action::kSwallow,
          "T27r: 无槽位时 Enter 只吞掉");
    Check(none.focused == -1, "T27s: 无槽位时焦点保持 -1");

    Check(HandleKey(st, 'A', false) == Action::kNone,
          "T27t: 无关按键不处理(kNone)");
  }

  // ----------------------------------------------------------------
  // T28 (spec 099 P0-5): 每槽 tooltip(名称 + 当前态)。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-5] 每槽 tooltip\n");
  {
    using namespace fluxing::panel_tooltip;
    fluxing::panel_status::PanelState st;
    st.ascii_mode = false;
    st.fullwidth = true;
    st.always_show = true;
    st.schema_name = L"小狼毫";
    st.version = L"1.1.9.0";

    Check(wcscmp(SlotName(0), L"切换到下一个输入方案") == 0,
          "T28a: 槽 0 名称 = 切换到下一个输入方案");
    Check(wcscmp(SlotName(1), L"用户短语") == 0, "T28b: 槽 1 名称 = 用户短语");
    Check(wcscmp(SlotName(2), L"快捷键设置") == 0, "T28c: 槽 2 名称 = 快捷键设置");
    Check(wcscmp(SlotName(3), L"关于 / 账号说明") == 0,
          "T28d: 槽 3 名称 = 关于 / 账号说明");
    // v4 stage2: 槽 4/5 = 中英文切换 / 全半角切换;槽 6 才是越界。
    Check(wcscmp(SlotName(4), L"中英文切换") == 0,
          "T28e: 槽 4 名称 = 中英文切换");
    Check(wcscmp(SlotName(5), L"全半角切换") == 0,
          "T28e2: 槽 5 名称 = 全半角切换");
    Check(SlotName(6)[0] == L'\0', "T28e3: 越界槽名 = 空串");

    Check(BuildTooltip(0, st) == L"切换到下一个输入方案 · 当前:小狼毫",
          "T28f: 槽 0 tooltip = 名称 + 当前方案名");
    // v4 stage2: 槽 1 语义改为「用户短语」⇒ 无当前态(中英态迁到槽 4)。
    Check(BuildTooltip(1, st) == L"用户短语",
          "T28g: 槽 1 tooltip = 仅名称(无当前态)");
    Check(BuildTooltip(2, st) == L"快捷键设置 · 常驻显示:开",
          "T28h: 槽 2 tooltip = 名称 + 常驻显示态");
    Check(BuildTooltip(3, st) == L"关于 / 账号说明 · 版本:1.1.9.0",
          "T28i: 槽 3 tooltip = 名称 + 版本号");
    // v4 stage2: 槽 4/5 tooltip 承载中英 / 全半角当前态(需求⑤)。
    Check(BuildTooltip(4, st) == L"中英文切换 · 当前:中",
          "T28i2: 槽 4 tooltip = 中英文切换 + 当前态");
    Check(BuildTooltip(5, st) == L"全半角切换 · 当前:全",
          "T28i3: 槽 5 tooltip = 全半角切换 + 当前态");
    Check(BuildTooltip(9, st).empty(), "T28j: 越界槽 tooltip = 空串");

    fluxing::panel_status::PanelState en = st;
    en.ascii_mode = true;
    Check(BuildTooltip(4, en) == L"中英文切换 · 当前:英",
          "T28k: 英文态 tooltip 显示「英」");

    fluxing::panel_status::PanelState half = st;
    half.fullwidth = false;
    Check(BuildTooltip(5, half) == L"全半角切换 · 当前:半",
          "T28k2: 半角态 tooltip 显示「半」");

    fluxing::panel_status::PanelState unk = st;
    unk.schema_name.clear();
    Check(BuildTooltip(0, unk) == L"切换到下一个输入方案 · 当前:未知",
          "T28l: 缺方案名 ⇒ 显示「未知」");

    // 生产通道:TooltipText 必须与纯逻辑同源。
    QuickPanelDialog::SetInputState(false, false);
    // v4 stage2: 槽 1 无当前态 ⇒ tooltip 仅名称。
    Check(QuickPanelDialog::TooltipText(1) == L"用户短语",
          "T28m: 生产 TooltipText(1) 与纯逻辑一致");
    const std::wstring expect2 =
        std::wstring(L"快捷键设置 · 常驻显示:") +
        (QuickPanelDialog::AlwaysShowMode() ? L"开" : L"关");
    Check(QuickPanelDialog::TooltipText(2) == expect2,
          "T28n: 生产 TooltipText(2) 跟随唯一真相源 AlwaysShowMode()");
    Check(QuickPanelDialog::TooltipText(0) == L"切换到下一个输入方案 · 当前:未知",
          "T28o: 无 provider 时槽 0 tooltip 兜底「未知」");
    // v4 stage2: 生产通道的槽 4/5 也必须回显当前态(需求⑤)。
    Check(QuickPanelDialog::TooltipText(4) == L"中英文切换 · 当前:中",
          "T28o2: 生产 TooltipText(4) 回显中英态");
    Check(QuickPanelDialog::TooltipText(5) == L"全半角切换 · 当前:半",
          "T28o3: 生产 TooltipText(5) 回显全半角态");
  }

  // ----------------------------------------------------------------
  // T29 (spec 099 P0-5): 生产级键盘激活(端到端,走真实 WndProc)。
  // ----------------------------------------------------------------
  std::printf("\n[spec 099 P0-5] 生产级键盘激活(端到端)\n");
  {
    bool schema_fired = false;
    QuickPanelDialog::Show(false, [&]() { schema_fired = true; }, []() {},
                           []() {}, [](bool) {}, []() {}, []() {});
    HWND panel = QuickPanelDialog::ActiveHwnd();
    Check(panel != NULL, "T29a: 面板已 Show()");
    Check(QuickPanelDialog::FocusedSlot() == -1,
          "T29b: Show() 后无焦点槽位(不预选槽位)");
    Check(!QuickPanelDialog::KeyboardInteractive(),
          "T29c: Show() 后未进入键盘交互态");

    QuickPanelDialog::WndProc(panel, WM_KEYDOWN, VK_TAB, 0);
    Check(QuickPanelDialog::FocusedSlot() == 0,
          "T29d: 生产路径 Tab ⇒ 焦点槽 0");
    Check(QuickPanelDialog::KeyboardInteractive(),
          "T29e: 生产路径 Tab ⇒ 进入键盘交互态");

    QuickPanelDialog::WndProc(panel, WM_KEYDOWN, VK_RETURN, 0);
    Check(schema_fired, "T29f: 生产路径 Enter 激活焦点槽(槽 0 回调触发)");

    // Esc:已交互态 ⇒ 收起面板自身,不触碰任何其他窗口(守 D0032)。
    QuickPanelDialog::WndProc(panel, WM_KEYDOWN, VK_ESCAPE, 0);
    Check(!IsWindowVisible(panel), "T29g: 已交互态 Esc 收起面板自身(D0032)");
    QuickPanelDialog::Hide();
  }

  // ----------------------------------------------------------------
  // T31 (需求⑦): 已发布品牌素材不得含近黑描边(keyline ⇒ 标志上的黑框)。
  // 判据 luma<=64 的依据: 旧素材 36 个描边像素 luma 43~55; 品牌红主体
  // (175,49,35) luma=85; 白底 255; 面板图标灰 (130,130,140) luma=131。
  // ----------------------------------------------------------------
  std::printf("\n[需求⑦] 品牌素材无近黑描边\n");
  {
    // 判据自检: 近黑命中 / 品牌红不命中 —— 防止这条检查变成「没牙」的恒真。
    Check(LogoLuma(152, 0, 0) <= 64 && LogoLuma(175, 49, 35) > 64,
          "T31a: 近黑判据有牙(152,0,0 命中 / 品牌红 175,49,35 不命中)");
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const std::wstring asset = FindLogoAsset();
    Check(!asset.empty(),
          "T31b: 找到已发布素材 output/fluxing-logo_small.png");
    if (!asset.empty()) {
      std::vector<BYTE> buf;
      UINT w = 0, h = 0;
      const bool ok = DecodePngBgra(asset, buf, w, h);
      Check(ok, "T31c: WIC 成功解码已发布品牌素材");
      if (ok) {
        int dark = 0;
        for (UINT y = 0; y < h; ++y) {
          for (UINT x = 0; x < w; ++x) {
            const BYTE* px = &buf[((size_t)y * w + x) * 4];  // BGRA
            if (px[3] < 200)
              continue;  // 半透明抗锯齿像素不算描边
            if (LogoLuma(px[2], px[1], px[0]) <= 64)
              ++dark;
          }
        }
        Check(dark == 0,
              "T31d: 已发布素材无近黑描边像素(需求⑦ 黑框根因已消除)");
      }
    }
  }

  // ----------------------------------------------------------------
  // T32 (需求⑦ 二次根因): logo 位图必须先预合成到面板底色再贴图。
  // 素材层(T31)只是第一道护栏; 真正的黑框来自 GDI alpha 合成路径, 这里
  // 守住「预合成」这个动作: 合成后 alpha 恒 255, 且无任何近黑像素。
  // ----------------------------------------------------------------
  std::printf("\n[需求⑦] logo 预合成到面板底色\n");
  {
    // T32a: 判据有牙 —— α=0 且 RGB=(0,0,0) 的像素, 合成后必须是**底色**
    // 而不是黑色(任何漏掉全透明分支的实现都会在这里产出黑块)。
    uint8_t probe[4] = {0, 0, 0, 0};  // BGRA
    fluxing::panel_logo::CompositeOpaqueOverBg(probe, 1, 1, 200, 225, 250);
    Check(probe[3] == 255 && probe[0] == 250 && probe[1] == 225 &&
              probe[2] == 200,
          "T32a: α=0 像素合成后是底色而非黑色(判据有牙)");

    // T32b: 半透明像素合成后 alpha 恒 255, 且亮度介于源色与底色之间。
    // (源 (0,0,0) α=128 合成到 kBgTop(200,225,250) ⇒ R≈100 G≈112 B≈125)
    uint8_t half[4] = {0, 0, 0, 128};
    fluxing::panel_logo::CompositeOpaqueOverBg(half, 1, 1, 200, 225, 250);
    Check(half[3] == 255 && half[2] >= 95 && half[2] <= 105 &&
              half[1] >= 107 && half[1] <= 117 && half[0] >= 120 &&
              half[0] <= 130,
          "T32b: α=128 像素合成后 alpha=255 且亮度介于源色与底色之间");

    // T32c: 真实素材走一遍生产同款解码 + 合成路径。
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const std::wstring asset = FindLogoAsset();
    Check(!asset.empty(),
          "T32c: 找到已发布素材 output/fluxing-logo_small.png");
    if (!asset.empty()) {
      std::vector<BYTE> buf;
      UINT w = 0, h = 0;
      const bool ok = DecodePngBgra(asset, buf, w, h);
      Check(ok, "T32d: WIC 成功解码已发布品牌素材");
      if (ok) {
        fluxing::panel_logo::CompositeOpaqueOverBg(buf.data(), (int)w, (int)h,
                                                    200, 225, 250);
        int dark = 0;
        int nonOpaque = 0;
        for (size_t i = 0; i < (size_t)w * h; ++i) {
          const BYTE* px = &buf[i * 4];  // BGRA
          if (px[3] != 255)
            ++nonOpaque;
          if (fluxing::panel_logo::IsNearBlack(px, 70))
            ++dark;
        }
        Check(nonOpaque == 0, "T32e: 预合成后 alpha 恒 255(不透明位图)");
        Check(dark == 0, "T32f: 预合成后无近黑像素(需求⑦ 黑框已消除)");
      }
    }
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
