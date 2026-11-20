$ErrorActionPreference = 'Stop'

# 批次 BB · 修缺陷 C：账号页 DPI 感知布局（单测侧）
#   · 旧调用点补显式 dpi（96 = 旧字面量，语义不变）
#   · 新增 TestAccountPageUiDpiScale()：7 档 DPI 的缩放律判据（含反向有牙判据）

$path = 'F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp'

$bytes = [System.IO.File]::ReadAllBytes($path)
$hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
$text = [System.IO.File]::ReadAllText($path)
$nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
Write-Host ("LINE_ENDING=" + $(if ($nl -eq "`r`n") { 'CRLF' } else { 'LF' }) + " BOM=" + $hasBom)

function Norm([string]$s) {
  return $s.Replace("`r`n", "`n").Replace("`n", $nl)
}

# ---- 正则替换（带命中次数断言） --------------------------------------------

$regexPatches = @(
  @{ name = 'R1-VisibleControlIds'; pattern = 'fc::AccountVisibleControlIds\((false|true)\)'; replacement = 'fc::AccountVisibleControlIds(${1}, fc::kAccountBaseDpi)'; expect = 2 },
  @{ name = 'R2-VisibleIds'; pattern = 'fc::AccountVisibleIds\((fc::AccountWindowRole::k\w+), true\)'; replacement = 'fc::AccountVisibleIds(${1}, true, fc::kAccountBaseDpi)'; expect = 6 },
  @{ name = 'R3-VisibleLayoutRole'; pattern = 'fc::AccountVisibleLayout\((fc::AccountWindowRole::k\w+), (true|false)\)'; replacement = 'fc::AccountVisibleLayout(${1}, ${2}, fc::kAccountBaseDpi)'; expect = 3 },
  @{ name = 'R4-VisibleLayoutRoleVar'; pattern = 'fc::AccountVisibleLayout\(role, st == 1\)'; replacement = 'fc::AccountVisibleLayout(role, st == 1, fc::kAccountBaseDpi)'; expect = 1 },
  @{ name = 'R5-ClientSizeAccount'; pattern = 'fc::AccountClientSize\(fc::AccountWindowRole::kAccount, &client_w, &client_h\)'; replacement = 'fc::AccountClientSize(fc::AccountWindowRole::kAccount, fc::kAccountBaseDpi, &client_w, &client_h)'; expect = 1 },
  @{ name = 'R6-ClientSizeVar'; pattern = 'fc::AccountClientSize\(role, &w, &h\)'; replacement = 'fc::AccountClientSize(role, fc::kAccountBaseDpi, &w, &h)'; expect = 1 },
  @{ name = 'R7-Widgets'; pattern = 'fc::AccountWidgets\(fc::AccountWindowRole::kAccount\)'; replacement = 'fc::AccountWidgets(fc::AccountWindowRole::kAccount, fc::kAccountBaseDpi)'; expect = 1 }
)

foreach ($p in $regexPatches) {
  $rx = [regex]$p.pattern
  $hits = $rx.Matches($text).Count
  if ($hits -ne $p.expect) {
    throw ("PATCH ABORT [" + $p.name + "] 命中 " + $hits + " 次（要求 " + $p.expect + " 次）")
  }
  $text = $rx.Replace($text, $p.replacement)
  Write-Host ("OK " + $p.name + " x" + $hits)
}

# ---- 新增判据函数 -----------------------------------------------------------

$anchor = @'
// ---------------------------------------------------------------------------
// 批次 AT-1 · ADR 0063：历史邮箱存储
// ---------------------------------------------------------------------------
'@

$newBlock = @'
// ---------------------------------------------------------------------------
// 批次 BB · ADR 0068：账号页 DPI 感知布局（修缺陷 C）
// ---------------------------------------------------------------------------
//
// 缺陷 C（批次 BA 真机实测）：144 dpi（150% 缩放）下「换钥提示」的 37 个全角字
// 折成两行、需 36 px，而控件高度字面量也是 36 ⇒ **余量 0 px** ⇒ >150% 缩放的
// 机器必然裁掉末行（175% 需 42 px、200% 需 48 px）。
//
// 修法（真相源 CloudAccountUi.h）：**整表按 DPI 等比缩放** ⇒「文字 : 盒子」的
// 比值与 DPI 无关 ⇒ 任何缩放下都裁不到；96 dpi 下逐像素等于旧布局（零行为
// 变化）。判据全部取自生产头（R7：生产与单测共用同一份实现）。

