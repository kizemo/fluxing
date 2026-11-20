
## L230 — 「量字体反推 DPI」是错的；`weasel.sln` 只构建 Win32 ⇒ x64 TSF 产物必须走 `build.bat`（2026-11-20 批次 BB）

**症状**：① 按 stock 字体 `tm` 度量反推 DPI，本机得 **168dpi**（系统真值 144dpi，高 17%），会让布局整体超缩、反而裁字；② 只跑 `msbuild weasel.sln` 的门禁里 `artifact-version-resource` 红在 `weaselx64.dll`，而 `output\Win32\weaselx64.dll` **根本不存在**（x64 TSF 产物在 `output\weaselx64.dll`，只有 `build.bat` 会产出）。

**规则**：

- **缩放系数取系统 DPI，不取字体反推**：`GetDpiForSystem()`（user32，**动态 `GetProcAddress` 取** —— 静态导入会让 Win8.1 及更早系统加载失败）→ 回退 gdi32 `GetDeviceCaps(GetDC(nullptr), LOGPIXELSX)` → 兜底 96。**禁用 `GetDpiForMonitor`**（L122：会引入 `api-ms-win-shcore-scaling-l1-1-1.dll` 依赖）。
- **stock 字体 `tm` 行高 ≠ 控件单行高**：144dpi 下 `tmHeight + tmExternalLeading` = **21px**，而控件 `DT_CALCRECT` 单行只有 **18px** ⇒ 用行高反推 DPI 必然偏大（96dpi 机器同理得 112dpi）⇒ 直接违反「96dpi 逐像素不变」不变量。字体度量只能当**交叉校验告警**（偏差 >12.5% 记 `acctdlg` 日志），**不得**反过来决定布局。
- **不按窗口所在屏 DPI 缩放**（`GetDpiForWindow`）：stock 字体是**进程级**资源、按系统 DPI 光栅化；按窗口 DPI 缩放会与实际渲染失配 ⇒ 反而裁字。
- **不保留「无 dpi」兼容重载**：那个形态**就是**本缺陷的形态，留着就是复发入口；旧单测字面量断言改传基准 dpi 即可。
- **`weasel.sln` 只含 Win32 平台**：`TestFluxingCloud` **不在** sln 里（须显式构建，且须校验 `exe.mtime > src.mtime` 才不假绿）；**x64 TSF 产物只由 `build.bat` 权威路径产出** ⇒ 只跑 `msbuild weasel.sln` 的门禁会因「产物缺失 / 版本落后」假红。**版本 bump 后必须用 `build.bat weasel installer` 出包，再复跑 L0**（本轮实测：改走 `build.bat` 后 L0 立刻回到 15/15）。
- 追加 `lessons-learned.md` 用 `AppendAllText` + `UTF8Encoding($false)` + **显式 CRLF**（文件本体 UTF-8+BOM+CRLF，追加不能插裸 LF，否则 L0 的 `bareLF==0` 判红）。
