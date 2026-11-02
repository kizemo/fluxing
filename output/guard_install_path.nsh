; ============================================================================
; guard_install_path.nsh — Fluxing 安装路径守卫（L169 权威层）
; ----------------------------------------------------------------------------
; 背景（.specify/memory/lessons-learned.md L169，2026-11-03 真实事故）：
;   把工作区里的 tmp\ 当作安装目标传给静默安装包装器做负对照，NSIS 安装器
;   **真的执行了**：.onInit 先 taskkill 掉真机 WeaselServer.exe，再删掉写死的
;   D:\Program Files\Fluxing\weasel\WeaselServer.exe（NTFS 大小写不敏感 ⇒ 命
;   中真机），装机静默残废，注册表 4 处被改指到临时目录。
;
;   根因有两层：
;     ① 脚本层没有守卫 → 已修（scripts/install/silent-install.ps1 的 G1–G4）
;     ② **权威层（安装器本体）没有守卫** → 就是本文件补的
;
; 契约：
;   GuardInstallPath       入口。读 $INSTDIR，命中危险路径 ⇒ 写日志 +
;                           SetErrorLevel 2 + Abort（静默模式不弹窗）。
;                           必须在 install.nsi 的 .onInit **第一条语句**调用：
;                           早于 taskkill 循环与写死路径的 Delete，否则
;                           「先拦下来」这个核心价值就没有了。
;   GuardCheckInstallPath  纯判定（不 Abort）。输入 $GuardTestPath，输出
;                           $GuardTestResult = ALLOW/BLOCK 与 $GuardTestReason
;                           （简体中文原因）。测试脚本 guard_install_path_test.nsi
;                           include 同一个文件 ⇒ 「测的代码 == 装的代码」。
;
; 判定规则（大小写不敏感）：
;   BLOCK  含路径段 rime_claude   —— 源码工作区特征
;   BLOCK  含路径段 00selfmade   —— 源码工作区特征
;   BLOCK  含路径段 tmp          —— 临时目录
;   BLOCK  位于 C:\TEMP\ 下（含 C:\TEMP\test\ 与 C:\TEMP）
;   BLOCK  位于 C:\Users\test 下
;   ALLOW  其余一切路径：D:\Program Files\fluxing / C:\Program Files\Fluxing /
#          E:\Apps\fluxing / C:\fluxing ... 以及不含上述特征的普通路径
;
; 命中即**中止安装**，不改成「重置成默认路径」：静默忽略用户意图正是 L169
; 「装错地方还不吭声」的病根。
; ----------------------------------------------------------------------------
; 实现备注（NSIS 细节，改动前请先读）：
;   * ${StrStr} 是**大小写敏感**的，且命中时返回「命中位置之后的尾巴」；
;     为了大小写不敏感，这里先把路径转成小写副本（GuardLowerCase，纯 NSIS，
;     不依赖 System 插件 —— ${StrCase} 要 System.dll，装机包里没有它）。
;   * 「命中在字符串末尾」时尾巴为空，与「未命中」无法区分 ⇒ 查找时给被查
;     字符串尾部加哨兵 "|"（GuardProbe）。
;   * NSIS 的 StrCpy 参数顺序是 **length 在前、offset 在后**：
;     StrCpy dest src [length] [offset]（负 offset = 从末尾往前数）。
;   * StrFunc 的宏是内联展开的「伪函数」，必须先 ${Using:StrFunc} 声明，
;     否则编译期报 "You forgot ${Using:StrFunc}"。
;   * StrFunc 的实现会占用 $R0–$R8 ⇒ 本文件的状态一律放在 Var 里。
; ============================================================================

!include "LogicLib.nsh"
!include "StrFunc.nsh"

${Using:StrFunc} StrStr
${Using:StrFunc} StrRep

