#pragma once
// include/FluxingCandidateHitTest.h
//
// spec 100 / lessons L186: 「右键点候选删不掉」的根因是 OnRButtonDown 用
// m_hoverIndex 当唯一入口, 而 m_hoverIndex 只在 hover_type != NONE 时才会被
// OnMouseMove 赋值 —— 出厂默认 weasel.yaml 是 `hover_type: none`
// (include/WeaselIPCData.h 里 UIStyle 的默认值同样是 NONE), 所以默认配置下
// 右键永远 m_hoverIndex == -1, 直接静默 return, 连 IPC 都不发。
//
// 修法不能再依赖 hover: 右键必须按**点击坐标**自己算命中索引, 且命中口径要和
// OnMouseMove 逐字一致(GetCandidateRect → reposition 偏移 → hilite padding
// 扩张 → PtInRect), 否则「看到的」和「点得到的」会差一个 padding。
//
// 这段几何被抽成纯函数放在 include/ 下, 有两个原因:
//   1. WeaselPanel.cpp 拉进 WTL/ATL/Gdiplus, test/TestCandidateRButtonDown
//      链不动它(见该文件头注释), 旧测试只能"手抄一份 FakePanel 逻辑",
//      于是生产代码怎么改它都全绿 —— 这正是 L186 能潜伏 4 轮的原因。
//   2. 抽成纯函数后, 测试 include 真的这一份头文件断言, 咬得到生产逻辑。
//
// 只依赖 <windows.h> 的几何原语(OffsetRect / InflateRect / PtInRect),
// 不碰 ATL, 不碰 IPC 线格式(ADR 0025: 命令号 / wParam / lParam 零变化)。

#include <windows.h>

#include <cstddef>
#include <vector>

namespace fluxing {

// 一个候选在面板客户区里的基础矩形 + reposition(退格选字)时的纵向偏移。
// `rect` 由调用方原样填入 `CandidateLayout::GetCandidateRect(i)` 的返回值,
// `offsetY` 填 `m_offsetys[i]`; istorepos 为 false 时 offsetY 不参与计算。
struct CandidateHitRect {
  RECT rect = {0, 0, 0, 0};
  int offsetY = 0;
};

// 命中测试: 返回第一个包含 pt 的候选索引, 全不命中返回 -1。
//
// padX / padY 传 **已经 DPI 换算过** 的 hilite_padding_x / hilite_padding_y
// (与 OnMouseMove 里 DPI_SCALE(...) 的结果同源), 本函数不做任何缩放,
// 保证纯函数单测里的数字就是纸面几何。
//
// 边界语义与 ::PtInRect 完全一致: [left, right) × [top, bottom)。
inline int HitTestCandidateIndex(const std::vector<CandidateHitRect>& items,
                                 POINT pt,
                                 bool istorepos,
                                 int padX,
                                 int padY) {
  for (std::size_t i = 0; i < items.size(); ++i) {
    RECT rc = items[i].rect;
    if (istorepos) {
      OffsetRect(&rc, 0, items[i].offsetY);
    }
    InflateRect(&rc, padX, padY);
    if (PtInRect(&rc, pt)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace fluxing
