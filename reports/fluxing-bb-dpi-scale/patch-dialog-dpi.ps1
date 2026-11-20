$ErrorActionPreference = 'Stop'

# 批次 BB · 修缺陷 C：账号页 DPI 感知布局（生产实现侧）
# 一次性做 8 处定点替换；每处都断言「原文恰好命中一次」，否则整体失败。

$path = 'F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountDialog.cpp'

$bytes = [System.IO.File]::ReadAllBytes($path)
$hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
$text = [System.IO.File]::ReadAllText($path)
$nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
Write-Host ("LINE_ENDING=" + $(if ($nl -eq "`r`n") { 'CRLF' } else { 'LF' }) + " BOM=" + $hasBom)

function Norm([string]$s) {
  return $s.Replace("`r`n", "`n").Replace("`n", $nl)
}

$oldA = @'
    case kIdKeyRotationNote:
      // ADR 0063 补充（用户拍板「加一句提示」）：重置口令 ⇒ enc_key 变 ⇒
      // 云端旧密文永不可解（R2 服务端永不见 enc_key，无法代解）。口径与
      // 「找回密码」窗的补救指引一致（批次 AV），此处为常驻说明。
      return L"重置登录口令后，云端旧条目将无法再解开，可在「设备与云数据」清空云端数据。";
'@

$newA = @'
    case kIdKeyRotationNote:
      // ADR 0063 补充（用户拍板「加一句提示」）：重置口令 ⇒ enc_key 变 ⇒
      // 云端旧密文永不可解（R2 服务端永不见 enc_key，无法代解）。口径与
      // 「找回密码」窗的补救指引一致（批次 AV），此处为常驻说明。
      // ⚠️ 文案本体在真相源 CloudAccountUi.h（批次 BB）：单测要拿它判定
      //    「控件装不装得下这行字」⇒ 生产与单测必须是同一份（R7）。
      return std::wstring(AccountKeyRotationNoteText());
'@

