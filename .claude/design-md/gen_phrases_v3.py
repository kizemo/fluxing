"""PhrasesDialog v3 mockup — v2 + (a) larger canvas 720x720 (b) larger fonts
(c) fix 删除/取消 button overlap bug.

Output: phrases-dialog-v2.png (720x720, overwrites v2 output to keep filename stable)

Bug fix math (was overlap = 28 px in v2 at W=480):
  v2:  delete ends at x = PANEL_PADDING + 3*(92+8) = 14 + 300 = 314
       cancel starts at x = (W - PANEL_PADDING - save_w) - 80 = (480-14-100) - 80 = 286
       OVERLAP = 28 px
  v3:  W=720, BTN_W=110, GAP_LG=16, PANEL_PADDING=16
       Left side total = PANEL_PADDING + 3*(BTN_W + GAP_LG) - GAP_LG
                       = 16 + 3*126 - 16 = 16 + 362 = 378
       cancel_w=86, save_w=120, GAP_LG=16
       save_x     = W - PANEL_PADDING - save_w        = 720 - 16 - 120 = 584
       cancel_x   = save_x - cancel_w - GAP_LG       = 584 - 86 - 16  = 482
       Gap between left total (378) and cancel_x (482) = 104 px  (>= 32, OK)
       Gap between cancel_x+cancel_w (568) and save_x (584) = 16 px (>= 16, OK)
"""
import os
from PIL import Image, ImageDraw, ImageFont

# ============================================================
# Tokens (unchanged from v2 — design language is preserved)
# ============================================================
K_BG_TOP    = (245, 245, 248)
K_BG_BOT    = (220, 222, 230)
K_TEXT      = ( 30,  30,  40)
K_TEXT_50   = ( 30,  30,  40, 128)
K_TEXT_35   = ( 30,  30,  40,  90)
K_SEL_BG    = (255, 235, 220)
K_ACCENT    = (255,  95,  49)
K_ACCENT_2  = (155,  81, 224)
K_HAIRLINE  = (  0,   0,   0,  20)
K_HAIRLINE_2= (  0,   0,   0,  32)
K_HAIRLINE_TOP = (255, 255, 255, 100)
K_SHADOW    = (  0,   0,   0,  30)
K_ICON_DIM  = (130, 130, 140)
K_ICON_TEXT = ( 75,  75,  85)
K_GREEN_DOT = ( 52, 176,  94)
K_WARN_BG   = (255, 244, 220)
K_WARN_TXT  = (120,  80,  30)
K_EDIT_BG   = (255, 252, 240)
K_EDIT_BORDER = (255, 149,  85)
K_SUCCESS   = ( 52, 176,  94)
K_DESTRUCT  = (208,  69,  69)
K_HOVER     = (255, 255, 255,  92)

RADIUS_LG      = 14
RADIUS_MD      = 10
RADIUS_SM      = 7
RADIUS_XS      = 5
RADIUS_PILL    = 9999

# --- v3: enlarged type scale (~1.25x body, ~1.24x label, ~1.18x caption) ---
FONT_TITLE_PX  = 21   # was 17
FONT_BODY_PX   = 16   # was 13
FONT_CAPTION_PX= 13   # was 11
FONT_BTN_PX    = 16   # was 13 (button text follows body)
FONT_MONO_PX   = 14   # was 12 (mono for shortcut codes)

# --- v3: enlarged geometry ---
TITLE_H        = 38   # was 30
GAP_XS         = 6    # was 4
GAP_SM         = 12   # was 8
GAP_MD         = 16   # was 12
GAP_LG         = 16   # was 14
GAP_XL         = 24   # was 16
PANEL_PADDING  = 16   # was 14
ROW_H          = 38   # was 24
SEARCH_H       = 38   # was 30
BTN_H          = 38   # was 30
BTN_W          = 110  # was 92
CANCEL_W       = 86   # was 72
SAVE_W         = 120  # was 100

# --- v3: canvas size ---
CANVAS_W       = 720
CANVAS_H       = 720

WIN_FONT_DIR = r'C:\Windows\Fonts'
FONT_DIR_CANDIDATE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    'skills', 'canvas-design', 'canvas-fonts'
)