void TestAccountPageUiDpiScale() {
  std::printf("[BB] ADR 0068 账号页 DPI 感知布局（缺陷 C）\n");

  const int kDpis[] = {96, 120, 144, 168, 192, 240, 288};
  const int kDpiCount = 7;
  const int note_chars =
      fc::AccountWideCharCount(fc::AccountKeyRotationNoteText());

  // 1) 字体行高 → 有效 DPI 的映射（生产侧量 DEFAULT_GUI_FONT 得到的口径）。
  Check(fc::AccountEffectiveDpiFromFontPx(12) == 96 &&
            fc::AccountEffectiveDpiFromFontPx(15) == 120 &&
            fc::AccountEffectiveDpiFromFontPx(18) == 144 &&
            fc::AccountEffectiveDpiFromFontPx(21) == 168 &&
            fc::AccountEffectiveDpiFromFontPx(24) == 192 &&
            fc::AccountEffectiveDpiFromFontPx(30) == 240 &&
            fc::AccountEffectiveDpiFromFontPx(36) == 288 &&
            fc::AccountEffectiveDpiFromFontPx(0) == 96,
        "BB 字体行高 → 有效 DPI 映射（12→96 … 36→288）");

  // 2) 换钥提示文案 = 37 个全角字（与批次 BA 真机实测逐字一致）。
  Check(note_chars == 37, "BB 换钥提示文案 = 37 个全角字（与真机实测一致）");

  // 3) 100% 缩放下**逐像素不变**（零行为变化；坐标取自 ADR 0063 冻结合同）。
  {
    const std::vector<fc::AccountLayoutEntry> logged =
        fc::AccountVisibleLayout(fc::AccountWindowRole::kAccount, true, 96);
    fc::AccountLayoutEntry note = {-1, -1, -1, -1, -1};
    fc::AccountLayoutEntry status = {-1, -1, -1, -1, -1};
    fc::AccountLayoutEntry close = {-1, -1, -1, -1, -1};
    for (size_t i = 0; i < logged.size(); ++i) {
      if (logged[i].id == fc::kIdKeyRotationNote) note = logged[i];
      if (logged[i].id == fc::kIdStatus) status = logged[i];
      if (logged[i].id == fc::kIdClose) close = logged[i];
    }
    int cw = 0;
    int ch = 0;
    fc::AccountClientSize(fc::AccountWindowRole::kAccount, 96, &cw, &ch);
    Check(note.x == 12 && note.y == 156 && note.w == 456 && note.h == 36 &&
              status.x == 12 && status.y == 198 && status.w == 456 &&
              status.h == 40 && close.x == 368 && close.y == 250 &&
              close.w == 100 && close.h == 28 && cw == 480 && ch == 290,
          "BB 100% 缩放下账号页几何逐像素不变（零行为变化）");
  }

  // 4) 多 DPI：所有可见控件都不得越出客户区（客户区同样按 DPI 放大）。
  {
    bool all_fit = true;
    int checked = 0;
    for (int d = 0; d < kDpiCount; ++d) {
      const int dpi = kDpis[d];
      for (int r = 0; r < static_cast<int>(fc::AccountWindowRole::kCount); ++r) {
        const fc::AccountWindowRole role =
            static_cast<fc::AccountWindowRole>(r);
        int w = 0;
        int h = 0;
        fc::AccountClientSize(role, dpi, &w, &h);
        for (int st = 0; st < 2; ++st) {
          const std::vector<fc::AccountLayoutEntry> layout =
              fc::AccountVisibleLayout(role, st == 1, dpi);
          for (size_t i = 0; i < layout.size(); ++i) {
            ++checked;
            if (layout[i].x < 0 || layout[i].y < 0 || layout[i].w <= 0 ||
                layout[i].h <= 0 || layout[i].x + layout[i].w > w ||
                layout[i].y + layout[i].h > h) {
              all_fit = false;
            }
          }
        }
      }
    }
    CheckMany(all_fit, "BB 7 档 DPI × 5 窗口 × 两态：控件全部落在客户区内",
              checked);
  }

  // 5) 核心判据：换钥提示在**每档** DPI 下都装得下，且至少留 1 行余量
  //    （缺陷 C 的现场余量 = 0 ⇒ 必然判红）。
  {
    bool ok = true;
    int checked = 0;
    for (int d = 0; d < kDpiCount; ++d) {
      const int dpi = kDpis[d];
      const std::vector<fc::AccountLayoutEntry> logged =
          fc::AccountVisibleLayout(fc::AccountWindowRole::kAccount, true, dpi);
      for (size_t i = 0; i < logged.size(); ++i) {
        if (logged[i].id != fc::kIdKeyRotationNote)
          continue;
        ++checked;
        const int required =
            fc::AccountRequiredHeightForText(note_chars, logged[i].w, dpi);
        if (logged[i].h < required + fc::AccountLineHeightPx(dpi))
          ok = false;
      }
    }
    CheckMany(ok, "BB 换钥提示在 7 档 DPI 下都装得下且留 ≥1 行余量", checked);
  }

  // 6) **判据有牙**：用缺陷 C 的现场几何（144 dpi 下未缩放的 456×36）算余量
  //    ⇒ 必须是 0（判红）。若这条变成「通过」，说明判据本身失效。
  {
    const int dpi = 144;
    const int old_w = 456;  // 未缩放的盒子宽（字面量）
    const int old_h = 36;   // 未缩放的盒子高（字面量）
    const int required =
        fc::AccountRequiredHeightForText(note_chars, old_w, dpi);
    const int margin = old_h - required;
    Check(required == 36 && margin == 0,
          "BB 反向判据：缺陷 C 现场（144 dpi 未缩放）余量 = 0（判红）");
  }

  // 7) 等比性：144 dpi 下盒子与文字同比放大 ⇒ 折行数恒为 1（不再新增折行）。
  {
    const int w96 = fc::AccountTextWidthEm(note_chars, 96);
    const int w144 = fc::AccountTextWidthEm(note_chars, 144);
    const std::vector<fc::AccountLayoutEntry> logged144 =
        fc::AccountVisibleLayout(fc::AccountWindowRole::kAccount, true, 144);
    int box144 = 0;
    for (size_t i = 0; i < logged144.size(); ++i) {
      if (logged144[i].id == fc::kIdKeyRotationNote)
        box144 = logged144[i].w;
    }
    // BA 真机实测：144 dpi 下 37 个全角字单行宽 = 666 px。
    Check(w96 == 444 && w144 == 666 &&
              fc::AccountTextLineCount(w144, box144) == 1 &&
              fc::AccountTextLineCount(w144, 456) == 2,
          "BB 换钥提示：144 dpi 单行宽 666 px（与真机实测一致）且缩放后不再折行");
  }

  // 8) 缩放入口唯一：客户区尺寸与控件几何同源于 AccountScale（同一把尺子）。
  {
    bool ok = true;
    for (int d = 0; d < kDpiCount; ++d) {
      const int dpi = kDpis[d];
      int w = 0;
      int h = 0;
      fc::AccountClientSize(fc::AccountWindowRole::kAccount, dpi, &w, &h);
      if (w != fc::AccountScale(480, dpi) || h != fc::AccountScale(290, dpi))
        ok = false;
    }
    Check(ok, "BB 客户区尺寸 = 基准 × dpi/96（与控件同一把尺子）");
  }
}

'@

$anchorNorm = Norm $anchor
if (($text.Split(@($anchorNorm), [System.StringSplitOptions]::None).Count - 1) -ne 1) {
  throw 'PATCH ABORT [插入新判据函数] 锚点命中次数 != 1'
}
$text = $text.Replace($anchorNorm, (Norm $newBlock) + $anchorNorm)
Write-Host 'OK I-NewDpiTestFunction'

# ---- 注册到 main() ----------------------------------------------------------

$oldMain = @'
  TestAccountPageUi();
'@
$newMain = @'
  TestAccountPageUi();
  TestAccountPageUiDpiScale();
'@
$oldMainNorm = Norm $oldMain
if (($text.Split(@($oldMainNorm), [System.StringSplitOptions]::None).Count - 1) -ne 1) {
  throw 'PATCH ABORT [main 注册] 锚点命中次数 != 1'
}
$text = $text.Replace($oldMainNorm, (Norm $newMain))
Write-Host 'OK J-RegisterInMain'

$enc = New-Object System.Text.UTF8Encoding($hasBom)
[System.IO.File]::WriteAllText($path, $text, $enc)
Write-Host ("BYTES=" + ([System.IO.File]::ReadAllBytes($path)).Length)
