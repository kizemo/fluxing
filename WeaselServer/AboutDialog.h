#pragma once
//
// AboutDialog.h — 「关于 / 账号说明」页(spec 099 P0-3)
//
// 落点:快捷面板第 4 槽(Account)。用户点击该槽 ⇒ 打开本页。
// 槽位**保留**(kButtonCount 仍是 4),不是「摘除」——用户 2026-11-04 定稿。
//
// 关键约束:
//  - P2:显示期与点击期**零 I/O** —— 版本/产品名/数据目录只在服务端启动期
//    (Initialize / InitializeFromPath)读一次并缓存;窗口创建、绘制、按钮
//    点击都不再碰磁盘或注册表。
//  - P5:全部用户可见文案为简体中文。
//  - 版本/产品名**单一真相源** = 宿主 exe 的 PE 版本资源字符串表
//    (ProductName / ProductVersion);回退链 ProductVersion → FileVersion →
//    VS_FIXEDFILEINFO。L175:rc 的 FileVersion 曾因缺 STRZ() 被写成空串,
//    只读一个字段会读到空值 ⇒ 必须有回退链。
//  - 不用 MessageBox:它会阻塞服务端 UI 线程。
//
#include <windows.h>
#include <string>

namespace AboutDialog {

// 启动期调用一次:读宿主 exe 的 PE 版本资源 + 用户数据目录并缓存。
void Initialize();

// 测试注入:指定读哪个文件的版本信息(数据目录照常缓存)。
void InitializeFromPath(const std::wstring& exe_path);

// 读 PE 版本资源。成功返回 true;product_name / product_version
// 允许为空(取决于该文件有没有对应字符串)。
bool ReadVersionInfo(const std::wstring& file_path,
                     std::wstring* product_name,
                     std::wstring* product_version);

// 缓存后的产品名 / 版本号 / 用户数据目录(启动期已读好,此处纯内存)。
std::wstring ProductName();
std::wstring VersionText();
std::wstring DataDirText();

// 打开关于页(modeless;已打开则置前)。返回窗口句柄(失败为 NULL)。
HWND Show();
HWND ActiveHwnd();

}  // namespace AboutDialog
