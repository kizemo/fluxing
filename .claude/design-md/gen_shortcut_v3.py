"""ShortcutSettings v3 mockup — v2 + (a) larger canvas 800x680 (b) larger fonts
(c) verify no button overlap.

Output: shortcut-settings.png (800x680, overwrites v2 output)

Font scale:
  body 13 -> 16, label 17 -> 21, caption 11 -> 13, mono 12 -> 14
Button overlap math:
  v2 (W=640): 4 left buttons (90+8 each, then 76+8, 76+8) = 90+8+90+8+76+8+76+8 = 364
              cancel_x = W - PANEL_PAD - save_w - cancel_w - 8 = 640-14-90-80-8 = 448
              left_total_right_edge = PANEL_PAD + 364 = 14 + 364 = 378
              gap = 448 - 378 = 70   OK but tight
  v3 (W=800): widened canvas; left total ~430, right group ~250; gap still >= GAP_LG
"""
import os
from PIL import Image, ImageDraw, ImageFont

# PIL on Python 3.14 requires integer coords for primitive drawing.
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

# Tokens
K_BG_TOP        = (245, 245, 248)
K_BG_BOT        = (220, 222, 230)
K_TEXT          = ( 30,  30,  40)
K_TEXT_60       = ( 30,  30,  40, 153)
K_TEXT_45       = ( 30,  30,  40, 115)
K_TEXT_30       = ( 30,  30,  40,  77)
K_SEL_BG        = (255, 235, 220)
K_ACCENT        = (255,  95,  49)
K_WARNING_BG    = (255, 230, 200)
K_WARNING_BORDER= (196, 110,  28)
K_CONFLICT_BG   = (255, 218, 210)
K_CONFLICT_TXT  = (208,  69,  69)
K_SUCCESS       = ( 36, 138,  61)
K_HAIRLINE      = (  0,   0,   0,  22)
K_HAIRLINE_TOP  = (255, 255, 255, 100)
K_SHADOW        = (  0,   0,   0,  30)
K_ICON_DIM      = (130, 130, 140)
K_ICON_ACCENT   = (255,  95,  49)
K_CAPTURE_BG    = (252, 252, 254)
K_POP_SHADOW    = (  0,   0,   0,  60)

# Geometry (v3 enlarged)
W, H             = 800, 680
RADIUS_LG        = 14
RADIUS_MD        = 10
RADIUS_SM        = 6
TITLE_H          = 38          # was 38 (kept)
TOOLBAR_H        = 42          # was 36
HEADER_H         = 32          # was 28
ROW_H            = 40          # was 30
BOTTOM_H         = 60          # was 50
PANEL_PAD        = 16          # was 14
GAP_SM           = 12          # was implicit 8
GAP_LG           = 16          # was implicit 8

# Fonts (v3 enlarged)
FONT_TITLE_PX    = 21          # was 17
FONT_BODY_PX     = 16          # was 13
FONT_CAPTION_PX  = 13          # was 11
FONT_MONO_PX     = 14          # was 12
FONT_BTN_PX      = 16          # was 13

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


