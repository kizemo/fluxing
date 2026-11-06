#include "stdafx.h"
#include "Deserializer.h"
#include "Configurator.h"

using namespace weasel;

Deserializer::Ptr Configurator::Create(ResponseParser* pTarget) {
  return Deserializer::Ptr(new Configurator(pTarget));
}

Configurator::Configurator(ResponseParser* pTarget) : Deserializer(pTarget) {}

Configurator::~Configurator() {}

void Configurator::Store(Deserializer::KeyType const& key,
                         std::wstring const& value) {
  // L187 (2026-11-06 宿主崩溃): 原判空只看了 p_context, 漏了 p_config。
  //
  // WeaselTSF 构造 ResponseParser 共有四个调用点, 其中 OnSetThreadFocus、
  // _Reconnect、_HandleCompartment 连 p_context 都不传, 被上面原有的判空早退;
  // 只有 _DeleteCandidateOnCurrentPage 传了 &ctx 却不传 config (它按设计只关
  // 心 status / context / style, config 形参留默认值 0)。
  //
  // 而服务端 RimeWithWeasel 组装响应时无条件 append config.inline_preedit,
  // 并把 config 列进 action= 名单, 于是删候选词这条路径上本函数必被调用;
  // 「p_context 非空 + p_config 为空」落到下面的 inline_preedit 赋值处, 在
  // 宿主进程 (飞书 / MiniMax Code 等 Electron 应用) 的窗口回调线程里解引用
  // 空指针, 异常码 0xc000041d, 表现为「右键候选窗 => 宿主软件一起被杀死」。
  //
  // 同批 deserializer (Styler / ContextUpdater / Committer / StatusUpdater)
  // 都判了各自的 target 指针, 只有这里漏了 p_config。
  if (!m_pTarget->p_context || !m_pTarget->p_config || key.size() < 2)
    return;
  bool bool_value = (!value.empty() && value != L"0");
  if (key[1] == L"inline_preedit") {
    m_pTarget->p_config->inline_preedit = bool_value;
  }
}