Var GuardTestPath      ; 待判定的安装根（入口写 $INSTDIR；测试脚本写用例）
Var GuardTestResult    ; "ALLOW" / "BLOCK"
Var GuardTestReason    ; 命中原因（简体中文）
Var GuardLower         ; 路径的小写副本（大小写不敏感判定用）
Var GuardHit           ; ${StrStr} 的返回值（命中时非空）
Var GuardNeedle        ; 当前被查找的特征串
Var GuardWhy           ; 当前特征串对应的原因
Var GuardSeg           ; 路径段 / 前缀临时变量
Var GuardChar          ; 单字符临时变量
Var GuardCharLow       ; 对应的小写字符
Var GuardIdx           ; 循环下标
Var GuardUpperAlpha    ; 大写字母表
Var GuardLowerAlpha    ; 小写字母表

; ----------------------------------------------------------------------------
; 入口：install.nsi 的 .onInit 第一条语句调用
; ----------------------------------------------------------------------------
Function GuardInstallPath
  StrCpy $GuardTestPath $INSTDIR
  Call GuardCheckInstallPath

  ; 日志留痕：事后能从日志判断「是不是被守卫拦下的」以及「拦的是哪个路径」。
  ;   只用 DetailPrint：本机 makensis 未定义 NSIS_CONFIG_LOG（LogSet / LogText
  ;   会报 "LogSet specified, NSIS_CONFIG_LOG not defined"），而 DetailPrint 的
  ;   内容会被 /LOG= 的安装日志完整记录。
  ${If} $GuardTestResult == "BLOCK"
    DetailPrint "Fluxing 守卫：拒绝安装 —— $GuardTestReason"
    DetailPrint "Fluxing 守卫：被拒绝的安装根 = $GuardTestPath"
    SetErrorLevel 2
    IfSilent guard_install_path_abort
    MessageBox MB_OK|MB_ICONSTOP "安装已中止：$GuardTestReason$\n$\n目标安装路径：$GuardTestPath$\n$\nFluxing 不允许安装到临时目录、源码工作区或冒烟测试目录。$\n请改用产品安装根（例如 D:\Program Files\fluxing）后重试。"
  guard_install_path_abort:
    Abort
  ${Else}
    DetailPrint "Fluxing 守卫：放行 —— 安装根 = $GuardTestPath"
  ${EndIf}
FunctionEnd

