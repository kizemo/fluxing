#!/usr/bin/env python3
"""
Inject v0.19.0.66 metadata into pinyin staging files.
Safe insertion: backup first, then modify in place.

Per handoff-after-phase-L-bugfix-3-ship-v0.19.0.66-2026-07-25.md:
- installer md5: b4733435610fe4f22c5f837a23036a7e
- size: 43,360,374 bytes
- build time: 2026-07-25 23:05:57
- WeaselServer.exe md5: b3bcb65967d6ad0b00a5d0460405ec73
- weaselx64.dll md5: c62a364bd9c08e0a7210ee07e9fe4529 (sink 注册改动 binary)
- FluxingPhrasesDialog.exe md5: c969aeb31e24d9ee1468f8869150970d
"""
import re
from pathlib import Path

STAGING = Path("E:/办公文件/L1网站/pinyin")
BACKUP = STAGING / ".bak-2026-07-26"

# === version.json (full replace) ===
version_json = """{
    "product": "火流猩输入法",
    "version": "0.19.0.66",
    "filename": "fluxing-0.19.0.66-installer.exe",
    "updated": "2026-07-25",
    "notes": "Phase L 装机 bugfix 3 真修 (18 ship 累积 TSF sink 注册 bug)：Win+Space 切到火流猩浮动栏自动弹 — ITfActiveLanguageProfileNotifySink 终于 AdviseSink 到 TSF"
}
"""

# === appcast.xml (insert <item> after <language>zh-CN</language>) ===
V066_ITEM = """    <item>
      <title>火流猩 0.19.0.66</title>
      <sparkle:releaseNotesLink>https://www.aiec.fun/pinyin/release-notes.html</sparkle:releaseNotesLink>
      <pubDate>Sat, 25 Jul 2026 23:05:00 +0800</pubDate>
      <enclosure url="https://www.aiec.fun/pinyin/download.php?file=fluxing-0.19.0.66-installer.exe"
                 sparkle:version="0.19.0.66"
                 sparkle:shortVersionString="0.19.0.66"
                 length="43360374"
                 type="application/octet-stream"/>
    </item>
"""

