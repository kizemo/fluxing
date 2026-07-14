"""Improve PhrasesDialog v2 mockup to match user-dictionary design language.

Output: .claude/design-md/phrases-dialog-v2.png (480×460, signature visual upgrade)

Design system follows DESIGN-PHILOSOPHY.md (Liquid Discipline) and the tokens
established in user-dictionary.png / gen_mockups.py — title bar with book glyph
+ bilingual caption, macOS-style search field + ⌘F hint, status pill chips,
finder-like tree with chevron disclosures, selected/hover row treatment,
soft hairlines, per-pixel-alpha glass chrome.

States rendered:
  primary canvas (state 1 — Browsing, selected phrase)
  ─ plus 3 supporting states saved as phrases-dialog-v2-{search,edit,empty}.png
    so reviewers can compare the family in one glance.
"""
import os
from PIL import Image, ImageDraw, ImageFont

# ============================================================
# Token system (mirror FLUENT-UI-TOKENS.md §3.6 + user-dictionary style)
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

FONT_TITLE_PX  = 17
FONT_BODY_PX   = 13
FONT_CAPTION_PX= 11
FONT_BTN_PX    = 13
TITLE_H        = 44
GAP_XS         = 4
GAP_SM         = 8
GAP_MD         = 11
GAP_LG         = 14
GAP_XL         = 16
PANEL_PADDING  = 14

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
# Drawing helpers (mirror style of user-dictionary, no shared imports)
# ============================================================
def draw_panel_chrome(canvas, w, h):
    """Liquid Glass chrome — rounded rect, gradient, hairline, top highlight."""
    # Drop-shadow (soft offset rectangle layer — macOS panel style)
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
    # Glass body with vertical gradient + per-pixel alpha
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
    # Hairline border
    bd.rounded_rectangle([0, 0, w - 1, h - 1], radius=RADIUS_LG,
                         outline=K_HAIRLINE, width=1)
    # Glass top highlight
    bd.line([(RADIUS_LG, 1), (w - RADIUS_LG, 1)], fill=K_HAIRLINE_TOP, width=1)


