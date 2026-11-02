; ============================================================================
; guard_install_path_test.nsi — 安装路径守卫的判定逻辑单元测试（L169）
; ----------------------------------------------------------------------------
; 必须 !include 同一个 guard_install_path.nsh ⇒ 「测的代码 == 装的代码」。
;
; 用法（cwd = output）：
;   makensis /O<tmp>\guard_install_path_test.exe guard_install_path_test.nsi
;   <tmp>\guard_install_path_test.exe
;
; 产物：<输出目录>\guard_install_path_result.txt，每行一例：
;   OK       expected=BLOCK actual=BLOCK path=… reason=…
;   MISMATCH expected=ALLOW actual=BLOCK path=… reason=…
; 末行：TOTAL=<例数> MISMATCH=<不符例数>
; 退出码：0 = 全部符合预期；1 = 有用例与预期不符。
;
; 用例表同时包含「该拦的」与「不该拦的」——只有拦截样例通过不算数
; （L169 的直接教训：负对照必须能证明合法路径不会被误拦）。
; ============================================================================

Unicode true

!include "LogicLib.nsh"
!include "guard_install_path.nsh"

!ifndef GUARD_TEST_OUT
  !define GUARD_TEST_OUT "$EXEDIR"
!endif

; 产物位置：默认落在脚本同目录；可用 /DGUARD_TEST_OUTFILE=<绝对路径> 改到别处
!ifdef GUARD_TEST_OUTFILE
  OutFile "${GUARD_TEST_OUTFILE}"
!else
  OutFile "guard_install_path_test.exe"
!endif

Var GuardTestFile      ; 结果文件句柄
Var GuardCasePath      ; 当前用例的路径
Var GuardCaseExpected  ; 当前用例的期望结论
Var GuardCaseTotal     ; 用例总数
Var GuardCaseFail      ; 与预期不符的用例数

Function .onInit
  StrCpy $GuardCaseTotal 0
  StrCpy $GuardCaseFail 0

  ClearErrors
  FileOpen $GuardTestFile "${GUARD_TEST_OUT}\guard_install_path_result.txt" w
  ${If} $GuardTestFile == ""
    MessageBox MB_OK|MB_ICONSTOP "无法写入结果文件：${GUARD_TEST_OUT}\guard_install_path_result.txt"
    Abort
  ${EndIf}

  ; ---------------- 应拦截：L169 事故靶子 + 冒烟测试根 ----------------
  Push "BLOCK"
  Push "F:\soft\00selfmade\rime_claude\tmp\fluxing"
  Call GuardCase

  Push "BLOCK"
  Push "F:\soft\00selfmade\rime_claude\tmp"
  Call GuardCase

  Push "BLOCK"
  Push "F:\soft\00selfmade\rime_claude"
  Call GuardCase

  Push "BLOCK"
  Push "C:\TEMP\test"
  Call GuardCase

  Push "BLOCK"
  Push "C:\TEMP\test\fluxing"
  Call GuardCase

  Push "BLOCK"
  Push "C:\TEMP"
  Call GuardCase

  Push "BLOCK"
  Push "C:\Users\test"
  Call GuardCase

  Push "BLOCK"
  Push "D:\tmp\fluxing"
  Call GuardCase

  ; 以 \tmp 目录段结尾（覆盖「路径段 tmp」规则的结尾分支）
  Push "BLOCK"
  Push "E:\tmp"
  Call GuardCase

  ; ---------------- 应放行：产品默认根 / 普通路径 ----------------
  Push "ALLOW"
  Push "D:\Program Files\fluxing"
  Call GuardCase

  Push "ALLOW"
  Push "C:\Program Files\Fluxing"
  Call GuardCase

  Push "ALLOW"
  Push "E:\Apps\fluxing"
  Call GuardCase

  Push "ALLOW"
  Push "C:\fluxing"
  Call GuardCase

  ; ---------------- 应放行：负对照（名字相近但不是 tmp 段 / 不是 TEMP 根）--------
  Push "ALLOW"
  Push "D:\Program Files\fluxing\tmp2"
  Call GuardCase

  Push "ALLOW"
  Push "C:\tempfiles\app"
  Call GuardCase

  Push "ALLOW"
  Push "F:\tmpstore\fluxing"
  Call GuardCase

  FileWrite $GuardTestFile "TOTAL=$GuardCaseTotal MISMATCH=$GuardCaseFail$\r$\n"
  FileClose $GuardTestFile

  ${If} $GuardCaseFail != 0
    SetErrorLevel 1
  ${Else}
    ; Abort 的默认退出码是 2（NSIS「安装被中止」语义），这里显式归 0，
    ; 让「退出码 0 = 全部符合预期；1 = 有用例不符」这条契约成立。
    SetErrorLevel 0
  ${EndIf}
  Abort
FunctionEnd

; ----------------------------------------------------------------------------
; 跑一例：调用守卫的纯判定函数，把「期望 / 实测」写进结果文件
;   入栈：expected（先压）、path（后压）
; ----------------------------------------------------------------------------
Function GuardCase
  Pop $GuardCasePath
  Pop $GuardCaseExpected
  ; 守卫的纯判定函数读 $GuardTestPath（不是 $GuardCasePath）⇒ 必须先把本例
  ; 路径搬过去，否则守卫读到的是空串，首轮 8 条 BLOCK 会全被判成 ALLOW。
  StrCpy $GuardTestPath $GuardCasePath
  Call GuardCheckInstallPath
  IntOp $GuardCaseTotal $GuardCaseTotal + 1
  ${If} $GuardTestResult != $GuardCaseExpected
    IntOp $GuardCaseFail $GuardCaseFail + 1
    FileWrite $GuardTestFile "MISMATCH expected=$GuardCaseExpected actual=$GuardTestResult path=$GuardCasePath reason=$GuardTestReason$\r$\n"
  ${Else}
    FileWrite $GuardTestFile "OK       expected=$GuardCaseExpected actual=$GuardTestResult path=$GuardCasePath reason=$GuardTestReason$\r$\n"
  ${EndIf}
FunctionEnd

Section "GuardTest"
SectionEnd
