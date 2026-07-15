//
// ModalChrome.h — 共用 modal chrome paint helper (v0.19.0.31, spec 070 L95-A)
//
// 抽取自 PhrasesDialog / UserDictionary / ShortcutSettings 三处
// WS_POPUP + WS_EX_LAYERED modal 的统一 chrome paint 路径。
// 解决 v0.19.0.30 ship bug: 三处 OnPaint 只画 title bar (30-56px),
// 不画 body → 用户看到空 body / 无 chrome。
//
// 修正:画**整个 client** 渐变 (kBgTop→kBgBot), 然后 chrome elements
// (border / title text / X) 覆盖其上。
//
#pragma once

#include <windows.h>

namespace ModalChrome {

// 画整个 client area 的渐变 bg (kBgTop → kBgBot, 全 (0,0)-(w,h))。
// Chrome elements (border / title text / X) 在调用方接着画。
// 用 BeginPaint 出来的 hdc 即可。失败 (GradientFill 退化) 退化为
// solid brush fill kBgTop 保证最少有 body bg。
void PaintBackground(HDC hdc, int w, int h,
                     COLORREF bgTop, COLORREF bgBot);

// 画 rounded 圆角 hairline border (1px, kBorderColor)。
// 通常 PaintBackground 之后调用。
void PaintBorder(HDC hdc, int w, int h, int radius, COLORREF borderColor);

// 一次性 BG + Border 复合 helper (dr pattern)。
// 给 PhrasesDialog / UserDictionary / ShortcutSettings OnPaint 复用。
void PaintBackgroundAndBorder(HDC hdc, int w, int h,
                              COLORREF bgTop, COLORREF bgBot,
                              int radius, COLORREF borderColor);

}  // namespace ModalChrome