; ----------------------------------------------------------------------------
; 纯判定：只给结论，不 Abort。测试脚本直接调用它。
;   输入  $GuardTestPath
;   输出  $GuardTestResult = "ALLOW" / "BLOCK"
;         $GuardTestReason = 原因（ALLOW 时为空）
; ----------------------------------------------------------------------------
Function GuardCheckInstallPath
  StrCpy $GuardTestResult "ALLOW"
  StrCpy $GuardTestReason ""
  StrCpy $GuardLower $GuardTestPath
  Call GuardLowerCase

  ; ---- BLOCK：源码工作区特征 rime_claude ----
  Push "路径命中源码工作区特征 rime_claude（源码工作区不是产品安装根）"
  Push "rime_claude"
  Call GuardProbe
  ${If} $GuardHit != ""
    StrCpy $GuardTestResult "BLOCK"
    StrCpy $GuardTestReason $GuardWhy
    Return
  ${EndIf}

  ; ---- BLOCK：源码工作区特征 00selfmade ----
  Push "路径命中源码工作区特征 00selfmade（源码工作区不是产品安装根）"
  Push "00selfmade"
  Call GuardProbe
  ${If} $GuardHit != ""
    StrCpy $GuardTestResult "BLOCK"
    StrCpy $GuardTestReason $GuardWhy
    Return
  ${EndIf}

  ; ---- BLOCK：tmp 路径段（\tmp\ …） ----
  Push "路径含 tmp 目录段（临时目录不是产品安装根）"
  Push "\tmp\"
  Call GuardProbe
  ${If} $GuardHit != ""
    StrCpy $GuardTestResult "BLOCK"
    StrCpy $GuardTestReason $GuardWhy
    Return
  ${EndIf}

  ; ---- BLOCK：路径以 tmp 目录段结尾（…\tmp） ----
  StrCpy $GuardSeg $GuardLower "" -4
  ${If} $GuardSeg == "\tmp"
    StrCpy $GuardTestResult "BLOCK"
    StrCpy $GuardTestReason "路径以 tmp 目录段结尾（临时目录不是产品安装根）"
    Return
  ${EndIf}

  ; ---- BLOCK：C:\TEMP\ 冒烟测试根（含 C:\TEMP\test\） ----
  ; "c:\temp\" 共 8 个字符（c : \ t e m p \）
  StrCpy $GuardSeg $GuardLower 8
  ${If} $GuardSeg == "c:\temp\"
    StrCpy $GuardTestResult "BLOCK"
    StrCpy $GuardTestReason "路径位于 C:\TEMP\ 冒烟测试根（临时目录不是产品安装根）"
    Return
  ${EndIf}

  ; ---- BLOCK：C:\TEMP 恰好（没有尾斜杠） ----
  ; "c:\temp" 共 7 个字符（c : \ t e m p）；下标 7 取第 8 个字符
  StrCpy $GuardSeg $GuardLower 7
  ${If} $GuardSeg == "c:\temp"
    StrCpy $GuardChar $GuardLower 1 7
    ${If} $GuardChar == ""
      StrCpy $GuardTestResult "BLOCK"
      StrCpy $GuardTestReason "路径位于 C:\TEMP 冒烟测试根（临时目录不是产品安装根）"
      Return
    ${EndIf}
    ${If} $GuardChar == "\"
      StrCpy $GuardTestResult "BLOCK"
      StrCpy $GuardTestReason "路径位于 C:\TEMP\ 冒烟测试根（临时目录不是产品安装根）"
      Return
    ${EndIf}
  ${EndIf}

  ; ---- BLOCK：C:\Users\test 冒烟测试根 ----
  ;      前缀语义与既有 check_reg（install.nsi）一致：宁可多拦，不可漏拦。
  ; "c:\users\test" 共 13 个字符（c : \ u s e r s \ t e s t）
  StrCpy $GuardSeg $GuardLower 13
  ${If} $GuardSeg == "c:\users\test"
    StrCpy $GuardTestResult "BLOCK"
    StrCpy $GuardTestReason "路径位于 C:\Users\test 冒烟测试根（临时目录不是产品安装根）"
    Return
  ${EndIf}

  ; ---- ALLOW ----
  StrCpy $GuardTestResult "ALLOW"
  StrCpy $GuardTestReason ""
FunctionEnd

; ----------------------------------------------------------------------------
; 在 $GuardLower（路径）里查找特征串。
;   入栈：needle（先压）、reason（后压）
;   出参：$GuardHit 非空 = 命中；$GuardWhy = 命中原因
; ----------------------------------------------------------------------------
Function GuardProbe
  ; 调用方压栈顺序：先压 reason，再压 needle（needle 在栈顶）
  Pop $GuardNeedle
  Pop $GuardWhy
  ; 哨兵 "|"：${StrStr} 命中时返回「命中位置之后的尾巴」，命中在末尾时尾巴为
  ; 空 ⇒ 与「未命中」无法区分；给被查串尾部加一个哨兵即可区分。
  ${StrStr} $GuardHit "$GuardLower|" $GuardNeedle
FunctionEnd

; ----------------------------------------------------------------------------
; 把 $GuardLower 里的 ASCII 大写字母换成小写（纯 NSIS，不依赖 System 插件）
; ----------------------------------------------------------------------------
Function GuardLowerCase
  StrCpy $GuardUpperAlpha "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  StrCpy $GuardLowerAlpha "abcdefghijklmnopqrstuvwxyz"
  StrCpy $GuardIdx 0
  ${Do}
    ; StrCpy dest src [length] [offset]：length 在前、offset 在后
    StrCpy $GuardChar $GuardUpperAlpha 1 $GuardIdx
    StrCpy $GuardCharLow $GuardLowerAlpha 1 $GuardIdx
    ${StrRep} $GuardLower $GuardLower $GuardChar $GuardCharLow
    IntOp $GuardIdx $GuardIdx + 1
  ${LoopUntil} $GuardIdx >= 26
FunctionEnd
