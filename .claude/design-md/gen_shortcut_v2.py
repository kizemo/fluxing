"""ShortcutSettings v2 mockup — mac-style refined visual draft.

Output: shortcut-settings.png (640 x 460)

Design philosophy: Liquid Discipline (see DESIGN-PHILOSOPHY.md)
Tokens: FLUENT-UI-TOKENS.md §3.1 / §3.2 / §3.4 / §3.5 / §3.6
Frontend-design: typography pairing (monospaced for key combos + CJK serif
  for action labels), conflict = color + icon dual-signal, key-capture
  popover as signature element.
Canvas-design: per-pixel alpha glass, mac Liquid Glass chrome.

This is a SEPARATE script from gen_mockups.py — does NOT modify it.
"""

import os
from PIL import Image, ImageDraw, ImageFont

# PIL on Python 3.14 requires integer coords for primitive drawing.
# Wrap ImageDraw methods to coerce floats/half-pixels to ints once.
_orig_Draw = ImageDraw.Draw

def _round_xy(xy):
    if isinstance(xy, (list, tuple)):
        return type(xy)(_round_xy(v) for v in xy)
    if isinstance(xy, (int, float)):
        return int(round(xy))
    return xy

def _wrap_draw(*args, **kwargs):
    inst = _orig_Draw(*args, **kwargs)
    for name in ('line', 'rectangle', 'ellipse', 'polygon',
                 'rounded_rectangle', 'arc', 'pieslice', 'chord'):
        orig = getattr(inst, name)
        def make(o):
            def w(*a, **kw):
                if a and isinstance(a[0], (list, tuple)):
                    a = (_round_xy(a[0]),) + a[1:]
                if 'width' in kw and isinstance(kw['width'], float):
                    kw['width'] = int(round(kw['width']))
                return o(*a, **kw)
            return w
        setattr(inst, name, make(orig))
    return inst

ImageDraw.Draw = _wrap_draw

# ============================================================
# Tokens (mirror FLUENT-UI-TOKENS.md §3.6 PhrasesDialog family)
# ============================================================
K_BG_TOP        = (245, 245, 248)
K_BG_BOT        = (220, 222, 230)
K_TEXT          = ( 30,  30,  40)
K_TEXT_60       = ( 30,  30,  40, 153)   # secondary
K_TEXT_45       = ( 30,  30,  40, 115)   # tertiary / description
K_TEXT_30       = ( 30,  30,  40,  77)   # hint / disabled
K_SEL_BG        = (255, 235, 220)        # peach selection
K_ACCENT        = (255,  95,  49)        # brand orange (1.4% rule)
K_WARNING_BG    = (255, 230, 200)        # soft warning row tint
K_WARNING_BORDER= (196, 110,  28)        # warning text
K_CONFLICT_BG   = (255, 218, 210)        # conflict row tint (peach + warm)
K_CONFLICT_TXT  = (208,  69,  69)        # destructive
K_SUCCESS       = ( 36, 138,  61)
K_HAIRLINE      = (  0,   0,   0,  22)   # 8% black
K_HAIRLINE_TOP  = (255, 255, 255, 100)
K_SHADOW        = (  0,   0,   0,  30)
K_ICON_DIM      = (130, 130, 140)
K_ICON_ACCENT   = (255,  95,  49)
K_CAPTURE_BG    = (252, 252, 254)
K_POP_SHADOW    = (  0,   0,   0,  60)

# Geometry
W, H             = 640, 540
RADIUS_LG        = 14
RADIUS_MD        = 10
RADIUS_SM        = 6
TITLE_H          = 38
TOOLBAR_H        = 36
HEADER_H         = 28
ROW_H            = 30
BOTTOM_H         = 50
PANEL_PAD        = 14

# Fonts
WIN_FONT = r'C:\Windows\Fonts'

