#pragma once
// spec 099 P0-7 (2026-11-04): 快捷设置栏位置持久化 —— 落盘格式 + 钳制算法
// (header-only 单一事实源)。
//
// 为什么单独成文件(与 include/FluxingAlwaysShow.h 同一条理由):位置读写与钳制
// 要被**两个**模块同时用到 ——
//   - WeaselServer/QuickPanelDialog.cpp:建窗时按持久化位置落面板、拖动结束时
//     写回、DPI 变化时把当前位置钳回可视区;
//   - test/TestQuickPanelDialog:判别性测试(跨重启保留、分辨率/DPI 变化后钳制、
//     多屏负坐标)。
// 逻辑若留在 .cpp 里就只能靠间接断言覆盖 ⇒ 下沉成共享头,生产代码与测试同源
// (constitution R7 的精神)。
//
// 关键约束:**不得污染 quickpanel.state 的单一真相源语义(R7)**。
// quickpanel.state 的格式是「单行 always_show=0|1」,其读盘判据
// (always_show::ResolveAlwaysShowFromFiles)依赖「第一行内容精确匹配」——
// 把位置塞进同一个文件会改变该判据的语义 ⇒ 位置必须落在**独立文件**
// quickpanel.position(ADR 0035)。
//
// 依赖面:<filesystem> + <fstream> + <optional> + <string> + <vector> +
// windows.h(经 FluxingUserPaths.h)+ user32(枚举显示器工作区)。

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <FluxingUserPaths.h>

