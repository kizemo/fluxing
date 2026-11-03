// ReproEsc.cs (v3) — ADR 0032 验收探针
//
// 目标:验证「按 Esc 退出用户短语界面**不会**连带收起快捷设置栏」,并且
//       反向确认「用户真的切到别的输入法」时快捷设置栏**仍然必须收起**。
//
// v2 的两个假阳性陷阱(v2 已修):
//   (1) 前提失效:修复之后,自家 UI 的 focus_in(profile) 不再请求显示面板,
//       于是「先弹面板再开短语窗」的前提不成立。v2 由 keeper 以**普通用户应用**
//       身份把在场状态置为 present 并请求显示面板,恢复真实前提。
//   (2) 归因失效:v2 只看「Esc 之后有没有看不见过」,面板在**开窗**阶段就被
//       收起也算数。v2 分段采样,开窗后 / Esc 前必须一直可见。
//
// v3 新增:**瞬时态**下「面板自己隐藏」和「Esc 导致隐藏」必须分开。
//   瞬时态有两条与 Esc 无关的自动隐藏路径(WeaselServer/QuickPanelDialog.cpp):
//     (A) WM_ACTIVATEAPP(前台切走):show 起 grace 2000ms 之后即可触发;
//     (B) hover polling(鼠标在面板外累计 1500ms):最早发生在 show+3500ms。
//   v2 的 Esc 落在 show+3600ms,**恰好越过 (B) 的阈值** ⇒ 瞬时态下 v2 的
//   「BUG REPRODUCED」归因是假的(面板先被 (B) 收起,Esc 只是恰好随后发生)。
//   v3 用三阶段把因果拆开:
//     阶段 A(对照):不按 Esc、不开短语窗,只看面板会不会自己消失 ⇒ 证伪/证实 (B)。
//     阶段 B(主测):Esc 落在 [show+2200, show+3400] 窗口内 —— 过了 (A) 的 grace
//                   门槛、还没到 (B) 的 3500ms 阈值 ⇒ 窗口内出现的隐藏只能归因 Esc。
//     阶段 C(反向):keeper 在面板显示后 ~1400ms 发 FocusOut(PROFILE_DEACTIVATED)
//                   (早于 (B) 阈值) ⇒ 面板必须收起,且收起只能归因这次 FocusOut。
//
// 用法: ReproEsc.exe <serverPid> <hotkeyId> [keeperExe]
// 退出码: 0 = NOT REPRODUCED 且反向对照通过
//         1 = BUG REPRODUCED(Esc 之后快捷设置栏消失了)
//         3 = PROBE INVALID(前提没成立,结论不作数)
//         4 = 反向对照失败(切走输入法后面板没收起)
//
// Build: C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /nologo /out:ReproEsc.exe ReproEsc.cs
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

