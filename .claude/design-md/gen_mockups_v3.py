"""UserDictionary v3 mockup — gen_mockups.py + (a) larger canvas 920x600
(b) larger fonts (FONT_BODY 13->16, FONT_LABEL 17->21, caption 11->13,
mono 12->14).

Output: user-dictionary.png (920x600, overwrites v1 output)

Design language preserved (Liquid Discipline + FLUENT-UI-TOKENS §3.6):
  - radius.lg=14
  - per-pixel alpha glass chrome
  - hairline + top highlight
  - peach kSelBg + accent orange selection
  - YaHei body + Display + DM Mono data
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

# Tokens
K_BG_TOP       = (245, 245, 248)
K_BG_BOT       = (220, 222, 230)
K_TEXT         = ( 30,  30,  40)
K_TEXT_50      = ( 30,  30,  40, 128)
K_TEXT_35      = ( 30,  30,  40,  90)
K_SEL_BG       = (255, 235, 220)
K_ACCENT       = (255,  95,  49)
K_HAIRLINE     = (  0,   0,   0,  20)
K_HAIRLINE_TOP = (255, 255, 255, 100)
K_SHADOW       = (  0,   0,   0,  30)
K_ICON_DIM     = (130, 130, 140)
K_ICON_ACCENT  = (255,  95,  49)

# --- v3 enlarged sizing ---
RADIUS_LG      = 14
FONT_BODY_PX   = 16   # was 14 (also FONT_LABEL nav back-compat)
FONT_LABEL_PX  = 21   # was 17
FONT_CAPTION_PX= 13   # was 11
FONT_MONO_PX   = 14   # was 12
TITLE_H        = 38   # was 30
GAP_XS         = 6    # was 4
GAP_SM         = 12   # was 8
GAP_LG         = 16   # was 11
GAP_XL         = 24   # was 16
GAP_MD         = 16   # was 12
LINE_HAIR      = 1
PANEL_PADDING  = 16   # was 12
SEARCH_H       = 38   # was 30
ROW_H          = 38   # was 28
BTN_H          = 38   # was 30
HEADER_H       = 30   # was 28

CANVAS_W       = 920  # was 760
CANVAS_H       = 600  # was 480

FONT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
                        'skills', 'canvas-design', 'canvas-fonts')
WIN_FONT_DIR = r'C:\Windows\Fonts'


def load_font(size_px, weight='regular', mono=False):
    """Try several fonts in priority order. Prefer CJK-capable for Chinese text."""
    candidates = []
    if weight == 'bold':
        candidates += [
            os.path.join(WIN_FONT_DIR, 'msyhbd.ttc'),
            os.path.join(WIN_FONT_DIR, 'msjhbd.ttc'),
            os.path.join(WIN_FONT_DIR, 'simsunb.ttf'),
            os.path.join(FONT_DIR, 'BigShoulders-Bold.ttf'),
            os.path.join(WIN_FONT_DIR, 'segoeuib.ttf'),
            os.path.join(WIN_FONT_DIR, 'arialbd.ttf'),
        ]
    elif mono:
        candidates += [
            os.path.join(WIN_FONT_DIR, 'CascadiaMono.ttf'),
            os.path.join(WIN_FONT_DIR, 'consola.ttf'),
            os.path.join(WIN_FONT_DIR, 'consolab.ttf'),
            os.path.join(WIN_FONT_DIR, 'lucon.ttf'),
            os.path.join(FONT_DIR, 'DMMono-Regular.ttf'),
        ]
    else:
        candidates += [
            os.path.join(WIN_FONT_DIR, 'msyh.ttc'),
            os.path.join(WIN_FONT_DIR, 'msyh.ttf'),
            os.path.join(WIN_FONT_DIR, 'simsun.ttc'),
            os.path.join(WIN_FONT_DIR, 'msjh.ttc'),
            os.path.join(WIN_FONT_DIR, 'YuGothR.ttc'),
            os.path.join(WIN_FONT_DIR, 'simfang.ttf'),
            os.path.join(FONT_DIR, 'ArsenalSC-Regular.ttf'),
            os.path.join(FONT_DIR, 'BricolageGrotesque-Regular.ttf'),
            os.path.join(WIN_FONT_DIR, 'segoeui.ttf'),
            os.path.join(WIN_FONT_DIR, 'arial.ttf'),
        ]
    for path in candidates:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size_px)
            except Exception:
                continue
    return ImageFont.load_default()


def draw_panel_chrome(canvas, w, h, title):
    """Draw the Liquid Glass chrome: rounded rect, gradient fill, hairline, top highlight."""
    shadow = Image.new('RGBA', (w + 12, h + 12), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    for i in range(6):
        sd.rounded_rectangle(
            [i, i, w + 6 + i, h + 6 + i],
            radius=RADIUS_LG + i,
            outline=(0, 0, 0, max(0, K_SHADOW[3] - i * 4)),
            width=1
        )
    canvas.paste(shadow, (-6, -6), shadow)
    glass = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glass)
    for y in range(h):
        t = y / max(1, h - 1)
        r = int(K_BG_TOP[0] * (1 - t) + K_BG_BOT[0] * t)
        g = int(K_BG_TOP[1] * (1 - t) + K_BG_BOT[1] * t)
        b = int(K_BG_TOP[2] * (1 - t) + K_BG_BOT[2] * t)
        a = int(220 + (80 - 220) * t)
        gd.line([(0, y), (w, y)], fill=(r, g, b, a))
    mask = Image.new('L', (w, h), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle([0, 0, w - 1, h - 1], radius=RADIUS_LG, fill=255)
    canvas.paste(glass, (0, 0), mask)
    bd = ImageDraw.Draw(canvas)
    bd.rounded_rectangle([0, 0, w - 1, h - 1], radius=RADIUS_LG,
                          outline=K_HAIRLINE, width=1)
    bd.line([(RADIUS_LG, 1), (w - RADIUS_LG, 1)], fill=K_HAIRLINE_TOP, width=1)
    font_label = load_font(FONT_LABEL_PX, 'bold')
    bd.text((PANEL_PADDING, (TITLE_H - FONT_LABEL_PX) // 2 + 1),
             title, fill=K_TEXT, font=font_label)
    x_btn_x = w - PANEL_PADDING - 16
    x_btn_y = (TITLE_H - 16) // 2 + 1
    bd.line([(x_btn_x, x_btn_y), (x_btn_x + 16, x_btn_y + 16)],
             fill=K_TEXT, width=2)
    bd.line([(x_btn_x + 16, x_btn_y), (x_btn_x, x_btn_y + 16)],
             fill=K_TEXT, width=2)


def draw_text(d, xy, text, font, fill=K_TEXT):
    d.text(xy, text, fill=fill, font=font)


def draw_text_rgba(canvas, xy, text, font, fill):
    img = Image.new('RGBA', (max(60, len(text) * (font.size if hasattr(font, 'size') else 14) + 4),
                              (font.size if hasattr(font, 'size') else 14) + 8),
                    (0, 0, 0, 0))
    td = ImageDraw.Draw(img)
    td.text((0, 0), text, fill=fill, font=font)
    canvas.paste(img, xy, img)


def draw_button(canvas, xy, wh, label, style='normal'):
    """Draw a mac-style push button."""
    x, y = xy
    bw, bh = wh
    bd = ImageDraw.Draw(canvas)
    radius = 6
    if style == 'primary':
        bd.rounded_rectangle([x, y, x + bw, y + bh], radius=radius, fill=K_ACCENT)
        txt_color = (255, 255, 255)
    elif style == 'disabled':
        bd.rounded_rectangle([x, y, x + bw, y + bh], radius=radius,
                              fill=(245, 245, 248))
        txt_color = K_TEXT_35
    else:
        bd.rounded_rectangle([x, y, x + bw, y + bh], radius=radius,
                              fill=(255, 255, 255), outline=K_HAIRLINE, width=1)
        txt_color = K_TEXT
    font_btn = load_font(FONT_BODY_PX, 'regular')
    bbox = bd.textbbox((0, 0), label, font=font_btn)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    bd.text((x + (bw - tw) // 2, y + (bh - th) // 2 - 2),
             label, fill=txt_color, font=font_btn)


def draw_search_box(canvas, xy, wh, placeholder=True, query=''):
    """Draw a search input field."""
    x, y = xy
    bw, bh = wh
    bd = ImageDraw.Draw(canvas)
    radius = 6
    bd.rounded_rectangle([x, y, x + bw, y + bh], radius=radius,
                          fill=(255, 255, 255, 230), outline=K_HAIRLINE, width=1)
    icon_x = x + 10
    icon_y = y + bh // 2
    bd.ellipse([icon_x - 5, icon_y - 5, icon_x + 5, icon_y + 5],
               outline=K_ICON_DIM, width=2)
    bd.line([(icon_x + 5, icon_y + 5), (icon_x + 10, icon_y + 10)],
             fill=K_ICON_DIM, width=2)
    font_input = load_font(FONT_BODY_PX, 'regular')
    text_x = icon_x + 18
    if query:
        draw_text(bd, (text_x, y + (bh - FONT_BODY_PX) // 2 - 1),
                   query, font_input, K_TEXT)
    elif placeholder:
        draw_text_rgba(canvas, (text_x, y + (bh - FONT_BODY_PX) // 2 - 1),
                        '搜索...', font_input, K_TEXT_50)


def draw_hairline(canvas, xy, length, horizontal=True):
    bd = ImageDraw.Draw(canvas)
    if horizontal:
        bd.line([xy, (xy[0] + length, xy[1])], fill=K_HAIRLINE, width=1)
    else:
        bd.line([xy, (xy[0], xy[1] + length)], fill=K_HAIRLINE, width=1)


# ============================================================
# Mockup: UserDictionary (920 × 600) — only this one is kept in v3
# ============================================================
def make_user_dictionary():
    W, H = CANVAS_W, CANVAS_H
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_panel_chrome(canvas, W, H, '')
    bd = ImageDraw.Draw(canvas)

    font_title = load_font(21, 'bold')
    font_body = load_font(FONT_BODY_PX, 'regular')
    font_body_b = load_font(FONT_BODY_PX, 'bold')
    font_caption = load_font(FONT_CAPTION_PX, 'regular')
    font_mono_path = os.path.join(FONT_DIR, 'DMMono-Regular.ttf')
    if os.path.exists(font_mono_path):
        font_mono = ImageFont.truetype(font_mono_path, FONT_MONO_PX)
    else:
        font_mono = load_font(FONT_MONO_PX, mono=True)

    # Bespoke title bar: book glyph, two-line identity, restrained close control.
    icon_x, icon_y = PANEL_PADDING, 12
    bd.rounded_rectangle([icon_x, icon_y, icon_x + 16, icon_y + 14], radius=2,
                         fill=(255, 255, 255, 130), outline=(118, 118, 128, 130), width=1)
    bd.line([(icon_x + 8, icon_y + 2), (icon_x + 8, icon_y + 12)], fill=K_ACCENT, width=1)
    bd.line([(icon_x + 2, icon_y + 4), (icon_x + 6, icon_y + 4)], fill=K_ICON_DIM, width=1)
    bd.text((PANEL_PADDING + 24, 6), '用户词典', fill=K_TEXT, font=font_title)
    bd.text((PANEL_PADDING + 148, 12), 'Personal dictionary · 247 entries',
            fill=(110, 110, 120), font=font_caption)
    close_x, close_y = W - PANEL_PADDING - 12, 12
    bd.line([(close_x, close_y), (close_x + 12, close_y + 12)], fill=(75, 75, 84), width=1)
    bd.line([(close_x + 12, close_y), (close_x, close_y + 12)], fill=(75, 75, 84), width=1)

    # Toolbar remains structurally identical: search on left, context/status on right.
    toolbar_y = 44
    search_w = 360
    draw_search_box(canvas, (PANEL_PADDING, toolbar_y), (search_w, SEARCH_H))
    bd.rounded_rectangle([PANEL_PADDING + search_w + GAP_SM,
                          toolbar_y + (SEARCH_H - 26) // 2,
                          PANEL_PADDING + search_w + GAP_SM + 56,
                          toolbar_y + (SEARCH_H - 26) // 2 + 26], radius=6,
                         fill=(238, 238, 242), outline=(0, 0, 0, 16), width=1)
    bd.text((PANEL_PADDING + search_w + GAP_SM + 10,
             toolbar_y + (SEARCH_H - 26) // 2 + 4),
            '⌘F', fill=(112, 112, 122), font=font_caption)

    def pill(x, width, label, dot=None):
        bd.rounded_rectangle([x, toolbar_y + (SEARCH_H - 28) // 2,
                              x + width, toolbar_y + (SEARCH_H - 28) // 2 + 28],
                             radius=10,
                             fill=(255, 255, 255, 178), outline=(0, 0, 0, 18), width=1)
        tx = x + 12
        if dot:
            bd.ellipse([x + 12, toolbar_y + (SEARCH_H - 28) // 2 + 8,
                        x + 20, toolbar_y + (SEARCH_H - 28) // 2 + 16],
                       fill=dot)
            tx = x + 26
        bd.text((tx, toolbar_y + (SEARCH_H - 28) // 2 + 6),
                label, fill=(62, 62, 72), font=font_caption)

    pill_x = PANEL_PADDING + search_w + GAP_SM + 56 + GAP_LG
    pill(pill_x, 168, '已部署 · 3 分钟前', (52, 176, 94))
    pill(pill_x + 168 + GAP_SM, 156, 'luna_pinyin  ▾')
    pill(pill_x + 168 + GAP_SM + 156 + GAP_SM, 132, '更多  ▾')
    draw_hairline(canvas, (0, toolbar_y + SEARCH_H + GAP_SM), W)

    # Finder-like table: column geometry encodes information weight.
    table_x, table_y, table_w = 8, toolbar_y + SEARCH_H + GAP_SM + GAP_MD, W - 16
    col_x = [table_x, table_x + 280, table_x + 480, table_x + 580]
    col_w = [280, 200, 100, table_w - 580]
    headers = [('词条', '▼'), ('编码', '↕'), ('权重', '↕'), ('方案', '↕')]
    bd.rounded_rectangle([table_x, table_y, table_x + table_w,
                          table_y + 9 * ROW_H + HEADER_H], radius=9,
                         fill=(255, 255, 255, 84), outline=(0, 0, 0, 18), width=1)
    bd.rounded_rectangle([table_x, table_y, table_x + table_w, table_y + HEADER_H],
                         radius=9, fill=(255, 255, 255, 126))
    bd.rectangle([table_x, table_y + 18, table_x + table_w, table_y + HEADER_H],
                 fill=(255, 255, 255, 126))
    for idx, ((label, sort), cx, cw) in enumerate(zip(headers, col_x, col_w)):
        bd.text((cx + 12, table_y + 8), label, fill=(92, 92, 103), font=font_body_b)
        sort_fill = K_ACCENT if idx == 0 else (154, 154, 164)
        bd.text((cx + cw - 24, table_y + 9), sort, fill=sort_fill, font=font_caption)
        if idx < len(headers) - 1:
            divider_x = cx + cw
            for offset, alpha in [(-2, 0), (-1, 8), (0, 28), (1, 8)]:
                bd.line([(divider_x + offset, table_y + 6),
                         (divider_x + offset, table_y + HEADER_H - 6)],
                        fill=(0, 0, 0, alpha), width=1)
    bd.line([(table_x, table_y + HEADER_H), (table_x + table_w, table_y + HEADER_H)],
            fill=(0, 0, 0, 18), width=1)

    rows = [
        ('Fluxing输入法', 'fluxing', 100, 'luna_pinyin'),
        ('小狼毫', 'xlh', 90, 'luna_pinyin'),
        ('火流猩', 'hlx', 100, 'luna_pinyin'),
        ('北京', 'beij', 50, 'luna_pinyin'),
        ('上海', 'shang', 50, 'luna_pinyin'),
        ('Fluxing 项目', 'fluxproj', 85, 'luna_pinyin'),
        ('火流猩输入法', 'hlxsr', 95, 'luna_pinyin'),
        ('自然语言处理', 'zryycl', 68, 'luna_pinyin'),
        ('设计系统', 'shejixitong', 32, 'luna_pinyin'),
    ]

    def weight_color(weight):
        if weight <= 30:
            return K_ACCENT
        if weight <= 70:
            return (225, 156, 45)
        return (55, 166, 92)

    row_y = table_y + HEADER_H
    for index, (text, code, weight, schema) in enumerate(rows):
        selected = index == 0
        hovered = index == 3
        if selected:
            bd.rectangle([table_x + 1, row_y, table_x + table_w - 1, row_y + ROW_H],
                         fill=K_SEL_BG)
            bd.rectangle([table_x + 1, row_y, table_x + 5, row_y + ROW_H],
                         fill=K_ACCENT)
        elif hovered:
            bd.rectangle([table_x + 1, row_y, table_x + table_w - 1, row_y + ROW_H],
                         fill=(255, 255, 255, 92))
        if index:
            bd.line([(table_x + 8, row_y), (table_x + table_w - 8, row_y)],
                    fill=(0, 0, 0, 9), width=1)

        bd.text((col_x[0] + 12, row_y + 8), text, fill=K_TEXT,
                font=font_body_b if selected else font_body)
        bd.text((col_x[1] + 12, row_y + 9), code, fill=(66, 66, 77), font=font_mono)

        wc = weight_color(weight)
        bd.text((col_x[2] + 10, row_y + 8), str(weight), fill=wc, font=font_body_b)
        track_x, track_y, track_w = col_x[2] + 48, row_y + 17, 38
        bd.rounded_rectangle([track_x, track_y, track_x + track_w, track_y + 5],
                             radius=2, fill=(196, 196, 203, 105))
        bd.rounded_rectangle([track_x, track_y,
                              track_x + max(3, int(track_w * weight / 100)),
                              track_y + 5], radius=2, fill=wc)

        chip_x, chip_w = col_x[3] + 12, 110
        bd.rounded_rectangle([chip_x, row_y + 8, chip_x + chip_w, row_y + 28],
                             radius=10,
                             fill=(231, 232, 238, 180), outline=(0, 0, 0, 12), width=1)
        bd.text((chip_x + 12, row_y + 10), schema, fill=(83, 83, 94),
                font=font_caption)

        if selected:
            action_x = table_x + table_w - 72
            bd.rounded_rectangle([action_x, row_y + 6, action_x + 60, row_y + 30],
                                 radius=6,
                                 fill=(255, 255, 255, 180), outline=(0, 0, 0, 16),
                                 width=1)
            bd.text((action_x + 12, row_y + 5), '✎', fill=(75, 75, 85),
                    font=font_body)
            bd.line([(action_x + 32, row_y + 12), (action_x + 32, row_y + 26)],
                    fill=(0, 0, 0, 18), width=1)
            bd.text((action_x + 42, row_y + 6), '−', fill=(208, 69, 69),
                    font=font_body_b)
        row_y += ROW_H

    # Compact status strip doubles as the multi-select affordance.
    status_y = row_y + GAP_SM
    bd.text((PANEL_PADDING, status_y + 8),
            '247 条词条  ·  当前方案 luna_pinyin',
            fill=(104, 104, 115), font=font_caption)
    bd.text((W - PANEL_PADDING - 240, status_y + 8),
            'Ctrl / Shift 点击可多选',
            fill=(128, 128, 138), font=font_caption)

    # Bottom actions: creation left, destructive/contextual middle, primary deploy right.
    footer_y = status_y + GAP_LG + GAP_SM
    draw_hairline(canvas, (0, footer_y), W)
    footer_y += GAP_SM
    btn_w_l = 100   # left buttons slightly wider for new font size
    btn_w_m = 80
    btn_w_r = 86
    deploy_w = 130
    draw_button(canvas, (PANEL_PADDING, footer_y + 4), (btn_w_l, BTN_H), '+ 添加', 'normal')
    draw_button(canvas, (PANEL_PADDING + btn_w_l + GAP_SM, footer_y + 4),
                 (btn_w_l, BTN_H), '− 删除', 'normal')
    draw_button(canvas, (PANEL_PADDING + 2 * (btn_w_l + GAP_SM), footer_y + 4),
                 (btn_w_m, BTN_H), '导入', 'normal')
    draw_button(canvas, (PANEL_PADDING + 2 * (btn_w_l + GAP_SM) + btn_w_m + GAP_SM,
                         footer_y + 4),
                 (btn_w_m, BTN_H), '导出', 'normal')
    draw_button(canvas, (W - PANEL_PADDING - deploy_w - btn_w_r - GAP_LG, footer_y + 4),
                 (btn_w_r, BTN_H), '取消', 'normal')
    draw_button(canvas, (W - PANEL_PADDING - deploy_w, footer_y + 4),
                 (deploy_w, BTN_H), '⟳ 部署', 'primary')
    return canvas


# ============================================================
# Driver
# ============================================================
def main():
    here = os.path.dirname(os.path.abspath(__file__))
    targets = [
        ('user-dictionary.png', make_user_dictionary),
    ]
    for name, fn in targets:
        img = fn()
        out = os.path.join(here, name)
        img.save(out)
        print(f"  wrote {out} ({img.size[0]}x{img.size[1]})")


if __name__ == '__main__':
    main()