def load_font(size_px, weight='regular'):
    candidates = []
    if weight == 'bold':
        candidates += [
            os.path.join(WIN_FONT_DIR, 'msyhbd.ttc'),
            os.path.join(WIN_FONT_DIR, 'msjhbd.ttc'),
            os.path.join(WIN_FONT_DIR, 'simsunb.ttf'),
            os.path.join(WIN_FONT_DIR, 'segoeuib.ttf'),
            os.path.join(WIN_FONT_DIR, 'arialbd.ttf'),
        ]
    elif weight == 'mono':
        candidates += [
            os.path.join(WIN_FONT_DIR, 'consola.ttf'),
            os.path.join(WIN_FONT_DIR, 'CascadiaMono.ttf'),
            os.path.join(WIN_FONT_DIR, 'consolab.ttf'),
            os.path.join(WIN_FONT_DIR, 'lucon.ttf'),
            os.path.join(WIN_FONT_DIR, 'arial.ttf'),
        ]
    else:
        candidates += [
            os.path.join(WIN_FONT_DIR, 'msyh.ttc'),
            os.path.join(WIN_FONT_DIR, 'msyh.ttf'),
            os.path.join(WIN_FONT_DIR, 'simsun.ttc'),
            os.path.join(WIN_FONT_DIR, 'msjh.ttc'),
            os.path.join(WIN_FONT_DIR, 'YuGothR.ttc'),
            os.path.join(WIN_FONT_DIR, 'simfang.ttf'),
            os.path.join(FONT_DIR_CANDIDATE, 'DMMono-Regular.ttf'),
            os.path.join(WIN_FONT_DIR, 'consola.ttf'),
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


# ============================================================
# Drawing helpers (mirror style of v2)
# ============================================================
def draw_panel_chrome(canvas, w, h):
    """Liquid Glass chrome — rounded rect, gradient, hairline, top highlight."""
    shadow = Image.new('RGBA', (w + 16, h + 16), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    for i in range(8):
        sd.rounded_rectangle(
            [i, i, w + 8 + i, h + 8 + i],
            radius=RADIUS_LG + i,
            outline=(0, 0, 0, max(0, K_SHADOW[3] - i * 4)),
            width=1
        )
    canvas.paste(shadow, (-8, -8), shadow)
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
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=RADIUS_LG, fill=255)
    canvas.paste(glass, (0, 0), mask)
    bd = ImageDraw.Draw(canvas)
    bd.rounded_rectangle([0, 0, w - 1, h - 1], radius=RADIUS_LG,
                         outline=K_HAIRLINE, width=1)
    bd.line([(RADIUS_LG, 1), (w - RADIUS_LG, 1)], fill=K_HAIRLINE_TOP, width=1)


def draw_title_bar(canvas, w, title_cn, subtitle_en, icon_kind='folder'):
    bd = ImageDraw.Draw(canvas)
    font_title  = load_font(FONT_TITLE_PX, 'bold')
    font_caption = load_font(FONT_CAPTION_PX, 'regular')

    icon_x, icon_y = 16, 14
    bd.rounded_rectangle([icon_x, icon_y, icon_x + 14, icon_y + 13], radius=2,
                         fill=(255, 255, 255, 130),
                         outline=(118, 118, 128, 130), width=1)
    if icon_kind == 'folder':
        bd.line([(icon_x + 2, icon_y + 3), (icon_x + 8, icon_y + 3)],
                fill=K_ICON_DIM, width=1)
        bd.line([(icon_x + 5, icon_y + 6), (icon_x + 13, icon_y + 6)],
                fill=K_ICON_DIM, width=1)
        bd.line([(icon_x + 5, icon_y + 9), (icon_x + 12, icon_y + 9)],
                fill=K_ICON_DIM, width=1)
    elif icon_kind == 'edit':
        bd.line([(icon_x + 2, icon_y + 11), (icon_x + 13, icon_y + 2)],
                fill=K_ACCENT, width=1)
        bd.polygon([(icon_x + 13, icon_y + 2),
                    (icon_x + 15, icon_y + 1),
                    (icon_x + 14, icon_y + 3)], fill=K_ACCENT)
    elif icon_kind == 'search':
        bd.ellipse([icon_x + 1, icon_y + 1, icon_x + 10, icon_y + 10],
                   outline=K_ICON_DIM, width=1)
        bd.line([(icon_x + 9, icon_y + 9), (icon_x + 13, icon_y + 13)],
                fill=K_ICON_DIM, width=2)

    bd.text((38, 6), title_cn, fill=K_TEXT, font=font_title)
    bd.text((38, 28),
            subtitle_en, fill=(110, 110, 120), font=font_caption)

    # Close × — vertically centered vs the title row
    cx, cy = w - 24, 18
    bd.ellipse([cx - 6, cy - 6, cx + 12, cy + 12],
               fill=(255, 255, 255, 70))
    bd.line([(cx + 1, cy + 1), (cx + 8, cy + 8)], fill=K_ICON_TEXT, width=1)
    bd.line([(cx + 8, cy + 1), (cx + 1, cy + 8)], fill=K_ICON_TEXT, width=1)


def draw_hairline(canvas, xy, length, horizontal=True, color=K_HAIRLINE):
    bd = ImageDraw.Draw(canvas)
    if horizontal:
        bd.line([xy, (xy[0] + length, xy[1])], fill=color, width=1)
    else:
        bd.line([xy, (xy[0], xy[1] + length)], fill=color, width=1)


def draw_text(d, xy, text, font, fill=K_TEXT):
    d.text(xy, text, fill=fill, font=font)


def draw_search_field(canvas, x, y, w, h, query='', hint=True, active=False):
    """macOS standard search field: white pill + ⌘F hint + magnifier glyph + clear ×."""
    bd = ImageDraw.Draw(canvas)
    radius = RADIUS_MD
    bd.rounded_rectangle([x, y, x + w, y + h], radius=radius,
                         fill=(255, 255, 255, 235 if not active else 250),
                         outline=(0, 0, 0, 28 if active else 18), width=1)
    cx, cy = x + 16, y + h // 2
    bd.ellipse([cx - 5, cy - 5, cx + 6, cy + 6],
               outline=(110, 110, 120), width=1)
    bd.line([(cx + 5, cy + 5), (cx + 9, cy + 9)],
            fill=(110, 110, 120), width=1)
    if hint:
        chip_w = 44
        cx0 = x + w - chip_w - 6
        bd.rounded_rectangle([cx0, y + (h - 22) // 2,
                              cx0 + chip_w, y + (h - 22) // 2 + 22], radius=RADIUS_XS,
                             fill=(238, 238, 242),
                             outline=(0, 0, 0, 16), width=1)
        bd.text((cx0 + 8, y + (h - 22) // 2 + 2),
                '⌘F', fill=(112, 112, 122),
                font=load_font(FONT_CAPTION_PX, 'regular'))
        text_area_w = w - chip_w - 44
    else:
        text_area_w = w - 44
    font_body = load_font(FONT_BODY_PX, 'regular')
    tx = x + 30
    ty = y + (h - FONT_BODY_PX) // 2 - 1
    if query:
        bd.text((tx, ty), query, fill=K_TEXT, font=font_body)
        bd.text((tx + 10 * len(query) + FONT_BODY_PX, ty - 1),
                '✕', fill=(150, 150, 158),
                font=load_font(FONT_BODY_PX, 'regular'))
    elif hint:
        bd.text((tx, ty), '搜索短语...', fill=K_TEXT_50, font=font_body)


def draw_chip(canvas, x, y, w, h, label, dot_color=None, tail=None, fill=None):
    bd = ImageDraw.Draw(canvas)
    if fill is None:
        fill = (255, 255, 255, 178)
    bd.rounded_rectangle([x, y, x + w, y + h], radius=h // 2,
                         fill=fill, outline=(0, 0, 0, 18), width=1)
    tx = x + 12
    if dot_color:
        bd.ellipse([x + 12, y + (h - 10) // 2, x + 22, y + (h - 10) // 2 + 10],
                   fill=dot_color)
        tx = x + 28
    bd.text((tx, y + (h - FONT_CAPTION_PX) // 2 - 1), label,
            fill=(62, 62, 72), font=load_font(FONT_CAPTION_PX, 'regular'))
    if tail:
        bd.text((x + w - 16, y + (h - FONT_CAPTION_PX) // 2 - 1),
                tail, fill=(110, 110, 120),
                font=load_font(FONT_CAPTION_PX, 'regular'))


def draw_chevron(d, x, y, expanded, color=K_ICON_DIM, size=10):
    pts = [(x, y),
           (x + size, y + size // 2),
           (x, y + size)] if not expanded else [
           (x, y),
           (x + size // 2, y + size),
           (x + size, y)]
    d.line(pts + [pts[0]], fill=color, width=1)


def draw_folder_glyph(d, x, y, accent=False, size=14):
    body = (118, 118, 128) if not accent else K_ACCENT
    d.line([(x + 1, y + 4), (x + 6, y + 4)], fill=body, width=1)
    d.line([(x + 0, y + 6), (x + size, y + 6)], fill=body, width=1)
    d.line([(x + 0, y + 6), (x + 0, y + size - 1)], fill=body, width=1)
    d.line([(x + size, y + 6), (x + size, y + size - 1)], fill=body, width=1)
    d.line([(x + 0, y + size - 1), (x + size, y + size - 1)], fill=body, width=1)


def draw_button(canvas, x, y, bw, bh, label, style='normal', shortcut=None,
                 icon=None, danger=False):
    """macOS-style push button. Shortcut hint floats to the right edge, no overlap."""
    bd = ImageDraw.Draw(canvas)
    radius = RADIUS_SM
    if style == 'primary':
        bd.rounded_rectangle([x, y, x + bw, y + bh], radius=radius, fill=K_ACCENT)
        label_fill = (255, 255, 255)
    elif style == 'destructive':
        bd.rounded_rectangle([x, y, x + bw, y + bh], radius=radius,
                             fill=(255, 245, 245),
                             outline=(208, 69, 69, 160), width=1)
        label_fill = K_DESTRUCT
    elif style == 'disabled':
        bd.rounded_rectangle([x, y, x + bw, y + bh], radius=radius,
                             fill=(245, 245, 248))
        label_fill = K_TEXT_35
    else:
        bd.rounded_rectangle([x, y, x + bw, y + bh], radius=radius,
                             fill=(255, 255, 255, 235),
                             outline=(0, 0, 0, 22 if not danger else 50), width=1)
        label_fill = K_TEXT if not danger else K_DESTRUCT
    font_btn = load_font(FONT_BTN_PX, 'regular')
    icon_gap = 6 if icon else 0
    icon_w = FONT_BTN_PX if icon else 0
    bbox_l = bd.textbbox((0, 0), label, font=font_btn)
    label_w = bbox_l[2] - bbox_l[0]
    total_content_w = icon_w + icon_gap + label_w
    shortcut_w = 0
    if shortcut:
        bbox_s = bd.textbbox((0, 0), shortcut, font=load_font(FONT_CAPTION_PX, 'regular'))
        shortcut_w = bbox_s[2] - bbox_s[0] + 18
    usable_w = bw - shortcut_w
    cursor_x = x + (usable_w - total_content_w) // 2
    if icon:
        bd.text((cursor_x, y + (bh - FONT_BTN_PX) // 2 - 1), icon,
                fill=label_fill, font=font_btn)
        cursor_x += icon_w + icon_gap
    bd.text((cursor_x, y + (bh - FONT_BTN_PX) // 2 - 1),
            label, fill=label_fill, font=font_btn)
    if shortcut:
        bd.text((x + bw - shortcut_w + 4, y + (bh - FONT_CAPTION_PX) // 2 + 1),
                shortcut,
                fill=(118, 118, 128, 200) if style != 'primary' else (255, 235, 220, 220),
                font=load_font(FONT_CAPTION_PX, 'regular'))


# ============================================================
# Primary state — Browsing, with row selection
# ============================================================
def make_phrases_dialog():
    W, H = CANVAS_W, CANVAS_H
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_panel_chrome(canvas, W, H)
    draw_title_bar(canvas, W, '常用短语',
                   'Manage text snippets · 12 entries', icon_kind='folder')
    bd = ImageDraw.Draw(canvas)

    # Toolbar row — search | chips on the right
    toolbar_y = 50
    search_w = 320
    draw_search_field(canvas, PANEL_PADDING, toolbar_y, search_w, SEARCH_H)
    pill_x = PANEL_PADDING + search_w + GAP_LG
    pill_h = 28
    # compute pill widths by content
    cap_font = load_font(FONT_CAPTION_PX, 'regular')
    def pill_text_width(text):
        bb = bd.textbbox((0, 0), text, font=cap_font)
        return bb[2] - bb[0]
    pill1_label = '已部署 · 3 分钟前'
    pill1_w = pill_text_width(pill1_label) + 56  # dot + padding
    draw_chip(canvas, pill_x, toolbar_y + (SEARCH_H - pill_h) // 2,
              pill1_w, pill_h, pill1_label, dot_color=K_GREEN_DOT)
    pill_x += pill1_w + GAP_MD
    pill2_label = '按字母排序'
    pill2_w = pill_text_width(pill2_label) + 38  # tail ▾ + padding
    draw_chip(canvas, pill_x, toolbar_y + (SEARCH_H - pill_h) // 2,
              pill2_w, pill_h, pill2_label, tail='▾')
    draw_hairline(canvas, (0, toolbar_y + SEARCH_H + GAP_SM), W)

    # Tree header band
    tree_top = toolbar_y + SEARCH_H + GAP_SM + GAP_MD
    bd.rounded_rectangle([8, tree_top, W - 8, tree_top + 30], radius=RADIUS_SM,
                         fill=(255, 255, 255, 80), outline=(0, 0, 0, 14), width=1)
    bd.rectangle([8, tree_top + 15, W - 8, tree_top + 30], fill=(255, 255, 255, 80))
    col_shortcut_x = W - PANEL_PADDING - 130
    col_category_x = W - PANEL_PADDING - 80
    col_count_x    = W - PANEL_PADDING - 32
    bd.text((PANEL_PADDING + 6, tree_top + 6), '短语',
            fill=(92, 92, 103), font=load_font(FONT_BODY_PX, 'bold'))
    bd.text((PANEL_PADDING + 64, tree_top + 9), '▼',
            fill=K_ACCENT, font=load_font(FONT_CAPTION_PX, 'regular'))
    bd.text((col_shortcut_x, tree_top + 6), '快捷键', fill=(92, 92, 103),
            font=load_font(FONT_BODY_PX, 'bold'))
    bd.text((col_category_x, tree_top + 7), '分类', fill=(92, 92, 103),
            font=load_font(FONT_BODY_PX, 'bold'))
    bd.text((col_count_x, tree_top + 7), '计数', fill=(92, 92, 103),
            font=load_font(FONT_BODY_PX, 'bold'))

    # Tree rows
    tree_left = 8
    tree_right = W - 8
    tree_x0 = 20
    row_h = ROW_H
    font_body = load_font(FONT_BODY_PX, 'regular')
    font_body_b = load_font(FONT_BODY_PX, 'bold')
    font_caption = load_font(FONT_CAPTION_PX, 'regular')
    font_mono = load_font(FONT_MONO_PX, 'mono')

    rows = [
        ('cat-expanded',  0, '工作',     None,  None, 3, True),
        ('phrase',        1, '你好',     'nj',  '工作', None, False),
        ('phrase-selected', 1, '谢谢',    'tx',  '工作', None, False),
        ('phrase',        1, '期待合作', 'qzhz','工作', None, False),
        ('cat-collapsed', 0, '日常',     None,  None, 2, False),
        ('cat-collapsed', 0, '编辑类',   None,  None, 1, False),
        ('cat-expanded',  0, '自定义',   None,  None, 3, True),
        ('phrase',        1, 'Hello',    'h',   '自定义', None, False),
        ('phrase',        1, '测试短语', 'csc', '自定义', None, False),
        ('phrase',        1, '签名档',   'qmd', '自定义', None, False),
    ]
    y = tree_top + 38
    selected_row_y = None
    for kind, indent, text, shortcut, cat_name, count, _accent in rows:
        is_cat = kind.startswith('cat')
        expanded = kind == 'cat-expanded'
        is_selected = kind == 'phrase-selected'
        row_bg = [tree_left + 1, y - 2, tree_right - 1, y + row_h - 2]
        if is_selected:
            bd.rectangle([tree_left + 1, y - 2, tree_right - 1, y + row_h - 2],
                         fill=K_SEL_BG)
            bd.rectangle([tree_left + 1, y - 2, tree_left + 5, y + row_h - 2],
                         fill=K_ACCENT)
            selected_row_y = y
        chev_x = tree_x0 + indent * 18
        if is_cat:
            draw_chevron(bd, chev_x, y + row_h // 2 - 5, expanded)
        icon_x = chev_x + (20 if is_cat else 0)
        if is_cat:
            draw_folder_glyph(bd, icon_x, y + row_h // 2 - 7, accent=False, size=14)
            text_x = icon_x + 22
        else:
            text_x = icon_x + 6
        label = text
        label_font = font_body_b if is_cat or is_selected else font_body
        bd.text((text_x, y + (row_h - FONT_BODY_PX) // 2),
                label, fill=K_TEXT, font=label_font)
        if not is_cat:
            bd.text((col_shortcut_x, y + (row_h - FONT_BODY_PX) // 2),
                    shortcut or '', fill=(110, 110, 120), font=font_mono)
            bd.text((col_category_x, y + (row_h - FONT_BODY_PX) // 2),
                    cat_name or '', fill=(110, 110, 120), font=font_body)
        else:
            count_text = f'({count})'
            bd.text((col_count_x, y + (row_h - FONT_BODY_PX) // 2),
                    count_text, fill=(120, 120, 130), font=font_caption)
        y += row_h

    # Footer: bottom toolbar (with overlap-free button math)
    footer_y = H - 56
    draw_hairline(canvas, (0, footer_y - GAP_SM), W)

    # Left side actions — 3 buttons (添加 / 编辑 / 删除)
    bx = PANEL_PADDING
    draw_button(canvas, bx, footer_y, BTN_W, BTN_H, '添加', icon='+', shortcut='⌘N')
    bx += BTN_W + GAP_LG
    draw_button(canvas, bx, footer_y, BTN_W, BTN_H, '编辑', icon='✎', shortcut='⌘E')
    bx += BTN_W + GAP_LG
    draw_button(canvas, bx, footer_y, BTN_W, BTN_H, '删除', icon='−',
                 shortcut='Del', danger=True, style='destructive')
    left_total_width = PANEL_PADDING + 3 * (BTN_W + GAP_LG) - GAP_LG
    # That is: PANEL_PADDING + 3*BTN_W + 2*GAP_LG (last gap unused)

    # Right side: cancel + save (primary). Math: NO overlap guaranteed.
    save_x = W - PANEL_PADDING - SAVE_W
    cancel_x = save_x - CANCEL_W - GAP_LG
    assert cancel_x >= left_total_width + GAP_LG, \
        f'Overlap! cancel_x={cancel_x} but left_total_width={left_total_width}'
    assert save_x >= cancel_x + CANCEL_W + GAP_LG, \
        f'cancel/save overlap! save_x={save_x} cancel_right={cancel_x + CANCEL_W}'

    draw_button(canvas, cancel_x, footer_y, CANCEL_W, BTN_H, '取消')
    draw_button(canvas, save_x, footer_y, SAVE_W, BTN_H, '保存',
                 style='primary', shortcut='⌘S')

    return canvas


if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    img = make_phrases_dialog()
    out = os.path.join(here, 'phrases-dialog-v2.png')
    img.save(out)
    print(f'wrote {out} ({img.size[0]}x{img.size[1]})')
    # Print button geometry for verification
    left_total = PANEL_PADDING + 3 * BTN_W + 2 * GAP_LG
    save_x = CANVAS_W - PANEL_PADDING - SAVE_W
    cancel_x = save_x - CANCEL_W - GAP_LG
    print(f'  left_total_right_edge = {left_total}')
    print(f'  cancel_x = {cancel_x}')
    print(f'  cancel_right = {cancel_x + CANCEL_W}')
    print(f'  save_x = {save_x}')
    print(f'  left_to_cancel_gap = {cancel_x - left_total}  (>= {GAP_LG} required)')
    print(f'  cancel_to_save_gap = {save_x - (cancel_x + CANCEL_W)}  (>= {GAP_LG} required)')