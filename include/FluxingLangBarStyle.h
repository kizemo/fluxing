#pragma once
// FluxingLangBarStyle.h - ADR 0053 (2026-11-10)
//
// 单一真相源:语言栏项(ITfLangBarItemButton)的样式位 + 隐藏策略。
// 生产代码(WeaselTSF/LanguageBar.cpp)与单测
// (test/TestFluxingComponents/TestFluxingLangBar.cpp)同调本文件。
//
// 背景:通知区域(托盘)里那个方框「中」/「英」图标,就是本语言栏项:
//   GetIcon()          -> IDI_ZH / IDI_EN(方框「中」/「英」)
//   GetTooltipString() -> 「左键切换模式，右键打开菜单」
// ADR 0044 试图用「摘掉 dwStyle 里的 TF_LBI_STYLE_SHOWNINTRAY」去掉它,
// 实测无效:微软文档对该标志写明「目前不支援此标志」⇒ 该标志是 no-op,
// ADR 0044 的前提被本 ADR 修订。
//
// ADR 0053 原方案(MS 文档):
//   TF_LBI_STYLE_HIDDENSTATUSCONTROL - 允许用 TF_LBI_STATUS_HIDDEN 动态隐藏该项
//   TF_LBI_STATUS_HIDDEN              - 该项被隐藏
// => dwStyle 置 HIDDENSTATUSCONTROL,再恒置 STATUS_HIDDEN。
// **本方案亦已被真机证伪**(2026-11-10:装机副本 SHA256 与构建产物一致、
// 注册表指向同一文件、explorer 重启后仍跑新镜像、火流猩为活动 IME ⇒
// 方框「中」图标依然在)。故 dwStyle / STATUS_HIDDEN 仅作历史记录保留。
//
// 现行机制(v14,真机可证):根本不向语言栏注册该项 ⇒ 系统没有任何托管项
// 可渲染,托盘不可能出现该图标。开关 = 本文件末尾的 kLangBarRegisterItem。
//
// 禁用于此的标志:TF_LBI_STYLE_INVISIBLE —— 它会把语言栏里的入口也一并藏掉。
// (v14 起入口本就整体不注册,该禁令保留以防回退时误用。)

#include <msctf.h>

namespace fluxing {

// 语言栏项样式位。
//   TF_LBI_STYLE_BTN_BUTTON / TF_LBI_STYLE_BTN_MENU - 保留原有按钮行为
//   TF_LBI_STYLE_HIDDENSTATUSCONTROL                - 允许动态隐藏
// 刻意**不**包含 TF_LBI_STYLE_SHOWNINTRAY(微软标注「目前不支援」,no-op)。
const DWORD kLangBarItemStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_BTN_MENU |
                                TF_LBI_STYLE_HIDDENSTATUSCONTROL;

// 隐藏状态位(来自 msctf.h -> ctfutb.h)。
const DWORD kLangBarStatusHidden = TF_LBI_STATUS_HIDDEN;

// 永久隐藏策略:Show() 忽略调用方传入的 fShow,恒置隐藏位。
// 理由:用户可在「语言栏选项」里重新启用本项,系统随后 Show(TRUE)
// 会把托盘图标复活 => 必须恒隐藏。
// ⚠ v14 起该项根本不再注册,本策略仅在 kLangBarRegisterItem==true(回退)时有意义。
const bool kLangBarItemForceHidden = true;

// ===== v14 总开关 =====
// 是否向语言栏注册该项。false = 完全不 new / 不 AddItem ⇒ 通知区域不再出现
// 方框「中」/「英」。这是唯一经真机验证有效的手段(见文件头两条证伪记录)。
// 设为 true 可回退到 ADR 0053 时期的 dwStyle + STATUS_HIDDEN 行为。
const bool kLangBarRegisterItem = false;

}  // namespace fluxing