def font(size, weight='regular'):
    if weight == 'bold':
        cands = [
            os.path.join(WIN_FONT, 'msyhbd.ttc'),
            os.path.join(WIN_FONT, 'simsunb.ttf'),
            os.path.join(WIN_FONT, 'segoeuib.ttf'),
            os.path.join(WIN_FONT, 'arialbd.ttf'),
        ]
    elif weight == 'mono':
        # Cascadia Mono / Consolas for key combos
        cands = [
            os.path.join(WIN_FONT, 'CascadiaMono.ttf'),
            os.path.join(WIN_FONT, 'consola.ttf'),
            os.path.join(WIN_FONT, 'consolab.ttf'),
            os.path.join(WIN_FONT, 'lucon.ttf'),
        ]
    elif weight == 'mono-bold':
        cands = [
            os.path.join(WIN_FONT, 'CascadiaCode.ttf'),
            os.path.join(WIN_FONT, 'consolab.ttf'),
            os.path.join(WIN_FONT, 'consolaz.ttf'),
        ]
    else:
        cands = [
            os.path.join(WIN_FONT, 'msyh.ttc'),
            os.path.join(WIN_FONT, 'simsun.ttc'),
            os.path.join(WIN_FONT, 'segoeui.ttf'),
            os.path.join(WIN_FONT, 'arial.ttf'),
        ]
    for p in cands:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                pass
    return ImageFont.load_default()

# ============================================================
# Drawing primitives
# ============================================================