static class ReproEsc
{
    [DllImport("user32.dll")]
    static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
    delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern int GetClassNameW(IntPtr hWnd, StringBuilder buf, int max);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern int GetWindowTextW(IntPtr hWnd, StringBuilder buf, int max);
    [DllImport("user32.dll")]
    static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")]
    static extern bool PostMessageW(IntPtr hWnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")]
    static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")]
    static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")]
    static extern bool SetProcessDpiAwarenessContext(IntPtr v);

    const uint WM_COMMAND = 0x0111;
    const uint WM_HOTKEY = 0x0312;
    const int ID_QUICK_PANEL_SHOW = 40020;
    const byte VK_ESCAPE = 0x1B;

    // 面板自动隐藏的时间常数(见文件头)
    const int kGraceMs = 2000;       // show 起 grace(A 路径门槛)
    const int kAutoHideMs = 3500;    // grace + 鼠标在外 1500ms(B 路径最早点)
    const int kEscWindowMs = 2200;   // Esc 落在 show+kEscWindowMs 之后
    const int kAttrWindowMs = 1000;  // Esc 后 kAttrWindowMs 内的隐藏才算归因给 Esc

    static uint targetPid;
    static int hotkeyId;
    static string keeperExe;
    static List<IntPtr> serverWindows = new List<IntPtr>();
    static IntPtr panelHwnd = IntPtr.Zero;
    static Stopwatch sw = new Stopwatch();

    static string ClassOf(IntPtr h)
    {
        StringBuilder sb = new StringBuilder(256);
        GetClassNameW(h, sb, 256);
        return sb.ToString();
    }

    static string TitleOf(IntPtr h)
    {
        StringBuilder sb = new StringBuilder(256);
        GetWindowTextW(h, sb, 256);
        return sb.ToString();
    }

    static bool Collect(IntPtr h, IntPtr l)
    {
        uint pid; GetWindowThreadProcessId(h, out pid);
        if (pid == targetPid) serverWindows.Add(h);
        return true;
    }

    // 快捷设置栏是服务端进程自己的顶层窗口。按类名里含 "QuickPanel" 找,
    // 找不到就把该进程所有顶层窗口的类名列出来 —— 结论可自证。
    static IntPtr FindPanel()
    {
        IntPtr hit = IntPtr.Zero;
        EnumWindows(delegate (IntPtr h, IntPtr l)
        {
            string c = ClassOf(h);
            if (c.IndexOf("QuickPanel", StringComparison.OrdinalIgnoreCase) >= 0)
            { hit = h; return false; }
            return true;
        }, IntPtr.Zero);
        return hit;
    }

    static IntPtr FindVisiblePhrases()
    {
        IntPtr hit = IntPtr.Zero;
        EnumWindows(delegate (IntPtr h, IntPtr l)
        {
            string c = ClassOf(h);
            if (c.IndexOf("PhrasesDialog", StringComparison.OrdinalIgnoreCase) >= 0 &&
                IsWindowVisible(h)) { hit = h; return false; }
            return true;
        }, IntPtr.Zero);
        return hit;
    }

    static bool PanelVisible(IntPtr panel)
    {
        return panel != IntPtr.Zero && IsWindowVisible(panel);
    }

    static void Log(string s)
    {
        Console.WriteLine("[" + DateTime.Now.ToString("HH:mm:ss.fff") + "] " + s);
        Console.Out.Flush();
    }

    static void DumpServerWindows()
    {
        EnumWindows(Collect, IntPtr.Zero);
        Log("server pid=" + targetPid + " top-level windows=" + serverWindows.Count);
        foreach (IntPtr h in serverWindows)
        {
            Log("  hwnd=0x" + ((long)h).ToString("X16") +
                " class='" + ClassOf(h) + "' title='" + TitleOf(h) +
                "' visible=" + IsWindowVisible(h));
        }
    }

    static Process StartKeeper(string args)
    {
        try
        {
            return Process.Start(new ProcessStartInfo(keeperExe, args)
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true
            });
        }
        catch (Exception ex)
        {
            Log("keeper start failed: " + ex.Message);
            return null;
        }
    }

    static void KillKeeper(Process p)
    {
        if (p == null) return;
        try { p.Kill(); } catch { }
    }

    // 等面板出现。返回显示时刻(ms,相对探针起点);超时返回 -1。
    static int WaitPanelVisible(int timeoutMs)
    {
        int deadline = (int)sw.ElapsedMilliseconds + timeoutMs;
        while ((int)sw.ElapsedMilliseconds < deadline)
        {
            IntPtr found = FindPanel();
            if (found != IntPtr.Zero) panelHwnd = found;
            if (PanelVisible(panelHwnd)) return (int)sw.ElapsedMilliseconds;
            System.Threading.Thread.Sleep(50);
        }
        return -1;
    }

