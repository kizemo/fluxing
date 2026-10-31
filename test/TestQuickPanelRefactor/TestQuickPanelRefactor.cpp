// TestQuickPanelRefactor.cpp - spec 092 (2026-10-31)
//
// 快捷面板「窗口结构契约」的行为级测试。
//
// ===== 为什么必须重写（spec 092 · 缺陷 B）============================
// 本文件是 spec 038 (2026-07-05) 的产物,断言的是「WM_CREATE 造出 4 个
// Fluxing 子控件(Label/Panel/Toggle/Button)+ 1 个 native close」,子控件
// 总数 == 5。事实链:
//   1. spec 045 把 Fluxing 控件创建从 OnCreate 挪到 Show()(需要真实消息泵);
//   2. 随后 v3-rev3 又把整个面板从「WTL/D2D 子控件体系」整体重写成
//      **单窗口 GDI 自绘**(WeaselServer/QuickPanelDialog.h 头注释明写)。
// 于是「5 个子控件」这个前提从两代之前就不成立了,而测试从未跟进:
// v0.18.29.0 起整段被 `return 0` 短路成 SKIP —— 一份「永远绿、永远不测」
// 的测试,CHANGELOG 里却写着 "TestQuickPanelRefactor 9/9"。
//
// ===== 现在断言什么（现行生产契约,WeaselServer/QuickPanelDialog.cpp v3-rev3）====
//   - Show() 只建**一个** WS_POPUP 顶层窗口,类名 L"FluxingQuickPanel_v3",
//     类风格 CS_OWNDC;
//   - 扩展样式 WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TOPMOST;
//   - 几何 kPanelW x kPanelH = 245 x 48(DPI 由 OnCreate 换算成 *_phys 常量);
//   - 面板**没有任何子窗口**(logo + 4 个按钮全是 GDI 自绘);
//   - Hide() 只 KillTimer + SW_HIDE,**不销毁**窗口(供下次复用);
//   - Show() 第二次走复用路径,**同一个 HWND**;
//   - ToggleMode() 可见 => 收起 / 不可见 => 展开;
//   - HitTest() 按物理像素命中 4 个按钮,brand 区返回 -2,空白/越界返回 -1;
//   - 现行按钮路由:hit==1 -> s_onPhrases,hit==2 -> s_onShortcut,0/3 no-op。
//
// L24 link-probe pattern:链接真正的生产代码(见 .vcxproj 的 ClCompile 条目),
// 不写镜像实现。本文件断言的是生产契约,不是自己的副本。

#include "stdafx.h"

#include "QuickPanelDialog.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <string>

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* name) {
  if (cond) {
    ++g_pass;
    std::printf("  PASS: %s\n", name);
  } else {
    ++g_fail;
    std::printf("  FAIL: %s\n", name);
  }
}

// 面板窗口类名 —— 生产代码 QuickPanelDialog.cpp:37 的 kWindowClassName。
// 这里刻意**不**从生产代码取(它是 .cpp 里的 file-static 常量),而是把字面量
// 写死在测试里:这样「生产改了类名而测试没改」会被立刻发现,而不是两边
// 一起漂移成永远绿。
const wchar_t kExpectedWindowClass[] = L"FluxingQuickPanel_v3";

// EnumChildWindows 是递归的 —— 返回的是**全部后代**数量。
int CountDescendants(HWND parent) {
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

std::wstring ClassNameOf(HWND hwnd) {
  wchar_t cls[128] = {0};
  GetClassNameW(hwnd, cls, 128);
  return std::wstring(cls);
}

// 按钮布局 —— 镜像生产 HitTest() 的布局公式
// (WeaselServer/QuickPanelDialog.cpp:460-485)。直接用生产暴露出来的
// *_phys 常量算,不重新发明一套几何。
struct ButtonLayout {
  int start_x;
  int step;
  int y0;
  int size;
  int center_x(int index) const { return start_x + index * step + size / 2; }
  int center_y() const { return y0 + size / 2; }
};

ButtonLayout CurrentLayout() {
  // stdafx.h 未定义 NOMINMAX ⇒ windows.h 的 max/min 宏会污染 std::max
  // (展开成 std::((a)>(b)?(a):(b)) ⇒ 编译错误 C2589)。故此处用三元表达式。
  const int dpr_gap = (int)(2 * QuickPanelDialog::s_dpr_x + 0.5f);  // 与生产同式
  const int brand_gap = dpr_gap > 2 ? dpr_gap : 2;
  ButtonLayout lay;
  lay.start_x = QuickPanelDialog::s_panelPadding_phys +
                QuickPanelDialog::s_brandSize_phys + brand_gap;
  lay.step = QuickPanelDialog::s_btnSize_phys + QuickPanelDialog::s_btnGap_phys;
  lay.y0 = QuickPanelDialog::s_btnYOffset_phys;
  lay.size = QuickPanelDialog::s_btnSize_phys;
  return lay;
}

}  // namespace