namespace fluxing {
namespace panel_position {

// ===== 类型 =====

// 屏幕矩形。**允许负值** —— 左侧副屏的工作区 left/top 是负数(多屏负坐标)。
// 不直接用 RECT:共享头不额外引 windows.h 之外的依赖,且 Rect 要能进
// std::vector 做纯函数计算。
struct Rect {
  int left;
  int top;
  int right;
  int bottom;
};

struct Point {
  int x;
  int y;
};

inline int RectWidth(const Rect& r) {
  return r.right - r.left;
}

inline int RectHeight(const Rect& r) {
  return r.bottom - r.top;
}

// inner 是否完整落在 outer 内(含边界)。
inline bool RectContains(const Rect& outer, const Rect& inner) {
  return inner.left >= outer.left && inner.top >= outer.top &&
         inner.right <= outer.right && inner.bottom <= outer.bottom;
}

// ===== 落点 =====

// 位置文件名(不含目录)。**独立于 quickpanel.state**(ADR 0035)。
inline const wchar_t* PositionFileName() {
  return L"quickpanel.position";
}

// 默认落点:<用户数据目录>\quickpanel.position(与 quickpanel.state 同源目录,
// 但**不同文件** —— 见文件头 R7 说明)。
inline std::wstring DefaultPositionPath() {
  std::filesystem::path dir = ResolveFluxingUserDataPath();
  if (dir.empty())
    return L"quickpanel.position";
  return (dir / std::filesystem::path(PositionFileName())).wstring();
}

// 测试注入的落点(镜像 always_show::StatePathOverrideRef)。
inline std::wstring& PositionPathOverrideRef() {
  static std::wstring path;
  return path;
}

inline void SetPositionPathOverride(const std::wstring& path) {
  PositionPathOverrideRef() = path;
}

inline bool HasPositionPathOverride() {
  return !PositionPathOverrideRef().empty();
}

inline std::wstring ResolvePositionPath() {
  const std::wstring& ov = PositionPathOverrideRef();
  if (!ov.empty())
    return ov;
  return DefaultPositionPath();
}

// ===== 序列化 =====

// 文件内容:两行 ASCII "pos_x=<int>\npos_y=<int>\n"。
inline std::string FormatPosition(int x, int y) {
  return "pos_x=" + std::to_string(x) + "\npos_y=" + std::to_string(y) + "\n";
}

// 严格解析十进制整数:允许前导 '-',不允许空格 / 小数 / 尾巴多余字符。
// 超出 int 范围 ⇒ 不认(避免溢出进后续几何计算)。
inline std::optional<int> ParseIntStrict(const std::string& s) {
  if (s.empty())
    return std::nullopt;
  size_t i = 0;
  bool neg = false;
  if (s[0] == '-' || s[0] == '+') {
    neg = (s[0] == '-');
    i = 1;
  }
  if (i >= s.size())
    return std::nullopt;
  long long v = 0;
  const long long limit = neg ? 2147483648LL : 2147483647LL;
  for (; i < s.size(); ++i) {
    if (s[i] < '0' || s[i] > '9')
      return std::nullopt;
    v = v * 10 + (s[i] - '0');
    if (v > limit)
      return std::nullopt;
  }
  return std::optional<int>(static_cast<int>(neg ? -v : v));
}

// 坐标合理性上限:±100000 物理像素。远超任何真实多屏布局(4K 4 屏拼接也只到
// 几万像素),超界即视为文件损坏 ⇒ 不认(顺带杜绝 x + w 的 int 溢出)。
constexpr int kMaxCoordAbs = 100000;

// 解析位置文件内容。
//   - 必须**同时**有 pos_x 与 pos_y 两项才认(缺一即损坏);
//   - 重复键以**第一次**出现为准(损坏的尾巴不能覆盖掉好的头部);
//   - 其它行(注释/空白/未知键)忽略。
inline std::optional<Point> ParsePosition(const std::string& raw) {
  std::optional<int> x;
  std::optional<int> y;
  size_t pos = 0;
  while (pos <= raw.size()) {
    const size_t eol = raw.find('\n', pos);
    std::string line =
        raw.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();  // 容忍 CRLF 写盘
    if (!line.empty()) {
      if (!x.has_value() && line.rfind("pos_x=", 0) == 0) {
        auto v = ParseIntStrict(line.substr(6));
        if (v.has_value() && *v <= kMaxCoordAbs && *v >= -kMaxCoordAbs)
          x = v;
      } else if (!y.has_value() && line.rfind("pos_y=", 0) == 0) {
        auto v = ParseIntStrict(line.substr(6));
        if (v.has_value() && *v <= kMaxCoordAbs && *v >= -kMaxCoordAbs)
          y = v;
      }
    }
    if (eol == std::string::npos)
      break;
    pos = eol + 1;
  }
  if (x.has_value() && y.has_value())
    return Point{*x, *y};
  return std::nullopt;
}

// ===== 读写 =====

// 读位置文件。文件缺失 / 打不开 / 内容损坏 ⇒ nullopt(调用方保持现状)。
inline std::optional<Point> ReadPositionFile(const std::wstring& path) {
  if (path.empty())
    return std::nullopt;
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return std::nullopt;
  const std::string raw((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
  return ParsePosition(raw);
}

// 写位置文件并**回读自证**(本仓铁律:0 字节坏文件比没文件更危险)。
// 写失败 / 回读不一致 ⇒ 删掉坏文件并返回 false。
// 与 always_show::WriteStateFile 同款(截断写 + 回读断言),不引入原子 rename。
inline bool WritePositionFile(const std::wstring& path, int x, int y) {
  if (path.empty())
    return false;
  const std::wstring dir = path.substr(0, path.find_last_of(L"\\/"));
  if (!dir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
  }
  {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
      return false;
    f << FormatPosition(x, y);
    f.flush();
    if (!f)
      return false;
  }
  auto back = ReadPositionFile(path);
  if (!back.has_value() || back->x != x || back->y != y) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return false;
  }
  return true;
}

// ===== 钳制 =====

// 把 (x,y) 钳制回「可见区」。
// work_areas = 所有显示器的工作区(屏幕坐标,**允许负值**)。
// 规则(ADR 0035):
//   1) 面板矩形完整落在**某个**工作区内 ⇒ 原样返回。
//      —— 这一条就是「多屏负坐标」用例:左侧副屏的工作区 left/top 是负数,
//         面板放在那里时位置必须**原样保留**,不能被当成「越界」拉走。
//   2) 否则 ⇒ 选「面板中心点最近、且尺寸容得下面板」的工作区,在其中钳制:
//        x ∈ [wa.left, wa.right - w],y ∈ [wa.top, wa.bottom - h]
//      —— 这一条就是「分辨率/DPI 变化后钳回可视区」用例。
//   3) 一个工作区都容不下面板 ⇒ (0,0)。
inline Point ClampToVisibleArea(int x,
                                int y,
                                int w,
                                int h,
                                const std::vector<Rect>& work_areas) {
  if (work_areas.empty())
    return Point{0, 0};
  if (w <= 0 || h <= 0)
    return Point{x, y};  // 尺寸非法 ⇒ 不动(防御,不该发生)
  // 规则 1:面板矩形完整落在**某个**工作区内 ⇒ 原样保留。
  //   这一条就是「多屏负坐标」用例 —— 左侧副屏的工作区 left/top 是负数,
  //   面板放在那里时位置必须原样保留,不能被当成「越界」拉走。
  const Rect panel{x, y, x + w, y + h};
  for (const Rect& wa : work_areas) {
    if (RectContains(wa, panel))
      return Point{x, y};
  }
  // 规则 2:否则 ⇒ 选「面板中心点最近、且尺寸容得下面板」的工作区,在其中钳制。
  //   这一条就是「分辨率/DPI 变化后钳回可视区」用例。
  const long long cx = static_cast<long long>(x) + (w / 2);
  const long long cy = static_cast<long long>(y) + (h / 2);
  const Rect* best = nullptr;
  long long best_dist = 0;
  for (const Rect& wa : work_areas) {
    if (RectWidth(wa) < w || RectHeight(wa) < h)
      continue;  // 容不下面板的工作区不参与候选
    const long long mx = static_cast<long long>(wa.left) + (RectWidth(wa) / 2);
    const long long my = static_cast<long long>(wa.top) + (RectHeight(wa) / 2);
    const long long dx = cx - mx;
    const long long dy = cy - my;
    const long long dist = dx * dx + dy * dy;
    if (!best || dist < best_dist) {
      best = &wa;
      best_dist = dist;
    }
  }
  // 规则 3:一个工作区都容不下面板 ⇒ (0,0)。
  if (!best)
    return Point{0, 0};
  int nx = x;
  int ny = y;
  const int lo_x = best->left;
  const int hi_x = best->right - w;  // 已保证 hi_x >= lo_x(该工作区容得下面板)
  if (nx < lo_x)
    nx = lo_x;
  if (nx > hi_x)
    nx = hi_x;
  const int lo_y = best->top;
  const int hi_y = best->bottom - h;
  if (ny < lo_y)
    ny = lo_y;
  if (ny > hi_y)
    ny = hi_y;
  return Point{nx, ny};
}

// ===== 显示器工作区枚举 =====

// 枚举所有显示器的工作区。左侧/上方副屏的工作区坐标为负 —— 钳制必须能处理。
inline BOOL CALLBACK EnumWorkAreaProc(HMONITOR mon,
                                      HDC,
                                      LPRECT,
                                      LPARAM lp) {
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(mon, &mi))
    return TRUE;
  std::vector<Rect>* out = reinterpret_cast<std::vector<Rect>*>(lp);
  out->push_back(
      Rect{mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom});
  return TRUE;
}

inline std::vector<Rect> EnumerateWorkAreas() {
  std::vector<Rect> out;
  ::EnumDisplayMonitors(nullptr, nullptr, EnumWorkAreaProc,
                        reinterpret_cast<LPARAM>(&out));
  return out;
}

// ===== 运行期缓存(启动期读一次,显示路径零 I/O) =====

// 已持久化的面板位置(屏幕坐标)。函数内 static ⇒ 跨 TU 只有一份。
inline Point& SavedPositionRef() {
  static Point p{0, 0};
  return p;
}

inline bool& HasSavedPositionRef() {
  static bool has = false;
  return has;
}

}  // namespace panel_position
}  // namespace fluxing
