//
// ModalChrome.cpp — 共用 modal chrome paint helper 实现
//
#include "stdafx.h"
#include "ModalChrome.h"

namespace ModalChrome {

void PaintBackground(HDC hdc, int w, int h,
                     COLORREF bgTop, COLORREF bgBot) {
  if (!hdc || w <= 0 || h <= 0) return;
  RECT rc = {0, 0, w, h};
  TRIVERTEX v[2] = {};
  v[0].x = 0; v[0].y = 0;
  v[0].Red   = static_cast<COLOR16>(GetRValue(bgTop)) << 8;
  v[0].Green = static_cast<COLOR16>(GetGValue(bgTop)) << 8;
  v[0].Blue  = static_cast<COLOR16>(GetBValue(bgTop)) << 8;
  v[0].Alpha = 0xFF00;
  v[1].x = rc.right; v[1].y = rc.bottom;
  v[1].Red   = static_cast<COLOR16>(GetRValue(bgBot)) << 8;
  v[1].Green = static_cast<COLOR16>(GetGValue(bgBot)) << 8;
  v[1].Blue  = static_cast<COLOR16>(GetBValue(bgBot)) << 8;
  v[1].Alpha = 0xFF00;
  GRADIENT_RECT g = {0, 1};
  if (!GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_V)) {
    HBRUSH bg = CreateSolidBrush(bgTop);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
  }
}

void PaintBorder(HDC hdc, int w, int h, int radius, COLORREF borderColor) {
  if (!hdc || w <= 0 || h <= 0 || radius <= 0) return;
  HPEN hPen = CreatePen(PS_SOLID, 1, borderColor);
  HPEN hOld = static_cast<HPEN>(SelectObject(hdc, hPen));
  HBRUSH hOldBr = static_cast<HBRUSH>(
      SelectObject(hdc, GetStockObject(NULL_BRUSH)));
  RoundRect(hdc, 0, 0, w - 1, h - 1, radius * 2, radius * 2);
  SelectObject(hdc, hOld);
  SelectObject(hdc, hOldBr);
  DeleteObject(hPen);
}

void PaintBackgroundAndBorder(HDC hdc, int w, int h,
                              COLORREF bgTop, COLORREF bgBot,
                              int radius, COLORREF borderColor) {
  PaintBackground(hdc, w, h, bgTop, bgBot);
  PaintBorder(hdc, w, h, radius, borderColor);
}

}  // namespace ModalChrome
