//
// main.cpp — FluxingPhrasesDialog.exe 入口（独立 Win32 GUI 进程）
//
// Phase K1 (v0.19.0.50): 独立 exe，硬编码测试数据验证 UI 正常显示
// Phase K2 (v0.19.0.51): 加 named pipe IPC 通信
//   命令行: FluxingPhrasesDialog.exe --pipe=\\.\pipe\FluxingPhrasesDialog\{pid}
//   无 --pipe 参数时用硬编码测试数据（独立测试用）
//
#include "stdafx.h"
#include "PhrasesDialog.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
  // InitCommonControlsEx 给 ListView 用
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&icc);

  // 解析 --pipe= 参数
  std::wstring cmdLine(lpCmdLine);
  std::wstring pipeName;
  const std::wstring prefix = L"--pipe=";
  size_t pos = cmdLine.find(prefix);
  if (pos != std::wstring::npos) {
    pipeName = cmdLine.substr(pos + prefix.size());
    // 去掉可能的尾随空格 / 引号
    while (!pipeName.empty() && pipeName.back() == L' ') pipeName.pop_back();
  }

  if (!pipeName.empty()) {
    PhrasesDialog::SetPipeName(pipeName);
  }

  // 显示窗口
  PhrasesDialog::Show();

  // 消息循环
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  return static_cast<int>(msg.wParam);
}
