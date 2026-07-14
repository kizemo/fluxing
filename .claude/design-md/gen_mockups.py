"""Generate 3 PNG visual design drafts for Fluxing UI family.

Usage: python gen_mockups.py

Outputs (in this dir):
  - phrases-dialog-v2.png  (360x420 — phrases management)
  - user-dictionary.png    (760x480 — user dictionary editor)
  - shortcut-settings.png  (640x440 — shortcut rebinding)

Design philosophy: Liquid Discipline (see DESIGN-PHILOSOPHY.md)
Tokens: FLUENT-UI-TOKENS.md §3.6 (colors + sizes + times)
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

# ============================================================
# Token system (mirror FLUENT-UI-TOKENS.md §3.6)
# ============================================================
K_BG_TOP       = (245, 245, 248)
K_BG_BOT       = (220, 222, 230)
K_TEXT         = ( 30,  30,  40)
K_TEXT_50      = ( 30,  30,  40, 128)  # placeholder
K_TEXT_35      = ( 30,  30,  40,  90)   # disabled
K_SEL_BG       = (255, 235, 220)
K_ACCENT       = (255,  95,  49)
K_HAIRLINE     = (  0,   0,   0,  20)  # 8% black, hairline border
K_HAIRLINE_TOP = (255, 255, 255, 100)  # glass top highlight
K_SHADOW       = (  0,   0,   0,  30)  # 12% drop shadow
K_ICON_DIM     = (130, 130, 140)        # icon default color
K_ICON_ACCENT  = (255,  95,  49)        # hover/active

# Sizing (logical px)
RADIUS_LG      = 14
FONT_BODY_PX   = 14
FONT_LABEL_PX  = 17
TITLE_H        = 30
GAP_SM         = 8
GAP_LG         = 11
GAP_XL         = 16
GAP_MD         = 12
LINE_HAIR      = 1
PANEL_PADDING  = 12

# Font fallback (try Segoe UI Variable first; else canvas-fonts)
FONT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
                        'skills', 'canvas-design', 'canvas-fonts')
# Fallback: bundled Windows system font
WIN_FONT_DIR = r'C:\Windows\Fonts'


def load_font(size_px, weight='regular'):
    """Try several fonts in priority order. Prefer CJK-capable for Chinese text."""
    candidates = []
    if weight == 'bold':
        candidates += [
            os.path.join(WIN_FONT_DIR, 'msyhbd.ttc'),      # Microsoft YaHei Bold (CJK)
            os.path.join(WIN_FONT_DIR, 'msjhbd.ttc'),      # MS JhengHei Bold
            os.path.join(WIN_FONT_DIR, 'simsunb.ttf'),     # SimSun Bold (CJK)
            os.path.join(FONT_DIR, 'BigShoulders-Bold.ttf'),
            os.path.join(WIN_FONT_DIR, 'segoeuib.ttf'),
            os.path.join(WIN_FONT_DIR, 'arialbd.ttf'),
        ]
    else:
        candidates += [
            os.path.join(WIN_FONT_DIR, 'msyh.ttc'),        # Microsoft YaHei (CJK)
            os.path.join(WIN_FONT_DIR, 'msyh.ttf'),        # alt YaHei
            os.path.join(WIN_FONT_DIR, 'simsun.ttc'),     # SimSun (CJK)
            os.path.join(WIN_FONT_DIR, 'msjh.ttc'),        # MS JhengHei (CJK)
            os.path.join(WIN_FONT_DIR, 'YuGothR.ttc'),     # Yu Gothic (CJK)
            os.path.join(WIN_FONT_DIR, 'simfang.ttf'),     # FangSong (CJK)
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
    # Drop shadow (simulated by drawing shadow rects offset)
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
    # Glass body with gradient fill
    glass = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glass)
    for y in range(h):
        # Vertical gradient kBgTop → kBgBot
        t = y / max(1, h - 1)
        r = int(K_BG_TOP[0] * (1 - t) + K_BG_BOT[0] * t)
        g = int(K_BG_TOP[1] * (1 - t) + K_BG_BOT[1] * t)
        b = int(K_BG_TOP[2] * (1 - t) + K_BG_BOT[2] * t)
        # per-pixel alpha 220 → 80 (top opaque, bottom translucent)
        a = int(220 + (80 - 220) * t)
        gd.line([(0, y), (w, y)], fill=(r, g, b, a))
    # Rounded mask
    mask = Image.new('L', (w, h), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle([0, 0, w - 1, h - 1], radius=RADIUS_LG, fill=255)
    canvas.paste(glass, (0, 0), mask)
    # Hairline border
    bd = ImageDraw.Draw(canvas)
    bd.rounded_rectangle([0, 0, w - 1, h - 1], radius=RADIUS_LG,
                          outline=K_HAIRLINE, width=1)
    # Top highlight (1px white tint at top, inside the radius)
    bd.line([(RADIUS_LG, 1), (w - RADIUS_LG, 1)], fill=K_HAIRLINE_TOP, width=1)
    # Title bar text
    font_label = load_font(FONT_LABEL_PX, 'bold')
    bd.text((PANEL_PADDING, (TITLE_H - FONT_LABEL_PX) // 2 + 1),
             title, fill=K_TEXT, font=font_label)
    # Close button (X)
    x_btn_x = w - PANEL_PADDING - 14
    x_btn_y = (TITLE_H - 14) // 2 + 1
    bd.line([(x_btn_x, x_btn_y), (x_btn_x + 14, x_btn_y + 14)],
             fill=K_TEXT, width=2)
    bd.line([(x_btn_x + 14, x_btn_y), (x_btn_x, x_btn_y + 14)],
             fill=K_TEXT, width=2)


def draw_text(d, xy, text, font, fill=K_TEXT):
    d.text(xy, text, fill=fill, font=font)


def draw_text_rgba(canvas, xy, text, font, fill):
    img = Image.new('RGBA', (300, 50), (0, 0, 0, 0))
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
        # Accent fill
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
    # Magnifier glyph (small circle + handle)
    icon_x = x + 8
    icon_y = y + bh // 2
    bd.ellipse([icon_x - 4, icon_y - 4, icon_x + 4, icon_y + 4],
               outline=K_ICON_DIM, width=2)
    bd.line([(icon_x + 4, icon_y + 4), (icon_x + 8, icon_y + 8)],
             fill=K_ICON_DIM, width=2)
    # Query text or placeholder
    font_input = load_font(FONT_BODY_PX, 'regular')
    text_x = icon_x + 14
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
# Mockup 1: PhrasesDialog v2 (360 × 420)
# ============================================================
def make_phrases_dialog():
    W, H = 360, 420
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_panel_chrome(canvas, W, H, '常用短语')

    y = TITLE_H + GAP_SM
    # Search box
    draw_search_box(canvas, (PANEL_PADDING, y), (W - 2 * PANEL_PADDING, 30))
    y += 30 + GAP_SM
    draw_hairline(canvas, (0, y), W)
    y += GAP_SM

    # Tree (categories + phrases)
    font_body = load_font(FONT_BODY_PX, 'regular')
    font_body_b = load_font(FONT_BODY_PX, 'bold')

    items = [
        ('▶', '工作', None, False),
        ('  ', '    你好', None, False),
        ('  ', '    谢谢', None, False),
        ('  ', '    期待合作', None, False),
        ('▼', '日常', None, False),
        ('  ', '    收到', None, False),
        ('▸', '    好的,收到', None, True),    # selected (peach)
        ('  ', '    请稍等', None, False),
        ('▼', '(未分类)', None, False),
        ('  ', '    Hello', None, False),
        ('  ', '    测试短语', None, False),
    ]
    bd = ImageDraw.Draw(canvas)
    line_h = FONT_BODY_PX + 8
    for arrow, text, _, selected in items:
        if selected:
            bd.rectangle([PANEL_PADDING - 4, y, W - PANEL_PADDING, y + line_h],
                          fill=K_SEL_BG)
        # Draw arrow
        bd.text((PANEL_PADDING, y + 2), arrow, fill=K_ICON_DIM, font=font_body_b)
        # Draw text (browse-tree indent)
        # Indent further for phrases (text starts with 4 spaces already)
        if text.startswith('    '):
            draw_text(bd, (PANEL_PADDING + 20, y + 2), text[4:],
                       font_body, K_TEXT)
        else:
            draw_text(bd, (PANEL_PADDING + 16, y + 2), text,
                       font_body_b if not arrow.startswith(' ') else font_body,
                       K_TEXT)
        y += line_h

    y = H - 38 - GAP_SM
    draw_hairline(canvas, (0, y), W)
    y += GAP_SM
    # Buttons row
    btn_w = (W - 2 * PANEL_PADDING - 3 * GAP_SM) // 4
    btn_h = 32
    bx = PANEL_PADDING
    for label in ['+ 添加', '✎ 编辑', '− 删除', '取消']:
        draw_button(canvas, (bx, y), (btn_w, btn_h), label)
        bx += btn_w + GAP_SM
    return canvas


# ============================================================
# Mockup 2: UserDictionary (760 × 480)
# ============================================================
def make_user_dictionary():
    W, H = 760, 480
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_panel_chrome(canvas, W, H, '')
    bd = ImageDraw.Draw(canvas)

    # Three deliberate type roles: YaHei title/CJK, UI body, mono data.
    font_title = load_font(17, 'bold')
    font_body = load_font(13, 'regular')
    font_body_b = load_font(13, 'bold')
    font_caption = load_font(11, 'regular')
    font_mono_path = os.path.join(FONT_DIR, 'DMMono-Regular.ttf')
    font_mono = ImageFont.truetype(font_mono_path, 12) if os.path.exists(font_mono_path) else font_body

    # Bespoke title bar: book glyph, two-line identity, restrained close control.
    icon_x, icon_y = 14, 12
    bd.rounded_rectangle([icon_x, icon_y, icon_x + 13, icon_y + 12], radius=2,
                         fill=(255, 255, 255, 130), outline=(118, 118, 128, 130), width=1)
    bd.line([(icon_x + 6, icon_y + 2), (icon_x + 6, icon_y + 10)], fill=K_ACCENT, width=1)
    bd.line([(icon_x + 2, icon_y + 3), (icon_x + 5, icon_y + 3)], fill=K_ICON_DIM, width=1)
    bd.text((34, 6), '用户词典', fill=K_TEXT, font=font_title)
    bd.text((123, 10), 'Personal dictionary · 247 entries', fill=(110, 110, 120), font=font_caption)
    close_x, close_y = W - 24, 12
    bd.line([(close_x, close_y), (close_x + 10, close_y + 10)], fill=(75, 75, 84), width=1)
    bd.line([(close_x + 10, close_y), (close_x, close_y + 10)], fill=(75, 75, 84), width=1)

    # Toolbar remains structurally identical: search on left, context/status on right.
    toolbar_y = 36
    draw_search_box(canvas, (12, toolbar_y), (300, 30))
    bd.rounded_rectangle([262, toolbar_y + 5, 302, toolbar_y + 25], radius=5,
                         fill=(238, 238, 242), outline=(0, 0, 0, 16), width=1)
    bd.text((270, toolbar_y + 7), '⌘F', fill=(112, 112, 122), font=font_caption)

    def pill(x, width, label, dot=None):
        bd.rounded_rectangle([x, toolbar_y + 2, x + width, toolbar_y + 28], radius=8,
                             fill=(255, 255, 255, 178), outline=(0, 0, 0, 18), width=1)
        tx = x + 10
        if dot:
            bd.ellipse([x + 10, toolbar_y + 11, x + 17, toolbar_y + 18], fill=dot)
            tx = x + 23
        bd.text((tx, toolbar_y + 6), label, fill=(62, 62, 72), font=font_caption)

    pill(326, 156, '已部署 · 3 分钟前', (52, 176, 94))
    pill(490, 138, 'luna_pinyin  ▾')
    pill(636, 112, '更多  ▾')
    draw_hairline(canvas, (0, 74), W)

    # Finder-like table: column geometry encodes information weight.
    table_x, table_y, table_w = 8, 82, 744
    header_h, row_h = 28, 28
    col_x = [table_x, table_x + 240, table_x + 400, table_x + 480]
    col_w = [240, 160, 80, 264]
    headers = [('词条', '▼'), ('编码', '↕'), ('权重', '↕'), ('方案', '↕')]
    bd.rounded_rectangle([table_x, table_y, table_x + table_w, 407], radius=9,
                         fill=(255, 255, 255, 84), outline=(0, 0, 0, 18), width=1)
    bd.rounded_rectangle([table_x, table_y, table_x + table_w, table_y + header_h], radius=9,
                         fill=(255, 255, 255, 126))
    bd.rectangle([table_x, table_y + 18, table_x + table_w, table_y + header_h],
                 fill=(255, 255, 255, 126))
    for idx, ((label, sort), cx, cw) in enumerate(zip(headers, col_x, col_w)):
        bd.text((cx + 10, table_y + 6), label, fill=(92, 92, 103), font=font_body_b)
        sort_fill = K_ACCENT if idx == 0 else (154, 154, 164)
        bd.text((cx + cw - 20, table_y + 7), sort, fill=sort_fill, font=font_caption)
        if idx < len(headers) - 1:
            divider_x = cx + cw
            for offset, alpha in [(-2, 0), (-1, 8), (0, 28), (1, 8)]:
                bd.line([(divider_x + offset, table_y + 5),
                         (divider_x + offset, table_y + header_h - 5)],
                        fill=(0, 0, 0, alpha), width=1)
    bd.line([(table_x, table_y + header_h), (table_x + table_w, table_y + header_h)],
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

    row_y = table_y + header_h
    for index, (text, code, weight, schema) in enumerate(rows):
        selected = index == 0
        hovered = index == 3
        if selected:
            bd.rectangle([table_x + 1, row_y, table_x + table_w - 1, row_y + row_h], fill=K_SEL_BG)
            bd.rectangle([table_x + 1, row_y, table_x + 4, row_y + row_h], fill=K_ACCENT)
        elif hovered:
            bd.rectangle([table_x + 1, row_y, table_x + table_w - 1, row_y + row_h],
                         fill=(255, 255, 255, 92))
        if index:
            bd.line([(table_x + 8, row_y), (table_x + table_w - 8, row_y)],
                    fill=(0, 0, 0, 9), width=1)

        bd.text((col_x[0] + 10, row_y + 6), text, fill=K_TEXT,
                font=font_body_b if selected else font_body)
        bd.text((col_x[1] + 10, row_y + 7), code, fill=(66, 66, 77), font=font_mono)

        # Signature element: compact, color-coded weight meter.
        wc = weight_color(weight)
        bd.text((col_x[2] + 8, row_y + 6), str(weight), fill=wc, font=font_body_b)
        track_x, track_y, track_w = col_x[2] + 38, row_y + 13, 32
        bd.rounded_rectangle([track_x, track_y, track_x + track_w, track_y + 4], radius=2,
                             fill=(196, 196, 203, 105))
        bd.rounded_rectangle([track_x, track_y,
                              track_x + max(3, int(track_w * weight / 100)), track_y + 4],
                             radius=2, fill=wc)

        chip_x, chip_w = col_x[3] + 10, 92
        bd.rounded_rectangle([chip_x, row_y + 5, chip_x + chip_w, row_y + 23], radius=9,
                             fill=(231, 232, 238, 180), outline=(0, 0, 0, 12), width=1)
        bd.text((chip_x + 9, row_y + 6), schema, fill=(83, 83, 94), font=font_caption)

        if selected:
            # Quiet inline actions appear only for the chosen row.
            action_x = table_x + table_w - 59
            bd.rounded_rectangle([action_x, row_y + 4, action_x + 48, row_y + 24], radius=6,
                                 fill=(255, 255, 255, 180), outline=(0, 0, 0, 16), width=1)
            bd.text((action_x + 9, row_y + 3), '✎', fill=(75, 75, 85), font=font_body)
            bd.line([(action_x + 25, row_y + 8), (action_x + 25, row_y + 20)],
                    fill=(0, 0, 0, 18), width=1)
            bd.text((action_x + 33, row_y + 5), '−', fill=(208, 69, 69), font=font_body_b)
        row_y += row_h

    # Compact status strip doubles as the multi-select affordance.
    status_y = 408
    bd.text((14, status_y + 8), '247 条词条  ·  当前方案 luna_pinyin',
            fill=(104, 104, 115), font=font_caption)
    bd.text((W - 192, status_y + 8), 'Ctrl / Shift 点击可多选',
            fill=(128, 128, 138), font=font_caption)

    # Bottom actions: creation left, destructive/contextual middle, primary deploy right.
    footer_y = 440
    draw_hairline(canvas, (0, footer_y - 1), W)
    draw_button(canvas, (12, footer_y + 4), (84, 30), '+ 添加', 'normal')
    draw_button(canvas, (108, footer_y + 4), (72, 30), '− 删除', 'normal')
    bd.text((126, footer_y + 11), '− 删除', fill=(208, 69, 69), font=font_body)
    draw_button(canvas, (192, footer_y + 4), (68, 30), '导入', 'normal')
    draw_button(canvas, (272, footer_y + 4), (68, 30), '导出', 'normal')
    draw_button(canvas, (548, footer_y + 4), (76, 30), '取消', 'normal')
    draw_button(canvas, (636, footer_y + 4), (112, 30), '⟳ 部署', 'primary')
    return canvas


# ============================================================
# Mockup 3: ShortcutSettings (640 × 440)
# ============================================================
def make_shortcut_settings():
    W, H = 640, 440
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_panel_chrome(canvas, W, H, '快捷键设置')

    y = TITLE_H + GAP_SM
    bd = ImageDraw.Draw(canvas)
    font_body = load_font(FONT_BODY_PX, 'regular')
    font_body_b = load_font(FONT_BODY_PX, 'bold')
    font_label = load_font(FONT_LABEL_PX, 'bold')

    # Subtitle / status
    draw_text_rgba(canvas, (PANEL_PADDING, y + 4),
                    '点击 "新键" 列按下一个组合键。冲突会自动标红。',
                    font_body, (110, 110, 120))
    y += FONT_BODY_PX + GAP_SM
    draw_hairline(canvas, (0, y), W)
    y += GAP_SM

    # Table headers
    col_x = [PANEL_PADDING, 360, 510]
    headers = ['动作 (action)', '当前键 (current)', '新键 (new)']
    for x, h in zip(col_x, headers):
        draw_text(bd, (x + 4, y), h, font_body_b, (110, 110, 120))
    y += FONT_BODY_PX + 6
    draw_hairline(canvas, (0, y), W)
    y += GAP_SM

    # Rows — mix of current/new states
    rows = [
        ('切换中英文',          'Shift+Space',   'Shift+Space', False, False),
        ('打开 / 关闭设置栏',    'Ctrl+Shift+`,', 'Ctrl+Shift+S', False, False),
        ('切换全 / 半角',         'Shift+Space',   'Shift+Space', False, True),   # conflict (same)
        ('常用短语',              'Alt+.',          'Alt+.',       False, False),
        ('重选候选',              'Ctrl+Shift+R',   'Shift+L',     False, False),
        ('第二候选',              'Shift+Tab',      'Shift+Tab',   False, False),
        ('翻页 (候选上)',        'Page Up',        'Page Up',     False, False),
        ('翻页 (候选下)',        'Page Down',      'Page Down',   False, False),
    ]
    row_h = FONT_BODY_PX + 12
    for action, current, new, sel, conflict in rows:
        bg = None
        if sel:
            bg = K_SEL_BG
        elif conflict:
            bg = (255, 230, 225)  # warm warning
        if bg:
            bd.rectangle([PANEL_PADDING - 4, y - 2,
                            W - PANEL_PADDING, y + row_h - 2],
                           fill=bg)
        font_use = font_body_b if sel else font_body
        draw_text(bd, (col_x[0] + 4, y + 4), action, font_use, K_TEXT)
        # Current key (gray)
        draw_text(bd, (col_x[1] + 4, y + 4), current, font_body,
                   K_TEXT if not conflict else K_ACCENT)
        # New key (orange if changed)
        new_changed = new != current
        draw_text(bd, (col_x[2] + 4, y + 4), new, font_body,
                   K_ACCENT if new_changed else K_TEXT)
        y += row_h

    # Bottom: buttons
    y = H - 38 - GAP_SM
    draw_hairline(canvas, (0, y), W)
    y += GAP_SM
    btn_w = 90
    btn_h = 32
    labels_styles = [
        ('恢复默认', 'normal'),
        ('导入配置', 'normal'),
        ('导出配置', 'normal'),
        ('保存', 'primary'),
        ('取消', 'normal'),
    ]
    total_w = sum(btn_w for _ in labels_styles) + GAP_SM * (len(labels_styles) - 1)
    bx = PANEL_PADDING + (W - 2 * PANEL_PADDING - total_w) // 2
    for label, style in labels_styles:
        draw_button(canvas, (bx, y), (btn_w, btn_h), label, style)
        bx += btn_w + GAP_SM
    return canvas


# ============================================================
# Driver
# ============================================================
def main():
    here = os.path.dirname(os.path.abspath(__file__))
    targets = [
        ('phrases-dialog-v2.png', make_phrases_dialog),
        ('user-dictionary.png',   make_user_dictionary),
        ('shortcut-settings.png', make_shortcut_settings),
    ]
    for name, fn in targets:
        img = fn()
        out = os.path.join(here, name)
        img.save(out)
        print(f"  wrote {out} ({img.size[0]}x{img.size[1]})")


if __name__ == '__main__':
    main()