namespace fluxing_test {

int RunRefactorTest() {
  // ----------------------------------------------------------------
  // T0: 前置 —— 还没 Show 过,面板 HWND 必须是 NULL。
  //     (s_hwnd 只在 Show()/OnCreate 里被赋值,Hide() 不清空它。)
  // ----------------------------------------------------------------
  Check(QuickPanelDialog::ActiveHwnd() == nullptr,
        "T0: ActiveHwnd() == NULL (Show 之前没有活动面板)");

  // ----------------------------------------------------------------
  // T1: Show() 真的建出了一个窗口。
  // ----------------------------------------------------------------
  QuickPanelDialog::Show(false, nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr);
  HWND panel = QuickPanelDialog::ActiveHwnd();
  Check(panel != nullptr && IsWindow(panel),
        "T1: Show() 建立了面板窗口");

  if (panel == nullptr || !IsWindow(panel)) {
    std::printf("\nTestQuickPanelRefactor: %d/%d assertions passed\n", g_pass,
                g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
  }

  // ----------------------------------------------------------------
  // T2: 窗口类名 —— 生产契约锚点。
  // ----------------------------------------------------------------
  Check(ClassNameOf(panel) == kExpectedWindowClass,
        "T2: 窗口类名 == FluxingQuickPanel_v3");

  // ----------------------------------------------------------------
  // T3: 窗口样式 —— 顶层弹窗,不是子窗口。
  // ----------------------------------------------------------------
  // Windows SDK 只有 GWL_STYLE/GWL_EXSTYLE(-16/-20)，没有 GWLP_STYLE/GWLP_EXSTYLE。
  const LONG_PTR style = GetWindowLongPtrW(panel, GWL_STYLE);
  Check((style & WS_POPUP) != 0 && (style & WS_CHILD) == 0,
        "T3: 窗口样式 = WS_POPUP 顶层弹窗(无 WS_CHILD)");

  // ----------------------------------------------------------------
  // T4: 扩展样式 —— 四个扩展样式各自承担一条生产不变量。
  //     WS_EX_LAYERED  = per-pixel alpha(liquid glass)的前提
  //     WS_EX_NOACTIVATE = 调出面板不抢输入焦点
  //     WS_EX_TOOLWINDOW = 不进任务栏/Alt-Tab
  //     WS_EX_TOPMOST  = 常驻置顶
  // ----------------------------------------------------------------
  const LONG_PTR ex = GetWindowLongPtrW(panel, GWL_EXSTYLE);
  Check((ex & WS_EX_LAYERED) != 0, "T4a: WS_EX_LAYERED(per-pixel alpha 前提)");
  Check((ex & WS_EX_NOACTIVATE) != 0, "T4b: WS_EX_NOACTIVATE(不抢焦点)");
  Check((ex & WS_EX_TOOLWINDOW) != 0, "T4c: WS_EX_TOOLWINDOW(不进任务栏)");
  Check((ex & WS_EX_TOPMOST) != 0, "T4d: WS_EX_TOPMOST(常驻置顶)");

  // ----------------------------------------------------------------
  // T5: 几何 —— client rect 与 *_phys 常量必须同步。
  //     不断言绝对像素值(那会随显示器 DPI 变),只断言**同步关系**:
  //     OnCreate 把 client rect 抄进 s_panelW_phys/s_panelH_phys,
  //     dpr 由这两个常量除以设计常量得出。
  // ----------------------------------------------------------------
  RECT client;
  GetClientRect(panel, &client);
  const int cw = client.right - client.left;
  const int ch = client.bottom - client.top;
  Check(cw > 0 && ch > 0, "T5a: 面板 client rect 非空");
  Check(QuickPanelDialog::s_panelW_phys == cw &&
            QuickPanelDialog::s_panelH_phys == ch,
        "T5b: s_panelW_phys/s_panelH_phys 与实际 client rect 一致");
  Check(QuickPanelDialog::s_dpr_x == (float)QuickPanelDialog::s_panelW_phys /
                                         (float)QuickPanelDialog::kPanelW &&
            QuickPanelDialog::s_dpr_y == (float)QuickPanelDialog::s_panelH_phys /
                                         (float)QuickPanelDialog::kPanelH,
        "T5c: dpr = 物理尺寸 / 设计常量");

  // ----------------------------------------------------------------
  // T6: 面板没有子窗口 —— v3-rev3 是单窗口自绘。
  //     这条是 spec 038 老断言「5 个子控件」的**反面**:事实是 0 个。
  // ----------------------------------------------------------------
  Check(CountDescendants(panel) == 0,
        "T6: 面板 0 个子窗口(logo + 4 按钮全是 GDI 自绘)");

  // ----------------------------------------------------------------
  // T7: Hide() 只隐藏,不销毁(复用契约)。
  // ----------------------------------------------------------------
  QuickPanelDialog::Hide();
  Check(IsWindow(panel), "T7a: Hide() 后窗口仍存在(不销毁,供复用)");
  Check(!IsWindowVisible(panel), "T7b: Hide() 后窗口不可见");
  Check(QuickPanelDialog::ActiveHwnd() == panel,
        "T7c: Hide() 不清空 ActiveHwnd()(复用契约)");

  // ----------------------------------------------------------------
  // T8: Show() 第二次走复用路径 —— 同一个 HWND,不重建。
  // ----------------------------------------------------------------
  QuickPanelDialog::Show(false, nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr);
  Check(QuickPanelDialog::ActiveHwnd() == panel,
        "T8a: 二次 Show() 复用同一个 HWND(不重建窗口)");
  Check(IsWindowVisible(panel), "T8b: 二次 Show() 后窗口可见");

  // ----------------------------------------------------------------
  // T9: ToggleMode() —— 可见 => 收起 / 收起 => 展开,始终同一个 HWND。
  // ----------------------------------------------------------------
  QuickPanelDialog::ToggleMode();
  Check(!IsWindowVisible(panel), "T9a: 面板可见时 ToggleMode() 收起");
  QuickPanelDialog::ToggleMode();
  Check(IsWindowVisible(panel), "T9b: 面板收起时 ToggleMode() 展开");
  Check(QuickPanelDialog::ActiveHwnd() == panel,
        "T9c: ToggleMode() 始终复用同一个 HWND");

  // ----------------------------------------------------------------
  // T10: HitTest() 命中契约 —— 4 个按钮 + brand + 空白 + 越界。
  //      按钮中心按生产布局公式算出,因此这条同时钉住了布局与命中的一致性。
  // ----------------------------------------------------------------
  const ButtonLayout lay = CurrentLayout();
  Check(QuickPanelDialog::kButtonCount == 4,
        "T10a: kButtonCount == 4 (v0.19.0.62 UserDict 槽位删除)");
  bool all_buttons_hit = true;
  for (int i = 0; i < QuickPanelDialog::kButtonCount; ++i) {
    if (QuickPanelDialog::HitTest(lay.center_x(i), lay.center_y()) != i)
      all_buttons_hit = false;
  }
  Check(all_buttons_hit, "T10b: 4 个按钮中心各自命中自己的索引");
  const int brand_cx = QuickPanelDialog::s_panelPadding_phys +
                       QuickPanelDialog::s_brandSize_phys / 2;
  const int brand_cy = QuickPanelDialog::s_panelPadding_phys +
                       QuickPanelDialog::s_brandSize_phys / 2;
  Check(QuickPanelDialog::HitTest(brand_cx, brand_cy) == -2,
        "T10c: brand 区命中返回 -2");
  Check(QuickPanelDialog::HitTest(
            lay.center_x(QuickPanelDialog::kButtonCount), lay.center_y()) == -1,
        "T10d: 按钮区右侧空白命中返回 -1");
  Check(QuickPanelDialog::HitTest(-1, -1) == -1 &&
            QuickPanelDialog::HitTest(QuickPanelDialog::s_panelW_phys, 0) == -1,
        "T10e: 越界坐标命中返回 -1");

  // ----------------------------------------------------------------
  // T11: Hide() 把 hover / active 索引复位为 -1。
  //      (不复位会让上一次 hover 的橙色残留在下一轮绘制里。)
  // ----------------------------------------------------------------
  QuickPanelDialog::Hide();
  Check(QuickPanelDialog::s_hoveredIdx == -1 &&
            QuickPanelDialog::s_activeIdx == -1,
        "T11: Hide() 把 hover/active 索引复位为 -1");

  // ----------------------------------------------------------------
  // T12: 按钮点击路由 —— 现行契约 hit==1 -> s_onPhrases、
  //      hit==2 -> s_onShortcut、hit 0/3 是历史 placeholder(no-op)。
  //      模拟方式与 test/v0_19_0_32_e2e 相同:直接投递
  //      WM_LBUTTONDOWN / WM_LBUTTONUP 到面板窗口。
  //      生产在 LBUTTONDOWN 里无条件 s_dragging=TRUE(L88-fix),
  //      LBUTTONUP 的 drag 分支用「指针实际位移 < 4px」判 click,
  //      两次 GetCursorPos 之间指针不动 => 判定为 click => 触发回调。
  // ----------------------------------------------------------------
  {
    int phrase_count = 0;
    int shortcut_count = 0;
    QuickPanelDialog::SetOnShortcut([&shortcut_count]() { ++shortcut_count; });
    QuickPanelDialog::Show(false, nullptr, nullptr,
                           [&phrase_count]() { ++phrase_count; }, nullptr,
                           nullptr, nullptr);
    HWND qp = QuickPanelDialog::ActiveHwnd();
    const ButtonLayout l2 = CurrentLayout();

    LPARAM lp = MAKELPARAM(l2.center_x(1), l2.center_y());
    SendMessageW(qp, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    SendMessageW(qp, WM_LBUTTONUP, 0, lp);
    Check(phrase_count == 1, "T12a: 点按钮 1 -> s_onPhrases 触发 1 次");

    lp = MAKELPARAM(l2.center_x(2), l2.center_y());
    SendMessageW(qp, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    SendMessageW(qp, WM_LBUTTONUP, 0, lp);
    Check(shortcut_count == 1, "T12b: 点按钮 2 -> s_onShortcut 触发 1 次");

    lp = MAKELPARAM(l2.center_x(0), l2.center_y());
    SendMessageW(qp, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    SendMessageW(qp, WM_LBUTTONUP, 0, lp);
    lp = MAKELPARAM(l2.center_x(3), l2.center_y());
    SendMessageW(qp, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    SendMessageW(qp, WM_LBUTTONUP, 0, lp);
    Check(phrase_count == 1 && shortcut_count == 1,
          "T12c: 点按钮 0/3 不触发任何回调(历史 placeholder,no-op)");

    QuickPanelDialog::SetOnShortcut(nullptr);
  }

  // ----------------------------------------------------------------
  // T13: 残留契约 —— Show() 当前**忽略** currentFullwidth。
  //      事实:s_fullwidth 全仓只在静态初始化处赋值,Show() 里明写
  //      `(void)currentFullwidth;`,4 按钮布局里也没有全角/ASCII 按钮
  //      (s_onFullwidth 没有任何调用点)。所以 IsFullwidth() 恒为 false。
  //      这里把现状钉住:将来若恢复该能力,本断言会红,届时请同步更新。
  // ----------------------------------------------------------------
  QuickPanelDialog::Show(true, nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr);
  Check(!QuickPanelDialog::IsFullwidth(),
        "T13: 残留契约 —— Show(currentFullwidth=true) 后 IsFullwidth() 仍 false");

  // ----------------------------------------------------------------
  // T14: Show() 保存全部 6 个回调;传 nullptr 则清空。
  // ----------------------------------------------------------------
  QuickPanelDialog::Show(false, []() {}, []() {}, []() {}, [](bool) {}, []() {},
                         []() {});
  Check(QuickPanelDialog::s_onSchema && QuickPanelDialog::s_onUserFolder &&
            QuickPanelDialog::s_onPhrases && QuickPanelDialog::s_onFullwidth &&
            QuickPanelDialog::s_onSymbols && QuickPanelDialog::s_onLogin,
        "T14a: Show() 保存了全部 6 个回调");
  QuickPanelDialog::Show(false, nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr);
  Check(QuickPanelDialog::s_onSchema == nullptr &&
            QuickPanelDialog::s_onUserFolder == nullptr &&
            QuickPanelDialog::s_onPhrases == nullptr &&
            QuickPanelDialog::s_onFullwidth == nullptr &&
            QuickPanelDialog::s_onSymbols == nullptr &&
            QuickPanelDialog::s_onLogin == nullptr,
        "T14b: Show(nullptr,...) 把 6 个回调全部清空");

  // 收尾:收起面板,避免残留一个置顶窗口影响后续用例。
  QuickPanelDialog::Hide();

  std::printf("\nTestQuickPanelRefactor: %d/%d assertions passed\n", g_pass,
              g_pass + g_fail);
  return g_fail == 0 ? 0 : 1;
}

}  // namespace fluxing_test