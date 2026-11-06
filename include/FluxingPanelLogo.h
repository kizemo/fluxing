// FluxingPanelLogo.h —— 品牌标志位图的纯函数合成单元(header-only,不依赖 windows.h)。
//
// 背景(QuickPanel v4 需求⑦ 的**二次根因**):
//   运行时面板渲染落盘取证(qp-dump.bmp / qp-logo.bmp)证明,品牌标志位图本身
//   **不含任何不透明近黑像素**(opaque near-black == 0),但面板上仍然出现 203 个
//   近黑像素,位置恰好等于 logo 的**半透明边缘列**,且像素值 ≈ 255 − 源 alpha。
//   即:GDI AlphaBlend 这条 alpha 合成路径在本面板上把半透明源像素画成了近黑。
//   结论:病灶在**合成路径**,不在素材层。
//
// 处置(单一真相源):
//   1) WIC 解码出 straight-alpha BGRA 后,先用本文件的纯函数把 logo **预合成到
//      面板底色**(kBgTop)上,得到一张 **alpha 恒为 255 的不透明位图**;
//   2) 绘制阶段改用 BitBlt(SRCCOPY) 1:1 贴图,**彻底不经过 GDI alpha 合成**。
//
// 为什么必须预合成而不是直接 BitBlt: GDI 的 SRCCOPY 不看 alpha 通道,源里
// alpha=0 的像素会按其 RGB 原样贴上去(历史上造成「黑底 + 红 logo」)。
// 预合成之后每个像素的 RGB 已经是「与底色融合后的最终色」,alpha 恒 255,
// SRCCOPY 的行为就变得完全确定。
//
// 单一事实源铁律(R7):生产代码与单测共用本文件,测试不得复制合成公式。
#ifndef FLUXING_PANEL_LOGO_H_
#define FLUXING_PANEL_LOGO_H_

#include <stdint.h>
#include <stddef.h>

namespace fluxing {
namespace panel_logo {

// 把 straight-alpha 的 32bpp BGRA 像素按 src-over 合成到底色,原地改成不透明。
//
// bgra : 指向 w*h 个 BGRA 像素(每像素 4 字节,顺序 B,G,R,A)
// bgR/bgG/bgB : 目标底色(面板底色 kBgTop)
// 语义:out = src.rgb * a + bg * (1 - a),out.a = 255
//   - a == 255 → 原样保留 RGB,alpha 置 255
//   - a == 0   → 输出**底色**(而不是黑色),这是本函数「有牙」的关键判据:
//                任何忘记处理全透明像素的实现都会在这里产出黑块。
inline void CompositeOpaqueOverBg(uint8_t* bgra, int w, int h,
                                   uint8_t bgR, uint8_t bgG, uint8_t bgB) {
  if (!bgra || w <= 0 || h <= 0)
    return;
  const int total = w * h;
  for (int i = 0; i < total; ++i) {
    uint8_t* px = bgra + static_cast<size_t>(i) * 4;
    const uint32_t a = px[3];
    if (a == 255) {
      px[3] = 255;
      continue;
    }
    if (a == 0) {
      px[0] = bgB;
      px[1] = bgG;
      px[2] = bgR;
      px[3] = 255;
      continue;
    }
    const uint32_t ia = 255u - a;
    px[0] = static_cast<uint8_t>((px[0] * a + bgB * ia + 127u) / 255u);
    px[1] = static_cast<uint8_t>((px[1] * a + bgG * ia + 127u) / 255u);
    px[2] = static_cast<uint8_t>((px[2] * a + bgR * ia + 127u) / 255u);
    px[3] = 255;
  }
}

// 判定一个 BGRA 像素是否是「近黑」(各颜色通道 ≤ darkMax 且 alpha > 0)。
inline bool IsNearBlack(const uint8_t* px, uint8_t darkMax) {
  if (!px || px[3] == 0)
    return false;
  return px[0] <= darkMax && px[1] <= darkMax && px[2] <= darkMax;
}

}  // namespace panel_logo
}  // namespace fluxing

#endif  // FLUXING_PANEL_LOGO_H_