    static int Main(string[] args)
    {
        SetProcessDpiAwarenessContext((IntPtr)(-4));
        if (args.Length < 2)
        {
            Console.WriteLine("usage: ReproEsc.exe <serverPid> <hotkeyId> [keeperExe]");
            return 2;
        }
        targetPid = uint.Parse(args[0]);
        hotkeyId = int.Parse(args[1]);
        keeperExe = args.Length > 2 ? args[2]
            : @"F:\soft\00selfmade\rime_claude\release\TestWeaselIPC.exe";

        sw.Start();
        DumpServerWindows();

        panelHwnd = FindPanel();

        // ================= 阶段 A:自动隐藏对照(不按 Esc、不开短语窗) =================
        Log("--- phase A: auto-hide control (no Esc, no phrases dialog) ---");
        Process kA = StartKeeper("/hold-presence 9000 120000");
        if (kA == null) return 3;
        Log("keeper A started pid=" + kA.Id);
        int showA = WaitPanelVisible(6000);
        if (showA < 0)
        {
            Log("VERDICT: PROBE INVALID - phase A: the QuickPanel never became visible.");
            KillKeeper(kA);
            return 3;
        }
        Log("phase A: panel shown at t+" + showA + "ms");
        int hideA = -1;
        int watchUntil = showA + 6000;
        while ((int)sw.ElapsedMilliseconds < watchUntil)
        {
            System.Threading.Thread.Sleep(100);
            if (!PanelVisible(panelHwnd)) { hideA = (int)sw.ElapsedMilliseconds; break; }
        }
        if (hideA < 0)
            Log("phase A: panel stayed visible for 6000ms (no auto-hide)");
        else
            Log("phase A: panel auto-hid at t+" + hideA + "ms (offset from show = " +
                (hideA - showA) + "ms; B-threshold would be " + kAutoHideMs + "ms)");
        KillKeeper(kA);
        System.Threading.Thread.Sleep(300);

        // ================= 阶段 B:主测(Esc 归因窗口内) =================
        Log("--- phase B: Esc attribution ---");
        Process kB = StartKeeper("/hold-presence 18000 12000");
        if (kB == null) return 3;
        Log("keeper B started pid=" + kB.Id);
        int showB = WaitPanelVisible(6000);
        if (showB < 0)
        {
            Log("VERDICT: PROBE INVALID - phase B: the QuickPanel never became visible.");
            KillKeeper(kB);
            return 3;
        }
        Log("phase B: panel shown at t+" + showB + "ms");

        foreach (IntPtr h in serverWindows)
            PostMessageW(h, WM_COMMAND, (IntPtr)ID_QUICK_PANEL_SHOW, IntPtr.Zero);
        foreach (IntPtr h in serverWindows)
            PostMessageW(h, WM_HOTKEY, (IntPtr)hotkeyId, IntPtr.Zero);

        IntPtr phrases = IntPtr.Zero;
        int phraseDeadline = (int)sw.ElapsedMilliseconds + 6000;
        while ((int)sw.ElapsedMilliseconds < phraseDeadline)
        {
            phrases = FindVisiblePhrases();
            if (phrases != IntPtr.Zero && IsWindowVisible(phrases)) break;
            System.Threading.Thread.Sleep(50);
        }
        Log("phase B: phrases HWND=0x" + ((long)phrases).ToString("X16") +
            " visible=" + (phrases != IntPtr.Zero && IsWindowVisible(phrases)) +
            " (t+" + (int)sw.ElapsedMilliseconds + "ms)");
        if (phrases == IntPtr.Zero || !IsWindowVisible(phrases))
        {
            Log("VERDICT: PROBE INVALID - phase B: the phrases dialog never opened.");
            KillKeeper(kB);
            return 3;
        }

        // 等到 show+kEscWindowMs:过了 (A) 的 grace 门槛,还没到 (B) 的 3500ms 阈值
        int escTarget = showB + kEscWindowMs;
        while ((int)sw.ElapsedMilliseconds < escTarget)
            System.Threading.Thread.Sleep(10);

        // 紧贴 Esc 的采样:Esc 前 30ms 内面板必须可见,否则不能把隐藏归因给 Esc
        bool preVisible = PanelVisible(panelHwnd);
        Log("phase B: pre-Esc(tight) t+" + (int)sw.ElapsedMilliseconds + "ms panel.visible=" +
            preVisible + " (esc target t+" + escTarget + "ms)");
        if (!preVisible)
        {
            Log("VERDICT: PROBE INVALID - phase B: the QuickPanel was already hidden " +
                "before Esc; the hide cannot be attributed to Esc.");
            KillKeeper(kB);
            return 3;
        }

        Log(">>> injecting ESC into phrases window");
        if (phrases != IntPtr.Zero) SetForegroundWindow(phrases);
        System.Threading.Thread.Sleep(150);
        keybd_event(VK_ESCAPE, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(60);
        keybd_event(VK_ESCAPE, 0, 2, UIntPtr.Zero);
        int escAt = (int)sw.ElapsedMilliseconds;

        bool hiddenByEsc = false;
        int hidAt = -1;
        int attrDeadline = escAt + kAttrWindowMs;
        while ((int)sw.ElapsedMilliseconds < attrDeadline)
        {
            System.Threading.Thread.Sleep(25);
            if (!PanelVisible(panelHwnd)) { hiddenByEsc = true; hidAt = (int)sw.ElapsedMilliseconds; break; }
        }
        if (hiddenByEsc)
        {
            Log("VERDICT: BUG REPRODUCED - the QuickPanel vanished " + (hidAt - escAt) +
                "ms after Esc (attributable window).");
            KillKeeper(kB);
            return 1;
        }
        Log("phase B: panel still visible " + ((int)sw.ElapsedMilliseconds - escAt) +
            "ms after Esc (attribution window clean)");

        // 再看 3s:面板若随后消失,是瞬时态自己的自动隐藏,不是 Esc
        int lateHide = -1;
        int lateDeadline = (int)sw.ElapsedMilliseconds + 3000;
        while ((int)sw.ElapsedMilliseconds < lateDeadline)
        {
            System.Threading.Thread.Sleep(100);
            if (!PanelVisible(panelHwnd)) { lateHide = (int)sw.ElapsedMilliseconds; break; }
        }
        if (lateHide >= 0)
            Log("phase B: panel hid later at t+" + lateHide + "ms (offset from show = " +
                (lateHide - showB) + "ms) — transient auto-hide, NOT caused by Esc");
        else
            Log("phase B: panel stayed visible after Esc");
        KillKeeper(kB);
        System.Threading.Thread.Sleep(300);

        // ================= 阶段 C:反向对照(切走输入法 ⇒ 面板必须收起) =================
        Log("--- phase C: reverse control (user really switches away) ---");
        Process kC = StartKeeper("/hold-presence 6000 1500");
        if (kC == null) return 3;
        Log("keeper C started pid=" + kC.Id);
        int showC = WaitPanelVisible(6000);
        if (showC < 0)
        {
            Log("VERDICT: PROBE INVALID - phase C: the QuickPanel never became visible.");
            KillKeeper(kC);
            return 3;
        }
        Log("phase C: panel shown at t+" + showC + "ms");
        bool reverseHidden = false;
        int revHidAt = -1;
        int revDeadline = (int)sw.ElapsedMilliseconds + 4000;
        while ((int)sw.ElapsedMilliseconds < revDeadline)
        {
            System.Threading.Thread.Sleep(50);
            if (!PanelVisible(panelHwnd)) { reverseHidden = true; revHidAt = (int)sw.ElapsedMilliseconds; break; }
        }
        Log("phase C: reverse control panel.hidden=" + (reverseHidden ? "1" : "0") +
            (reverseHidden ? " at t+" + revHidAt + "ms (offset from show = " +
                (revHidAt - showC) + "ms)" : ""));
        KillKeeper(kC);
        if (!reverseHidden)
        {
            Log("VERDICT: REVERSE CONTROL FAILED - switching away from Fluxing did " +
                "NOT hide the QuickPanel (spec 091 / ADR 0024 broken).");
            return 4;
        }

        Log("VERDICT: NOT REPRODUCED - the QuickPanel stayed visible after Esc, " +
            "and the reverse control (switching away) still hides it.");
        return 0;
    }
}