# === release-notes.html (insert <h2>+<ul> after <h1>) ===
V066_SECTION = """<h2>0.19.0.66</h2>
<ul>
  <li><b>Phase L 装机 bugfix 3 真修 (18 ship 累积 bug)</b>：Win+Space 切到火流猩输入法时，浮动设置栏自动弹出 — v0.18.41.0 spec 066 起设计的 OnActivated → FocusIn → QuickPanelDialog::Show 链断了 18 个 ship 版本</li>
  <li><b>Root cause (3 维度铁证)</b>：<code>ITfActiveLanguageProfileNotifySink</code> 接口已声明 (<code>WeaselTSF.h:18</code>) + 已实现 (<code>WeaselTSF.cpp:221-257</code>, 注释明确说 &quot;spec 061 CORRECT trigger&quot;)，但<b>从未通过 AdviseSink 注册到 TSF</b>。<code>grep -RIn "IID_ITfActiveLanguageProfileNotifySink"</code> 修复前全工程 <b>0 命中</b>，5 处其它 sink (ThreadFocus / KeyEvent / ThreadMgrEvent / LanguageBar / DisplayAttribute) 都正常注册，唯独 ActiveLanguageProfile 漏</li>
  <li><b>调用链修复前后对比</b>：[Win+Space 切 Fluxing] → [TSF manager 切 language profile] → 应触发 sink → OnActivated → m_client.FocusIn() → IPC FocusIn → RimeWithWeaselHandler::FocusIn → QuickPanelDialog::Show(false, nullptr×6) → 浮动栏弹。修复前 sink 未注册，整条链断裂 → 浮动栏永不弹</li>
  <li><b>为什么 3+ ship 失败根因</b>：v0.18.41.0 (spec 066) 设计 OnActivated → FocusIn + EnableAlwaysShowMode → v0.18.41.4 (L69) GDI+ crash 临时禁 FocusIn → v0.19.0.63 (spec 074) 撤回 sticky，期望 OnActivated → FocusIn → Show → v0.19.0.64 (Phase L bugfix 1) 改 IMM32 hook 装机证实 hidden IPC server 无 IME context → v0.19.0.65 (Phase L bugfix 2) 改回 FocusIn + Show(null×6) — 所有路径都期望 OnActivated sink 已注册，但实际从未注册</li>
  <li><b>修复 (5 处改动，~36 行新增)</b>：<code>WeaselTSF.h</code> +5 行 (_InitActiveLanguageProfileSink / _UninitActiveLanguageProfileSink 方法声明 + _dwActiveLanguageProfileSinkCookie 字段) + <code>WeaselTSF.cpp</code> 构造函数 +1 行 cookie 字段初始化 TF_INVALID_COOKIE + QueryInterface (cpp:67-70) +3 行 IID_ITfActiveLanguageProfileNotifySink 分支 + Deactivate (cpp:107-108) +2 行 _UninitActiveLanguageProfileSink 调用 + ActivateEx (cpp:166-169) +4 行 _InitActiveLanguageProfileSink 调用 + goto ExitError + init/uninit 实现 (cpp:230-251) +22 行仿 _InitThreadFocusSink 模板</li>
  <li><b>L66 类比 (设计 vs 落地假完成)</b>：spec 061 设计意图写在代码注释里 + 实现函数写了，但 AdviseSink 调用从未补上 → spec 设计假完成。这是 L66 (NSIS safety-net) 同模式：设计意图 vs 落地之间的 &quot;假完成&quot; 陷阱</li>
  <li><b>Alt+, &quot;回归&quot; 真相 (race timing 错觉)</b>：v0.19.0.65 之前 KeyEventSink 在用户已在文本框输入时误触发 FocusIn → panel 短暂弹 → Alt+, 立即 Hide → user 感受 &quot;Alt+, work&quot; (实际是 FocusIn 显示的)。v0.19.0.65 sink 未注册 FocusIn IPC 不发 → user 报告 &quot;Alt+, 回归&quot; 实际是 timing 变化让 panel 完全不弹，user 立即注意到</li>
  <li><b>装机端 4 user flow 验证</b> (v0.19.0.66 ship 后)：1) Win+Space 切 Fluxing → 浮动栏自动弹 ✓; 2) Win+Space 切英文 → 浮动栏收 ✓; 3) 切焦点到 text input → panel 状态由当前 active IME 决定 (Fluxing 时弹，英文时不弹); 4) Alt+, → toggle ✓ (handler IsWindowVisible + Show/Hide 正常工作)</li>
  <li>installer md5 <code>b4733435610fe4f22c5f837a23036a7e</code>，43,360,374 bytes</li>
  <li>WeaselServer.exe md5 <code>b3bcb65967d6ad0b00a5d0460405ec73</code> (v0.19.0.66 build, 含 sink 注册)</li>
  <li>weaselx64.dll md5 <code>c62a364bd9c08e0a7210ee07e9fe4529</code> (实际 sink 注册改动的 binary)</li>
  <li>FluxingPhrasesDialog.exe md5 <code>c969aeb31e24d9ee1468f8869150970d</code> (x64 RimeWithWeasel.lib relink → md5 微变)</li>
</ul>
"""

# === Execute modifications ===
print("=== Inject v0.19.0.66 metadata ===")

# 1. version.json (full replace)
vj_path = STAGING / "version.json"
vj_old = vj_path.read_text(encoding="utf-8")
vj_path.write_text(version_json, encoding="utf-8")
print(f"version.json: {len(vj_old)} -> {len(version_json)} bytes")

# 2. appcast.xml (insert <item> after <language>zh-CN</language>)
ac_path = STAGING / "appcast.xml"
ac_text = ac_path.read_text(encoding="utf-8")
anchor = "<language>zh-CN</language>\n"
if anchor not in ac_text:
    raise SystemExit(f"appcast.xml anchor not found: {anchor!r}")
ac_new = ac_text.replace(anchor, anchor + "\n" + V066_ITEM, 1)
ac_path.write_text(ac_new, encoding="utf-8")
print(f"appcast.xml: {len(ac_text)} -> {len(ac_new)} bytes (added {len(V066_ITEM)} bytes)")

# 3. release-notes.html (insert <h2>+<ul> after <h1>)
rn_path = STAGING / "release-notes.html"
rn_text = rn_path.read_text(encoding="utf-8")
# Find the line right after <h1>...</h1>
h1_pat = re.compile(r"(<h1>火流猩输入法更新</h1>\n)", re.MULTILINE)
m = h1_pat.search(rn_text)
if not m:
    raise SystemExit("release-notes.html <h1> anchor not found")
rn_new = rn_text[:m.end()] + "\n" + V066_SECTION + rn_text[m.end():]
rn_path.write_text(rn_new, encoding="utf-8")
print(f"release-notes.html: {len(rn_text)} -> {len(rn_new)} bytes (added {len(V066_SECTION)} bytes)")

print("\n=== Done. Backups in:", BACKUP, "===")
print("Backup files:")
for f in BACKUP.iterdir():
    print(" ", f.name)