def draw_title_bar(canvas, w, title_cn, subtitle_en, icon_kind='folder'):
    """Bespoke title bar — book/folder glyph + bilingual identity + close ×."""
    bd = ImageDraw.Draw(canvas)
    font_title  = load_font(FONT_TITLE_PX, 'bold')
    font_caption = load_font(FONT_CAPTION_PX, 'regular')

    # Leading icon (12x12 rounded square + accent spine + glyph strokes)
    icon_x, icon_y = 14, 13
    bd.rounded_rectangle([icon_x, icon_y, icon_x + 13, icon_y + 12], radius=2,
                         fill=(255, 255, 255, 130),
                         outline=(118, 118, 128, 130), width=1)
    if icon_kind == 'folder':
        # Faint folder tab line
        bd.line([(icon_x + 2, icon_y + 3), (icon_x + 7, icon_y + 3)],
                fill=K_ICON_DIM, width=1)
        bd.line([(icon_x + 4, icon_y + 5), (icon_x + 12, icon_y + 5)],
                fill=K_ICON_DIM, width=1)
        bd.line([(icon_x + 4, icon_y + 7), (icon_x + 11, icon_y + 7)],
                fill=K_ICON_DIM, width=1)
    elif icon_kind == 'edit':
        bd.line([(icon_x + 2, icon_y + 10), (icon_x + 12, icon_y + 2)],
                fill=K_ACCENT, width=1)
        bd.polygon([(icon_x + 12, icon_y + 2),
                    (icon_x + 14, icon_y + 1),
                    (icon_x + 13, icon_y + 3)], fill=K_ACCENT)
    elif icon_kind == 'search':
        bd.ellipse([icon_x + 1, icon_y + 1, icon_x + 9, icon_y + 9],
                   outline=K_ICON_DIM, width=1)
        bd.line([(icon_x + 8, icon_y + 8), (icon_x + 12, icon_y + 12)],
                fill=K_ICON_DIM, width=2)

    bd.text((34, 6), title_cn, fill=K_TEXT, font=font_title)
    # subtitle: vertically aligned with the title's baseline (centered vertically)
    bd.text((34, 24),
            subtitle_en, fill=(110, 110, 120), font=font_caption)

    # Close × (drawn last, top-right) — vertically centered vs the title row
    cx, cy = w - 22, 18
    bd.ellipse([cx - 5, cy - 5, cx + 11, cy + 11],
               fill=(255, 255, 255, 70))
    bd.line([(cx + 1, cy + 1), (cx + 7, cy + 7)], fill=K_ICON_TEXT, width=1)
    bd.line([(cx + 7, cy + 1), (cx + 1, cy + 7)], fill=K_ICON_TEXT, width=1)


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
    # Magnifier glyph (with subtle stroke)
    cx, cy = x + 14, y + h // 2
    bd.ellipse([cx - 4, cy - 4, cx + 5, cy + 5],
               outline=(110, 110, 120), width=1)
    bd.line([(cx + 4, cy + 4), (cx + 8, cy + 8)],
            fill=(110, 110, 120), width=1)
    # ⌘F hint chip on the right edge of the search field
    if hint:
        chip_w = 36
        cx0 = x + w - chip_w - 4
        bd.rounded_rectangle([cx0, y + (h - 18) // 2,
                              cx0 + chip_w, y + (h - 18) // 2 + 18], radius=RADIUS_XS,
                             fill=(238, 238, 242),
                             outline=(0, 0, 0, 16), width=1)
        bd.text((cx0 + 6, y + (h - 18) // 2 + 1),
                '⌘F', fill=(112, 112, 122), font=load_font(FONT_CAPTION_PX, 'regular'))
        text_area_w = w - chip_w - 36
    else:
        text_area_w = w - 36
    # Text inside
    font_body = load_font(FONT_BODY_PX, 'regular')
    tx = x + 26
    ty = y + (h - FONT_BODY_PX) // 2 - 1
    if query:
        bd.text((tx, ty), query, fill=K_TEXT, font=font_body)
        # trailing clear × inside text area
        bd.text((tx + 8 * len(query) + FONT_BODY_PX, ty - 1),
                '✕', fill=(150, 150, 158), font=load_font(FONT_BODY_PX, 'regular'))
    elif hint:
        bd.text((tx, ty), '搜索短语...', fill=K_TEXT_50, font=font_body)


def draw_chip(canvas, x, y, w, h, label, dot_color=None, tail=None, fill=None):
    """Generic pill chip — white translucent fill + caption text + optional dot."""
    bd = ImageDraw.Draw(canvas)
    if fill is None:
        fill = (255, 255, 255, 178)
    bd.rounded_rectangle([x, y, x + w, y + h], radius=h // 2,
                         fill=fill, outline=(0, 0, 0, 18), width=1)
    tx = x + 10
    if dot_color:
        bd.ellipse([x + 10, y + (h - 8) // 2, x + 18, y + (h - 8) // 2 + 8],
                   fill=dot_color)
        tx = x + 24
    bd.text((tx, y + (h - FONT_CAPTION_PX) // 2 - 1), label,
            fill=(62, 62, 72), font=load_font(FONT_CAPTION_PX, 'regular'))
    if tail:
        bd.text((x + w - 14, y + (h - FONT_CAPTION_PX) // 2 - 1),
                tail, fill=(110, 110, 120), font=load_font(FONT_CAPTION_PX, 'regular'))


def draw_chevron(d, x, y, expanded, color=K_ICON_DIM, size=8):
    """Mac Finder-style chevron: rotated 90° between expanded/collapsed."""
    pts = [(x, y),
           (x + size, y + size // 2),
           (x, y + size)] if not expanded else [
           (x, y),
           (x + size // 2, y + size),
           (x + size, y)]
    d.line(pts + [pts[0]], fill=color, width=1)


def draw_folder_glyph(d, x, y, accent=False, size=12):
    """Tiny folder icon — tab + body."""
    body = (118, 118, 128) if not accent else K_ACCENT
    # tab
    d.line([(x + 1, y + 3), (x + 5, y + 3)], fill=body, width=1)
    # body
    d.line([(x + 0, y + 5), (x + size, y + 5)], fill=body, width=1)
    d.line([(x + 0, y + 5), (x + 0, y + size - 1)], fill=body, width=1)
    d.line([(x + size, y + 5), (x + size, y + size - 1)], fill=body, width=1)
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
    # pre-measure label + icon width to center them and reserve room for shortcut chip
    icon_gap = 4 if icon else 0
    icon_w = FONT_BTN_PX if icon else 0
    bbox_l = bd.textbbox((0, 0), label, font=font_btn)
    label_w = bbox_l[2] - bbox_l[0]
    total_content_w = icon_w + icon_gap + label_w
    shortcut_w = 0
    if shortcut:
        bbox_s = bd.textbbox((0, 0), shortcut, font=load_font(FONT_CAPTION_PX, 'regular'))
        shortcut_w = bbox_s[2] - bbox_s[0] + 14
    # place label centered within (total_w - shortcut_w)
    usable_w = bw - shortcut_w
    cursor_x = x + (usable_w - total_content_w) // 2
    # icon
    if icon:
        bd.text((cursor_x, y + (bh - FONT_BTN_PX) // 2 - 1), icon,
                fill=label_fill, font=font_btn)
        cursor_x += icon_w + icon_gap
    # label
    bd.text((cursor_x, y + (bh - FONT_BTN_PX) // 2 - 1),
            label, fill=label_fill, font=font_btn)
    # shortcut chip (right side)
    if shortcut:
        bd.text((x + bw - shortcut_w + 4, y + (bh - FONT_CAPTION_PX) // 2 + 1),
                shortcut, fill=(118, 118, 128, 200) if style != 'primary' else (255, 235, 220, 220),
                font=load_font(FONT_CAPTION_PX, 'regular'))


# ============================================================
# Primary state — Browsing, with row selection
# ============================================================
def make_phrases_dialog():
    W, H = 480, 500
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_panel_chrome(canvas, W, H)
    draw_title_bar(canvas, W, '常用短语',
                   'Manage text snippets · 12 entries', icon_kind='folder')
    bd = ImageDraw.Draw(canvas)

    # Toolbar row — search | chips on the right
    toolbar_y = 40
    draw_search_field(canvas, PANEL_PADDING, toolbar_y, 220, 30)
    # status pills (right-aligned)
    pill_x = PANEL_PADDING + 232
    pill_h = 26
    draw_chip(canvas, pill_x, toolbar_y + 2, 130, pill_h,
              '已部署 · 3 分钟前', dot_color=K_GREEN_DOT)
    draw_chip(canvas, pill_x + 138, toolbar_y + 2, 96, pill_h,
              '按字母排序', tail='▾')
    # Bottom-row split separator
    draw_hairline(canvas, (0, toolbar_y + 38), W)

    # Tree header band
    tree_top = toolbar_y + 46
    bd.rounded_rectangle([8, tree_top, W - 8, tree_top + 22], radius=RADIUS_SM,
                         fill=(255, 255, 255, 80), outline=(0, 0, 0, 14), width=1)
    bd.rectangle([8, tree_top + 11, W - 8, tree_top + 22], fill=(255, 255, 255, 80))
    # Column header labels — clamped inside the panel padding
    col_shortcut_x = W - PANEL_PADDING - 110   # column for shortcut codes
    col_category_x = W - PANEL_PADDING - 68    # column for category names
    col_count_x    = W - PANEL_PADDING - 28    # column for count badges
    bd.text((PANEL_PADDING + 4, tree_top + 4), '短语',
            fill=(92, 92, 103), font=load_font(FONT_BODY_PX, 'bold'))
    bd.text((PANEL_PADDING + 50, tree_top + 6), '▼',
            fill=K_ACCENT, font=load_font(FONT_CAPTION_PX, 'regular'))
    bd.text((col_shortcut_x, tree_top + 4), '快捷键', fill=(92, 92, 103),
            font=load_font(FONT_BODY_PX, 'bold'))
    bd.text((col_category_x, tree_top + 5), '分类', fill=(92, 92, 103),
            font=load_font(FONT_BODY_PX, 'bold'))
    bd.text((col_count_x, tree_top + 5), '计数', fill=(92, 92, 103),
            font=load_font(FONT_BODY_PX, 'bold'))

    # Tree rows
    tree_left = 8
    tree_right = W - 8
    tree_x0 = 18
    row_h = 24
    font_body = load_font(FONT_BODY_PX, 'regular')
    font_body_b = load_font(FONT_BODY_PX, 'bold')
    font_caption = load_font(FONT_CAPTION_PX, 'regular')
    font_mono = load_font(FONT_BODY_PX, 'regular')   # fallback (no DMMono)

    # 展开: 工作 (3)
    #   你好                    nir     工作       3  ★ selected
    #   谢谢                    tx      工作
    #   期待合作                 qzhz    工作
    # 折叠: 日常 (2)             ▸
    # 折叠: 编辑类 (1)           ▸
    # ▼ 自定义 (3)              gund
    #   ⭐ Hello                h
    #   ⭐ 测试短语             csc
    #   ⭐ 签名档              qmd
    rows = [
        # (kind, indent, text, shortcut, category, count, accent)
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
    y = tree_top + 28
    selected_row_y = None
    for kind, indent, text, shortcut, cat_name, count, _accent in rows:
        is_cat = kind.startswith('cat')
        expanded = kind == 'cat-expanded'
        is_selected = kind == 'phrase-selected'
        row_bg = [tree_left + 1, y - 2, tree_right - 1, y + row_h - 2]
        if is_selected:
            # peach selection
            bd.rectangle([tree_left + 1, y - 2, tree_right - 1, y + row_h - 2],
                         fill=K_SEL_BG)
            bd.rectangle([tree_left + 1, y - 2, tree_left + 4, y + row_h - 2],
                         fill=K_ACCENT)
            selected_row_y = y
        # chevron (only for category rows; small offset so folder glyph is beside it)
        chev_x = tree_x0 + indent * 14
        if is_cat:
            draw_chevron(bd, chev_x, y + row_h // 2 - 4, expanded)
        # folder icon for category rows
        icon_x = chev_x + (16 if is_cat else 0)
        if is_cat:
            draw_folder_glyph(bd, icon_x, y + row_h // 2 - 6, accent=False, size=12)
            text_x = icon_x + 18
        else:
            text_x = icon_x + 4
        # label
        label = text
        label_font = font_body_b if is_cat or is_selected else font_body
        bd.text((text_x, y + (row_h - FONT_BODY_PX) // 2),
                label, fill=K_TEXT, font=label_font)
        # data columns — match header x-positions above
        if not is_cat:
            bd.text((col_shortcut_x, y + (row_h - FONT_BODY_PX) // 2),
                    shortcut or '', fill=(110, 110, 120), font=font_mono)
            bd.text((col_category_x, y + (row_h - FONT_BODY_PX) // 2),
                    cat_name or '', fill=(110, 110, 120), font=font_body)
        else:
            count_text = f'({count})'
            bd.text((col_count_x, y + (row_h - FONT_BODY_PX) // 2),
                    count_text, fill=(120, 120, 130), font=font_caption)
        # divider between rows (skip if next row is first in 'before selected')
        y += row_h

    # Footer: bottom toolbar
    footer_y = H - 46
    draw_hairline(canvas, (0, footer_y - 6), W)
    # left side actions — wider buttons so icon + shortcut hint don't collide
    bx = PANEL_PADDING
    btn_h = 30
    draw_button(canvas, bx, footer_y, 92, btn_h, '添加', icon='+', shortcut='⌘N')
    bx += 100
    draw_button(canvas, bx, footer_y, 92, btn_h, '编辑', icon='✎', shortcut='⌘E')
    bx += 100
    draw_button(canvas, bx, footer_y, 92, btn_h, '删除', icon='−',
                 shortcut='Del', danger=True, style='destructive')

    # right side: cancel + save (primary)
    save_x = W - PANEL_PADDING - 100
    cancel_x = save_x - 80
    draw_button(canvas, cancel_x, footer_y, 72, btn_h, '取消')
    draw_button(canvas, save_x, footer_y, 100, btn_h, '保存',
                 style='primary', shortcut='⌘S')

    return canvas


if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    img = make_phrases_dialog()
    out = os.path.join(here, 'phrases-dialog-v2.png')
    img.save(out)
    print(f'wrote {out} ({img.size[0]}x{img.size[1]})')