def paste_text_rgba(canvas, xy, text, fnt, fill):
    """Draw text with alpha onto canvas."""
    img = Image.new('RGBA', (max(60, len(text) * fnt.size + 4), fnt.size + 6),
                    (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.text((0, 0), text, fill=fill, font=fnt)
    canvas.paste(img, xy, img)


def draw_glass_chrome(canvas, w, h):
    """Mac Liquid Glass panel: per-pixel alpha gradient, hairline, top highlight."""
    # Drop shadow
    sh = Image.new('RGBA', (w + 14, h + 14), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    for i in range(6):
        sd.rounded_rectangle(
            [i, i, w + 8 + i, h + 8 + i],
            radius=RADIUS_LG + i,
            outline=(0, 0, 0, max(0, K_SHADOW[3] - i * 5)),
            width=1)
    canvas.paste(sh, (-7, -5), sh)

    # Glass body
    glass = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glass)
    for y in range(h):
        t = y / max(1, h - 1)
        r = int(K_BG_TOP[0] * (1 - t) + K_BG_BOT[0] * t)
        g = int(K_BG_TOP[1] * (1 - t) + K_BG_BOT[1] * t)
        b = int(K_BG_TOP[2] * (1 - t) + K_BG_BOT[2] * t)
        a = int(230 + (95 - 230) * t)
        gd.line([(0, y), (w, y)], fill=(r, g, b, a))

    mask = Image.new('L', (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [0, 0, w - 1, h - 1], radius=RADIUS_LG, fill=255)
    canvas.paste(glass, (0, 0), mask)

    bd = ImageDraw.Draw(canvas)
    bd.rounded_rectangle([0, 0, w - 1, h - 1], radius=RADIUS_LG,
                         outline=K_HAIRLINE, width=1)
    # Top inner highlight
    bd.line([(RADIUS_LG, 1), (w - RADIUS_LG, 1)], fill=K_HAIRLINE_TOP, width=1)


def draw_text(d, xy, text, fnt, fill=K_TEXT):
    d.text(xy, text, fill=fill, font=fnt)


def button(canvas, xy, wh, label, style='normal', hint=None):
    x, y = xy
    bw, bh = wh
    d = ImageDraw.Draw(canvas)
    r = RADIUS_SM
    if style == 'primary':
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r, fill=K_ACCENT)
        txt = (255, 255, 255)
        fnt = font(13, 'bold')
    elif style == 'danger':
        # Ghost red border for destructive
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r,
                            fill=(255, 255, 255), outline=K_CONFLICT_TXT, width=1)
        txt = K_CONFLICT_TXT
        fnt = font(13, 'regular')
    elif style == 'ghost':
        # No border, light gray bg for legibility on glass
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r,
                            fill=(255, 255, 255, 200),
                            outline=K_HAIRLINE, width=1)
        txt = (110, 110, 120)
        fnt = font(13, 'regular')
    elif style == 'disabled':
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r,
                            fill=(245, 245, 248), outline=K_HAIRLINE, width=1)
        txt = K_TEXT_30
        fnt = font(13, 'regular')
    else:
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r,
                            fill=(255, 255, 255), outline=K_HAIRLINE, width=1)
        txt = K_TEXT
        fnt = font(13, 'regular')
    bbox = d.textbbox((0, 0), label, font=fnt)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    d.text((x + (bw - tw) // 2, y + (bh - th) // 2 - 2),
           label, fill=txt, font=fnt)
    # Keyboard hint (e.g. ⌘R) — small mono, top-right
    if hint:
        hf = font(10, 'mono')
        hb = d.textbbox((0, 0), hint, font=hf)
        hw = hb[2] - hb[0]
        d.text((x + bw - hw - 8, y + 5), hint,
               fill=K_TEXT_30, font=hf)


def chip(canvas, xy, wh, label, selected=False):
    """Filter chip (selected = peach fill + accent text)."""
    x, y = xy
    bw, bh = wh
    d = ImageDraw.Draw(canvas)
    if selected:
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=bh // 2,
                            fill=K_SEL_BG)
        txt = K_ACCENT
        fnt = font(12, 'bold')
    else:
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=bh // 2,
                            fill=(255, 255, 255), outline=K_HAIRLINE, width=1)
        txt = K_TEXT_60
        fnt = font(12, 'regular')
    bbox = d.textbbox((0, 0), label, font=fnt)
    tw = bbox[2] - bbox[0]
    d.text((x + (bw - tw) // 2, y + (bh - 13) // 2),
           label, fill=txt, font=fnt)


def badge(canvas, xy, label, style='changed'):
    """Inline status badge: 'changed' / 'unchanged' / '冲突'.

    Uses opaque fill colors to avoid alpha-blend artifacts in PIL.
    """
    d = ImageDraw.Draw(canvas)
    fnt = font(10, 'bold')
    bbox = d.textbbox((0, 0), label, font=fnt)
    tw = int(bbox[2] - bbox[0]) + 14
    th = 14
    x, y = xy
    if style == 'changed':
        bg = K_ACCENT
        fg = (255, 255, 255)
    elif style == 'unchanged':
        # Soft gray pill — opaque, distinguishable from row bg
        bg = (228, 228, 234)
        fg = (130, 130, 140)
    elif style == 'conflict':
        bg = K_CONFLICT_TXT
        fg = (255, 255, 255)
    elif style == 'warning':
        bg = K_WARNING_BORDER
        fg = (255, 255, 255)
    else:
        bg = (228, 228, 234)
        fg = K_TEXT_60
    d.rounded_rectangle([x, y, x + tw, y + th], radius=th // 2, fill=bg)
    bbox2 = d.textbbox((0, 0), label, font=fnt)
    bw = bbox2[2] - bbox2[0]
    bh = bbox2[3] - bbox2[1]
    d.text((x + (tw - bw) // 2, y + (th - bh) // 2 - 1),
           label, fill=fg, font=fnt)
    return tw


def hairline(canvas, x, y, length, horizontal=True):
    d = ImageDraw.Draw(canvas)
    if horizontal:
        d.line([(x, y), (x + length, y)], fill=K_HAIRLINE, width=1)
    else:
        d.line([(x, y), (x, y + length)], fill=K_HAIRLINE, width=1)


# ============================================================
# Mockup
# ============================================================

def make_shortcut_settings_v2():
    W, H = 640, 540
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_glass_chrome(canvas, W, H)

    # ----- Title bar (with ⌨ icon + subtitle) -----
    d = ImageDraw.Draw(canvas)
    # ⌨ keyboard glyph
    icon_fnt = font(17, 'regular')
    d.text((PANEL_PAD, (TITLE_H - 17) // 2 + 2), '⌨', fill=K_TEXT, font=icon_fnt)
    # Title (17px bold CJK)
    title_fnt = font(17, 'bold')
    d.text((PANEL_PAD + 26, 9), '快捷键设置', fill=K_TEXT, font=title_fnt)
    # Subtitle
    sub_fnt = font(11, 'regular')
    paste_text_rgba(canvas, (PANEL_PAD + 26, 28),
                    'Customize keyboard shortcuts · 14 bindings',
                    sub_fnt, K_TEXT_45)
    # Close button (X with circle)
    cx, cy = W - PANEL_PAD - 12, TITLE_H // 2 + 1
    d.ellipse([int(cx - 9), int(cy - 9), int(cx + 9), int(cy + 9)],
              outline=K_HAIRLINE, width=1)
    d.line([(int(cx - 4), int(cy - 4)), (int(cx + 4), int(cy + 4))],
           fill=K_TEXT_60, width=1)
    d.line([(int(cx + 4), int(cy - 4)), (int(cx - 4), int(cy + 4))],
           fill=K_TEXT_60, width=1)

    hairline(canvas, 0, TITLE_H, W)
    y = TITLE_H + 8

    # ----- Toolbar row: filter chips + search + schema -----
    # Filter chips (4) on the left
    chip_labels = [('全部', True), ('编辑类', False), ('切换类', False),
                   ('部署类', False)]
    chip_x = PANEL_PAD
    chip_y = y + 4
    for label, sel in chip_labels:
        chip_w = 50 if sel else 56
        chip(canvas, (chip_x, chip_y), (chip_w, 24), label, sel)
        chip_x += chip_w + 6

    # Schema dropdown — right of chips, anchored to right side first
    dd_w = 92
    dd_x = W - PANEL_PAD - dd_w
    dd_y = y + 4
    d.rounded_rectangle([dd_x, dd_y, dd_x + dd_w, dd_y + 24],
                        radius=RADIUS_SM, fill=(255, 255, 255),
                        outline=K_HAIRLINE, width=1)
    paste_text_rgba(canvas, (dd_x + 10, dd_y + 5),
                    'luna_pinyin', font(11, 'mono'), K_TEXT)
    d.text((dd_x + dd_w - 14, dd_y + 8), '▾',
           fill=K_TEXT_45, font=font(10))

    # Search box — between chips and schema
    sb_w = 170
    sb_x = dd_x - sb_w - 8
    sb_y = y + 4
    d.rounded_rectangle([sb_x, sb_y, sb_x + sb_w, sb_y + 24],
                        radius=RADIUS_SM, fill=(255, 255, 255),
                        outline=K_HAIRLINE, width=1)
    # Magnifier glyph
    mx, my = sb_x + 10, sb_y + 12
    d.ellipse([mx - 4, my - 4, mx + 4, my + 4],
              outline=K_ICON_DIM, width=1)
    d.line([(mx + 3, my + 3), (mx + 6, my + 6)],
           fill=K_ICON_DIM, width=1)
    # Placeholder
    paste_text_rgba(canvas, (sb_x + 22, sb_y + 6),
                    '搜索快捷键…', font(11), K_TEXT_30)
    # ⌘F hint
    hf = font(10, 'mono')
    paste_text_rgba(canvas, (sb_x + sb_w - 22, sb_y + 7),
                    '⌘F', hf, K_TEXT_30)

    y += TOOLBAR_H + 2

    # ----- Status row -----
    # Left: status badge
    paste_text_rgba(canvas, (PANEL_PAD, y + 4),
                    '●  未保存 (3 修改)', font(11, 'bold'), K_WARNING_BORDER)
    # Right: conflict count badge
    rb_x = W - PANEL_PAD - 88
    d.rounded_rectangle([rb_x, y + 1, rb_x + 88, y + 21],
                        radius=10, fill=K_CONFLICT_TXT)
    fnt_w = font(11, 'bold')
    d.text((rb_x + 10, y + 4), '⚠  2 个冲突', fill=(255, 255, 255), font=fnt_w)
    y += 24

    hairline(canvas, 0, y, W)
    y += 6

    # ----- Table header -----
    # Columns: action 320 / current 152 / new 130 / (badge column)
    col_x = [PANEL_PAD, PANEL_PAD + 320, PANEL_PAD + 320 + 152]
    col_w = [320, 152, 130]
    headers = [('动作 (Action)', ''),
               ('当前键 (Current)', ''),
               ('新键 (New)', '')]
    hf2 = font(11, 'bold')
    for (x, w), (label, _) in zip(zip(col_x, col_w), headers):
        d.text((x, y), label, fill=K_TEXT_60, font=hf2)
    # Sort indicator on '新键'
    sort_x = col_x[2] + d.textlength('新键 (New)', font=hf2) + 4
    d.text((sort_x, y), '↑', fill=K_ACCENT, font=font(11, 'bold'))
    y += HEADER_H
    hairline(canvas, 0, y, W)
    y += 4

    # ----- Rows -----
    # action, current, new, sel, conflict, changed, state
    rows = [
        ('切换中英文',   'Ctrl+Shift+Space', 'Ctrl+Shift+Space', False, False, False),
        ('打开 / 关闭设置栏', 'Ctrl+Shift+`,', 'Ctrl+Shift+S',   False, False, True),
        ('切换全 / 半角', 'Shift+Space',     'Shift+Space',     False, True,  False),
        ('常用短语',     'Alt+.',           'Alt+.',           False, False, False),
        ('重选候选',     'Ctrl+Shift+R',    'Shift+L',         False, False, True),
        ('第二候选',     'Shift+Tab',       'Shift+Tab',       False, False, False),
        ('翻页 (候选上)', 'Page Up',         'Page Up',         False, False, False),
        ('翻页 (候选下)', 'Page Down',       'Page Down',       False, False, False),
        ('立即验证',     'Ctrl+F7',         'Ctrl+F7',         False, False, False),
        ('重新部署',     'Ctrl+F5',         'Ctrl+F5',         False, False, False),
    ]
    action_fnt = font(13, 'regular')
    action_desc_fnt = font(10, 'regular')
    combo_fnt = font(12, 'mono')
    combo_b_fnt = font(12, 'mono-bold')

    for i, (action, current, new, sel, conflict, changed) in enumerate(rows):
        # Row background
        bg = None
        if conflict:
            bg = K_CONFLICT_BG
        elif changed:
            bg = (255, 248, 240)  # very faint peach tint to signal "editable"
        if bg:
            d.rectangle([8, y, W - 8, y + ROW_H - 2], fill=bg)
            if conflict:
                # Left red bar
                d.rectangle([8, y, 11, y + ROW_H - 2], fill=K_CONFLICT_TXT)

        # Action cell: 17px bold CJK + 11px regular description
        d.text((col_x[0], y + 4), action, fill=K_TEXT, font=action_fnt)
        # Description (English / hint)
        descs = {
            '切换中英文': 'toggle: ascii_mode',
            '打开 / 关闭设置栏': 'show QuickPanel',
            '切换全 / 半角': 'toggle: full_shape',
            '常用短语': 'open phrases dialog',
            '重选候选': 'reselect candidate',
            '第二候选': 'select second',
            '翻页 (候选上)': 'page up',
            '翻页 (候选下)': 'page down',
            '立即验证': 'verify hotkey',
            '重新部署': 'trigger /deploy',
        }
        desc = descs.get(action, '')
        paste_text_rgba(canvas, (col_x[0], y + 18), desc,
                        action_desc_fnt, K_TEXT_45)

        # Current key (mono, gray, strike-through if changed)
        cur_x = col_x[1]
        if changed:
            # Strike-through current
            tw = d.textlength(current, font=combo_fnt)
            d.text((cur_x, y + 9), current, fill=K_TEXT_30, font=combo_fnt)
            d.line([(cur_x - 1, y + 15), (cur_x + tw + 1, y + 15)],
                   fill=K_TEXT_30, width=1)
        else:
            d.text((cur_x, y + 9), current,
                   fill=K_CONFLICT_TXT if conflict else K_TEXT_45,
                   font=combo_b_fnt if conflict else combo_fnt)

        # New key (mono, accent if changed, conflict color if conflict)
        new_x = col_x[2]
        new_color = K_TEXT
        new_fnt_use = combo_fnt
        if conflict:
            new_color = K_CONFLICT_TXT
            new_fnt_use = combo_b_fnt
        elif changed:
            new_color = K_ACCENT
            new_fnt_use = combo_b_fnt
        d.text((new_x, y + 9), new, fill=new_color, font=new_fnt_use)

        # Badge after new key
        bx = new_x + int(d.textlength(new, font=new_fnt_use)) + 8
        if conflict:
            badge(canvas, (bx, y + 11), '⚠ 冲突', 'conflict')
        elif changed:
            badge(canvas, (bx, y + 11), '已修改 ⮕', 'changed')
        else:
            badge(canvas, (bx, y + 11), '• unchanged', 'unchanged')

        y += ROW_H

    hairline(canvas, 0, y, W)

    # ----- Bottom toolbar (separated) -----
    bottom_y = H - BOTTOM_H
    hairline(canvas, 0, bottom_y, W)

    # Right side: primary actions
    # Custom (left of group)
    bx = PANEL_PAD
    button(canvas, (bx, bottom_y + 10), (90, 32), '+ 自定义', 'ghost')
    bx += 90 + 8
    button(canvas, (bx, bottom_y + 10), (90, 32),
           '⟳ 恢复默认', 'danger', hint='⌘R')
    bx += 90 + 8
    button(canvas, (bx, bottom_y + 10), (76, 32), '导入', 'ghost')
    bx += 76 + 8
    button(canvas, (bx, bottom_y + 10), (76, 32), '导出', 'ghost')

    # Right-aligned: Cancel + Save (Save = primary, orange)
    save_w = 90
    cancel_w = 80
    save_x = W - PANEL_PAD - save_w
    cancel_x = save_x - cancel_w - 8
    # "无修改" tooltip / disabled indicator — we'll show Save ENABLED
    # because there ARE changes (3 modified per status bar)
    button(canvas, (cancel_x, bottom_y + 10), (cancel_w, 32),
           '取消', 'ghost')
    button(canvas, (save_x, bottom_y + 10), (save_w, 32),
           '保存', 'primary')

    # ----- Signature element: Key capture popover -----
    # Anchored to row index 4 ("重选候选" = capturing state), floats above it
    pop_w, pop_h = 260, 100
    # Compute row index 4 vertical center (y currently points PAST last row)
    row4_y = y - (len(rows) - 4) * ROW_H
    pop_x = W - pop_w - PANEL_PAD - 8  # right-anchored, never overflows
    pop_y = row4_y - pop_h - 8
    if pop_y < TITLE_H + TOOLBAR_H + 30:
        pop_y = row4_y + ROW_H + 4  # flip below if no room above
    # Shadow first
    sh = Image.new('RGBA', (pop_w + 16, pop_h + 16), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    sd.rounded_rectangle([4, 4, pop_w + 4, pop_h + 4],
                         radius=RADIUS_MD, fill=K_POP_SHADOW)
    canvas.paste(sh, (pop_x - 8, pop_y - 6), sh)

    # Card body (solid white, opaque — unlike the glass behind)
    d.rounded_rectangle([pop_x, pop_y, pop_x + pop_w, pop_y + pop_h],
                        radius=RADIUS_MD, fill=K_CAPTURE_BG,
                        outline=K_HAIRLINE, width=1)
    # Top accent stripe (orange) — signature 1px bar
    d.rectangle([pop_x, pop_y, pop_x + pop_w, pop_y + 2],
                fill=K_ACCENT)

    # Prompt
    paste_text_rgba(canvas, (pop_x + 14, pop_y + 12),
                    '请按快捷键…', font(12, 'bold'), K_TEXT)
    paste_text_rgba(canvas, (pop_x + pop_w - 60, pop_y + 12),
                    'Esc 取消', font(10, 'mono'), K_TEXT_45)

    # Live display — large dashed-look combo box
    cb_x, cb_y = pop_x + 14, pop_y + 36
    cb_w, cb_h = pop_w - 28, 36
    # 8px dashed border (rendered as repeating dashes)
    d.rounded_rectangle([cb_x, cb_y, cb_x + cb_w, cb_y + cb_h],
                        radius=RADIUS_SM, fill=(255, 255, 255),
                        outline=K_ACCENT, width=2)
    # Show captured combo (with modifier sorted: Ctrl < Shift)
    paste_text_rgba(canvas, (cb_x + 12, cb_y + 9),
                    'Ctrl + Shift + L', font(16, 'mono-bold'), K_ACCENT)
    # Blinking caret (simulated as thin orange line)
    d.rectangle([cb_x + cb_w - 16, cb_y + 8, cb_x + cb_w - 14, cb_y + cb_h - 8],
                fill=K_ACCENT)

    # Conflict warning line (live detection)
    paste_text_rgba(canvas, (cb_x, cb_y + cb_h + 8),
                    '⚠  与「打开 / 关闭设置栏」冲突 · 覆盖会禁用该快捷键',
                    font(10), K_CONFLICT_TXT)

    # Pointer tail (small triangle) — anchored to the row below
    tail_x = pop_x + 90
    tail_y = pop_y + pop_h
    d.polygon([(tail_x - 6, tail_y), (tail_x + 6, tail_y),
               (tail_x, tail_y + 6)], fill=K_CAPTURE_BG)
    # Cover the top edge of the triangle where it meets the card
    d.line([(tail_x - 6, tail_y + 1), (tail_x + 6, tail_y + 1)],
           fill=K_CAPTURE_BG, width=1)

    return canvas


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, 'shortcut-settings.png')
    img = make_shortcut_settings_v2()
    img.save(out)
    print(f'  wrote {out} ({img.size[0]}x{img.size[1]})')


if __name__ == '__main__':
    main()