$oldB = @'
void CreateWidgets(HWND hwnd, Role role) {
  const std::vector<AccountWidgetSpec> specs = AccountWidgets(role);
'@

$newB = @'
// ---------------------------------------------------------------------------
// 有效 DPI（批次 BB · 修缺陷 C）
// ---------------------------------------------------------------------------
//
// UI 字体是 **DEFAULT_GUI_FONT**（进程级 stock 对象，按系统 DPI 光栅化）。
// 账号页的控件几何若不跟着放大，文字就会被裁（批次 BA 实测：144 dpi 下
// 37 个全角字折两行正好 36 px，而控件高度字面量也是 36 ⇒ 余量 0；
// >150% 缩放的机器必然裁掉末行）。
//
// 口径 = **量真实字体**（GetTextMetrics 量 stock font 的行高）反推有效 DPI，
// 而不是猜一个 DPI —— 这样「盒子 : 文字」的比值恒定，任何缩放下都裁不到。
//
// ⚠️ 禁用 GetDpiForMonitor：它会引入 api-ms-win-shcore-scaling-l1-1-1.dll
//    依赖，Win10 24H2 上该 DLL 缺失 ⇒ regsvr32 退出码 3（教训 L122）。
int AccountUiDpi() {
  static int cached = 0;
  if (cached > 0)
    return cached;
  int dpi = kAccountBaseDpi;
  HDC dc = ::GetDC(nullptr);
  if (dc != nullptr) {
    HGDIOBJ old_font = ::SelectObject(dc, widgets::DefaultUiFont());
    TEXTMETRICW tm;
    ::ZeroMemory(&tm, sizeof(tm));
    if (::GetTextMetricsW(dc, &tm)) {
      const int line_px = tm.tmHeight + tm.tmExternalLeading;
      const int derived = AccountEffectiveDpiFromFontPx(line_px);
      // 不允许反推出比 100% 还小的缩放（度量失败 / 异常字体时的兜底）。
      if (derived >= kAccountBaseDpi)
        dpi = derived;
    }
    ::SelectObject(dc, old_font);
    ::ReleaseDC(nullptr, dc);
  }
  cached = dpi;
  return dpi;
}

void CreateWidgets(HWND hwnd, Role role, int dpi) {
  const std::vector<AccountWidgetSpec> specs = AccountWidgets(role, dpi);
'@

$oldC = @'
void ApplyLayout(HWND hwnd, Role role, bool logged_in) {
  const std::vector<AccountWidgetSpec> specs = AccountWidgets(role);
  const std::vector<AccountLayoutEntry> layout =
      AccountVisibleLayout(role, logged_in);
'@

$newC = @'
void ApplyLayout(HWND hwnd, Role role, bool logged_in, int dpi) {
  const std::vector<AccountWidgetSpec> specs = AccountWidgets(role, dpi);
  const std::vector<AccountLayoutEntry> layout =
      AccountVisibleLayout(role, logged_in, dpi);
'@

$oldD = @'
    ApplyLayout(hwnd, role, logged_in);
'@

$newD = @'
    ApplyLayout(hwnd, role, logged_in, AccountUiDpi());
'@

$oldE = @'
      CreateWidgets(hwnd, role);
'@

$newE = @'
      CreateWidgets(hwnd, role, AccountUiDpi());
'@

$oldF = @'
  int width = 480;
  int height = 250;
  AccountClientSize(role, &width, &height);
'@

$newF = @'
  // 客户区尺寸同样按 DPI 放大（缺陷 C 的教训：盒子不放大 ⇒ 文字被裁）。
  const int dpi = AccountUiDpi();
  int width = 480;
  int height = 250;
  AccountClientSize(role, dpi, &width, &height);
'@

$oldG = @'
    case WM_CLOSE:
      // 唯一的销毁入口：所有关闭触发点（关闭按钮 / 标题栏 X）都投递 WM_CLOSE
'@

$newG = @'
    case WM_DPICHANGED: {
      // **故意不跟随** 屏幕 DPI 缩放（批次 BB）：
      //   · UI 字体是进程级 stock 对象（按系统 DPI 光栅化），窗口挪到另一块
      //     屏也不会变大 ⇒ 跟着屏幕 DPI 放大会让「盒子 : 文字」失配 ⇒ 反而
      //     裁字；
      //   · 若交给 DefWindowProc 默认处理，它会按系统给的建议矩形缩放窗口，
      //     而子控件仍按创建时的缩放摆位 ⇒ 客户区与控件对不上（控件会被
      //     窗口边缘裁掉）。
      // ⇒ 显式吞掉该消息：窗口尺寸与控件布局始终来自**同一份**缩放，
      //   两者永远配套。
      // 彻底修好（按所在屏缩放 + 按屏建字体）需要 per-window font
      // （CreateFontIndirectW + SystemParametersInfoForDpi），见 ADR 0068
      // 的「已知限制」。
      return 0;
    }

    case WM_CLOSE:
      // 唯一的销毁入口：所有关闭触发点（关闭按钮 / 标题栏 X）都投递 WM_CLOSE
'@

$oldH = @'
#define NOMINMAX

#include "CloudAccountDialog.h"
'@

$newH = @'
#define NOMINMAX

// WM_DPICHANGED（Win8.1+）：旧 SDK 未必声明 ⇒ 兜底定义（值固定不变）。
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#include "CloudAccountDialog.h"
'@

$patches = @(
  @{ name = 'A-WidgetText'; old = $oldA; new = $newA },
  @{ name = 'B-DpiHelper+CreateWidgets'; old = $oldB; new = $newB },
  @{ name = 'C-ApplyLayout'; old = $oldC; new = $newC },
  @{ name = 'D-ApplyLayoutCall'; old = $oldD; new = $newD },
  @{ name = 'E-CreateWidgetsCall'; old = $oldE; new = $newE },
  @{ name = 'F-ClientSize'; old = $oldF; new = $newF },
  @{ name = 'G-WM_DPICHANGED'; old = $oldG; new = $newG },
  @{ name = 'H-WM_DPICHANGEDDefine'; old = $oldH; new = $newH }
)

$applied = 0
foreach ($p in $patches) {
  $o = Norm $p.old
  $n = Norm $p.new
  $count = 0
  $idx = $text.IndexOf($o)
  while ($idx -ge 0) {
    $count++
    $idx = $text.IndexOf($o, $idx + $o.Length)
  }
  if ($count -ne 1) {
    throw ("PATCH ABORT [" + $p.name + "] 命中 " + $count + " 次（要求恰好 1 次）")
  }
  $text = $text.Replace($o, $n)
  $applied++
  Write-Host ("OK " + $p.name)
}

$enc = New-Object System.Text.UTF8Encoding($hasBom)
[System.IO.File]::WriteAllText($path, $text, $enc)
Write-Host ("APPLIED=" + $applied + "/" + $patches.Count + " BYTES=" + ([System.IO.File]::ReadAllBytes($path)).Length)