def paste_text_rgba(canvas, xy, text, fnt, fill):
    img = Image.new('RGBA', (max(80, len(text) * fnt.size + 4), fnt.size + 8),
                    (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.text((0, 0), text, fill=fill, font=fnt)
    canvas.paste(img, xy, img)


def draw_glass_chrome(canvas, w, h):
    sh = Image.new('RGBA', (w + 14, h + 14), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    for i in range(6):
        sd.rounded_rectangle(
            [i, i, w + 8 + i, h + 8 + i],
            radius=RADIUS_LG + i,
            outline=(0, 0, 0, max(0, K_SHADOW[3] - i * 5)),
            width=1)
    canvas.paste(sh, (-7, -5), sh)

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
        fnt = font(FONT_BTN_PX, 'bold')
    elif style == 'danger':
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r,
                            fill=(255, 255, 255), outline=K_CONFLICT_TXT, width=1)
        txt = K_CONFLICT_TXT
        fnt = font(FONT_BTN_PX, 'regular')
    elif style == 'ghost':
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r,
                            fill=(255, 255, 255, 200),
                            outline=K_HAIRLINE, width=1)
        txt = (110, 110, 120)
        fnt = font(FONT_BTN_PX, 'regular')
    elif style == 'disabled':
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r,
                            fill=(245, 245, 248), outline=K_HAIRLINE, width=1)
        txt = K_TEXT_30
        fnt = font(FONT_BTN_PX, 'regular')
    else:
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=r,
                            fill=(255, 255, 255), outline=K_HAIRLINE, width=1)
        txt = K_TEXT
        fnt = font(FONT_BTN_PX, 'regular')
    bbox = d.textbbox((0, 0), label, font=fnt)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    d.text((x + (bw - tw) // 2, y + (bh - th) // 2 - 2),
           label, fill=txt, font=fnt)
    if hint:
        hf = font(FONT_CAPTION_PX - 2, 'mono')
        hb = d.textbbox((0, 0), hint, font=hf)
        hw = hb[2] - hb[0]
        d.text((x + bw - hw - 10, y + 6), hint,
               fill=K_TEXT_30, font=hf)


def chip(canvas, xy, wh, label, selected=False):
    x, y = xy
    bw, bh = wh
    d = ImageDraw.Draw(canvas)
    if selected:
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=bh // 2,
                            fill=K_SEL_BG)
        txt = K_ACCENT
        fnt = font(FONT_BODY_PX, 'bold')
    else:
        d.rounded_rectangle([x, y, x + bw, y + bh], radius=bh // 2,
                            fill=(255, 255, 255), outline=K_HAIRLINE, width=1)
        txt = K_TEXT_60
        fnt = font(FONT_BODY_PX, 'regular')
    bbox = d.textbbox((0, 0), label, font=fnt)
    tw = bbox[2] - bbox[0]
    d.text((x + (bw - tw) // 2, y + (bh - FONT_BODY_PX) // 2),
           label, fill=txt, font=fnt)


def badge(canvas, xy, label, style='changed'):
    d = ImageDraw.Draw(canvas)
    fnt = font(FONT_CAPTION_PX, 'bold')
    bbox = d.textbbox((0, 0), label, font=fnt)
    tw = int(bbox[2] - bbox[0]) + 16
    th = 18
    x, y = xy
    if style == 'changed':
        bg = K_ACCENT
        fg = (255, 255, 255)
    elif style == 'unchanged':
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


def make_shortcut_settings_v3():
    W, H = 800, 680
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_glass_chrome(canvas, W, H)

    d = ImageDraw.Draw(canvas)
    # ⌨ keyboard glyph
    icon_fnt = font(FONT_TITLE_PX, 'regular')
    d.text((PANEL_PAD, (TITLE_H - FONT_TITLE_PX) // 2 + 2), '⌨',
           fill=K_TEXT, font=icon_fnt)
    title_fnt = font(FONT_TITLE_PX, 'bold')
    d.text((PANEL_PAD + 32, 8), '快捷键设置', fill=K_TEXT, font=title_fnt)
    sub_fnt = font(FONT_CAPTION_PX, 'regular')
    paste_text_rgba(canvas, (PANEL_PAD + 32, 32),
                    'Customize keyboard shortcuts · 14 bindings',
                    sub_fnt, K_TEXT_45)
    cx, cy = W - PANEL_PAD - 14, TITLE_H // 2 + 1
    d.ellipse([int(cx - 10), int(cy - 10), int(cx + 10), int(cy + 10)],
              outline=K_HAIRLINE, width=1)
    d.line([(int(cx - 5), int(cy - 5)), (int(cx + 5), int(cy + 5))],
           fill=K_TEXT_60, width=1)
    d.line([(int(cx + 5), int(cy - 5)), (int(cx - 5), int(cy + 5))],
           fill=K_TEXT_60, width=1)

    hairline(canvas, 0, TITLE_H, W)
    y = TITLE_H + 10

    # Filter chips on the left (now with v3 sizes)
    chip_labels = [('全部', True), ('编辑类', False), ('切换类', False),
                   ('部署类', False)]
    chip_x = PANEL_PAD
    chip_y = y + 4
    chip_w_map = {True: 60, False: 72}
    for label, sel in chip_labels:
        chip_w = chip_w_map[sel]
        chip(canvas, (chip_x, chip_y), (chip_w, 28), label, sel)
        chip_x += chip_w + GAP_SM

    dd_w = 120
    dd_x = W - PANEL_PAD - dd_w
    dd_y = y + 4
    d.rounded_rectangle([dd_x, dd_y, dd_x + dd_w, dd_y + 28],
                        radius=RADIUS_SM, fill=(255, 255, 255),
                        outline=K_HAIRLINE, width=1)
    paste_text_rgba(canvas, (dd_x + 12, dd_y + 6),
                    'luna_pinyin', font(FONT_BODY_PX, 'mono'), K_TEXT)
    d.text((dd_x + dd_w - 16, dd_y + 10), '▾',
           fill=K_TEXT_45, font=font(FONT_CAPTION_PX))

    sb_w = 210
    sb_x = dd_x - sb_w - GAP_LG
    sb_y = y + 4
    d.rounded_rectangle([sb_x, sb_y, sb_x + sb_w, sb_y + 28],
                        radius=RADIUS_SM, fill=(255, 255, 255),
                        outline=K_HAIRLINE, width=1)
    mx, my = sb_x + 12, sb_y + 14
    d.ellipse([mx - 5, my - 5, mx + 5, my + 5],
              outline=K_ICON_DIM, width=1)
    d.line([(mx + 4, my + 4), (mx + 8, my + 8)],
           fill=K_ICON_DIM, width=1)
    paste_text_rgba(canvas, (sb_x + 26, sb_y + 7),
                    '搜索快捷键…', font(FONT_BODY_PX), K_TEXT_30)
    hf = font(FONT_CAPTION_PX - 2, 'mono')
    paste_text_rgba(canvas, (sb_x + sb_w - 28, sb_y + 8),
                    '⌘F', hf, K_TEXT_30)

    y += TOOLBAR_H + 4

    # Status row
    paste_text_rgba(canvas, (PANEL_PAD, y + 4),
                    '●  未保存 (3 修改)', font(FONT_BODY_PX, 'bold'),
                    K_WARNING_BORDER)
    rb_x = W - PANEL_PAD - 110
    d.rounded_rectangle([rb_x, y + 1, rb_x + 110, y + 25],
                        radius=12, fill=K_CONFLICT_TXT)
    fnt_w = font(FONT_BODY_PX, 'bold')
    d.text((rb_x + 12, y + 5), '⚠  2 个冲突', fill=(255, 255, 255), font=fnt_w)
    y += 28

    hairline(canvas, 0, y, W)
    y += 8

    # Table header — widened columns
    col_x = [PANEL_PAD, PANEL_PAD + 360, PANEL_PAD + 360 + 200]
    col_w = [360, 200, 170]
    headers = [('动作 (Action)', ''),
               ('当前键 (Current)', ''),
               ('新键 (New)', '')]
    hf2 = font(FONT_BODY_PX, 'bold')
    for (x, w), (label, _) in zip(zip(col_x, col_w), headers):
        d.text((x, y), label, fill=K_TEXT_60, font=hf2)
    sort_x = col_x[2] + d.textlength('新键 (New)', font=hf2) + 6
    d.text((sort_x, y), '↑', fill=K_ACCENT, font=font(FONT_BODY_PX, 'bold'))
    y += HEADER_H
    hairline(canvas, 0, y, W)
    y += 6

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
    action_fnt = font(FONT_BODY_PX, 'regular')
    action_desc_fnt = font(FONT_CAPTION_PX, 'regular')
    combo_fnt = font(FONT_MONO_PX, 'mono')
    combo_b_fnt = font(FONT_MONO_PX, 'mono-bold')

    for i, (action, current, new, sel, conflict, changed) in enumerate(rows):
        bg = None
        if conflict:
            bg = K_CONFLICT_BG
        elif changed:
            bg = (255, 248, 240)
        if bg:
            d.rectangle([8, y, W - 8, y + ROW_H - 2], fill=bg)
            if conflict:
                d.rectangle([8, y, 12, y + ROW_H - 2], fill=K_CONFLICT_TXT)

        d.text((col_x[0], y + 6), action, fill=K_TEXT, font=action_fnt)
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
        paste_text_rgba(canvas, (col_x[0], y + 24), desc,
                        action_desc_fnt, K_TEXT_45)

        cur_x = col_x[1]
        if changed:
            tw = d.textlength(current, font=combo_fnt)
            d.text((cur_x, y + 12), current, fill=K_TEXT_30, font=combo_fnt)
            d.line([(cur_x - 1, y + 19), (cur_x + tw + 1, y + 19)],
                   fill=K_TEXT_30, width=1)
        else:
            d.text((cur_x, y + 12), current,
                   fill=K_CONFLICT_TXT if conflict else K_TEXT_45,
                   font=combo_b_fnt if conflict else combo_fnt)

        new_x = col_x[2]
        new_color = K_TEXT
        new_fnt_use = combo_fnt
        if conflict:
            new_color = K_CONFLICT_TXT
            new_fnt_use = combo_b_fnt
        elif changed:
            new_color = K_ACCENT
            new_fnt_use = combo_b_fnt
        d.text((new_x, y + 12), new, fill=new_color, font=new_fnt_use)

        bx = new_x + int(d.textlength(new, font=new_fnt_use)) + 10
        if conflict:
            badge(canvas, (bx, y + 14), '⚠ 冲突', 'conflict')
        elif changed:
            badge(canvas, (bx, y + 14), '已修改', 'changed')
        else:
            badge(canvas, (bx, y + 14), 'unchanged', 'unchanged')

        y += ROW_H

    hairline(canvas, 0, y, W)

    # ----- Bottom toolbar (separated) -----
    bottom_y = H - BOTTOM_H
    hairline(canvas, 0, bottom_y, W)

    # Left side buttons (Custom / Restore / Import / Export)
    left_w = 100
    mid_w = 90
    btn_h = 38
    by = bottom_y + (BOTTOM_H - btn_h) // 2
    bx = PANEL_PAD
    button(canvas, (bx, by), (left_w, btn_h), '+ 自定义', 'ghost')
    bx += left_w + GAP_LG
    button(canvas, (bx, by), (left_w, btn_h),
           '⟳ 恢复默认', 'danger', hint='⌘R')
    bx += left_w + GAP_LG
    button(canvas, (bx, by), (mid_w, btn_h), '导入', 'ghost')
    bx += mid_w + GAP_LG
    button(canvas, (bx, by), (mid_w, btn_h), '导出', 'ghost')
    left_total_right = PANEL_PAD + 2 * left_w + 2 * mid_w + 3 * GAP_LG

    # Right-aligned: Cancel + Save
    save_w = 110
    cancel_w = 90
    save_x = W - PANEL_PAD - save_w
    cancel_x = save_x - cancel_w - GAP_LG
    assert cancel_x >= left_total_right + GAP_LG, \
        f'Overlap! cancel_x={cancel_x} but left_total_right={left_total_right}'
    button(canvas, (cancel_x, by), (cancel_w, btn_h),
           '取消', 'ghost')
    button(canvas, (save_x, by), (save_w, btn_h),
           '保存', 'primary')

    # ----- Key capture popover -----
    pop_w, pop_h = 320, 120
    row4_y = y - (len(rows) - 4) * ROW_H
    pop_x = W - pop_w - PANEL_PAD - 12
    pop_y = row4_y - pop_h - 10
    if pop_y < TITLE_H + TOOLBAR_H + 30:
        pop_y = row4_y + ROW_H + 6
    sh = Image.new('RGBA', (pop_w + 16, pop_h + 16), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    sd.rounded_rectangle([4, 4, pop_w + 4, pop_h + 4],
                         radius=RADIUS_MD, fill=K_POP_SHADOW)
    canvas.paste(sh, (pop_x - 8, pop_y - 6), sh)

    d.rounded_rectangle([pop_x, pop_y, pop_x + pop_w, pop_y + pop_h],
                        radius=RADIUS_MD, fill=K_CAPTURE_BG,
                        outline=K_HAIRLINE, width=1)
    d.rectangle([pop_x, pop_y, pop_x + pop_w, pop_y + 3],
                fill=K_ACCENT)

    paste_text_rgba(canvas, (pop_x + 18, pop_y + 16),
                    '请按快捷键…', font(FONT_BODY_PX, 'bold'), K_TEXT)
    paste_text_rgba(canvas, (pop_x + pop_w - 76, pop_y + 18),
                    'Esc 取消', font(FONT_CAPTION_PX, 'mono'), K_TEXT_45)

    cb_x, cb_y = pop_x + 18, pop_y + 48
    cb_w, cb_h = pop_w - 36, 44
    d.rounded_rectangle([cb_x, cb_y, cb_x + cb_w, cb_y + cb_h],
                        radius=RADIUS_SM, fill=(255, 255, 255),
                        outline=K_ACCENT, width=2)
    paste_text_rgba(canvas, (cb_x + 16, cb_y + 12),
                    'Ctrl + Shift + L', font(20, 'mono-bold'), K_ACCENT)
    d.rectangle([cb_x + cb_w - 20, cb_y + 10, cb_x + cb_w - 17, cb_y + cb_h - 10],
                fill=K_ACCENT)

    paste_text_rgba(canvas, (cb_x, cb_y + cb_h + 12),
                    '⚠  与「打开 / 关闭设置栏」冲突 · 覆盖会禁用该快捷键',
                    font(FONT_CAPTION_PX), K_CONFLICT_TXT)

    tail_x = pop_x + 110
    tail_y = pop_y + pop_h
    d.polygon([(tail_x - 7, tail_y), (tail_x + 7, tail_y),
               (tail_x, tail_y + 8)], fill=K_CAPTURE_BG)
    d.line([(tail_x - 7, tail_y + 1), (tail_x + 7, tail_y + 1)],
           fill=K_CAPTURE_BG, width=1)

    return canvas


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, 'shortcut-settings.png')
    img = make_shortcut_settings_v3()
    img.save(out)
    print(f'  wrote {out} ({img.size[0]}x{img.size[1]})')


if __name__ == '__main__':
    main()