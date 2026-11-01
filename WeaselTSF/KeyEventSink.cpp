#include "stdafx.h"
#include "WeaselIPC.h"
#include "WeaselTSF.h"
#include <KeyEvent.h>
#include "CandidateList.h"
#include <FluxingCandidateDeleteShortcut.h>

static weasel::KeyEvent prevKeyEvent;
static BOOL prevfEaten = FALSE;
static int keyCountToSimulate = 0;

void WeaselTSF::_ProcessKeyEvent(WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
  // when _IsKeyboardDisabled don't eat the key,
  // when keyboard closable and keyboard closed, don't eat the key
  if ((_isToOpenClose && !_IsKeyboardOpen()) || _IsKeyboardDisabled()) {
    *pfEaten = FALSE;
    return;
  }

  // if server connection is Not OK, don't eat it.
  if (!_EnsureServerConnected()) {
    *pfEaten = FALSE;
    return;
  }
  weasel::KeyEvent ke;
  GetKeyboardState(_lpbKeyState);
  // spec 094: 「Ctrl+Shift+数字」删词必须看**键盘状态**而不是 ConvertKeyEvent
  // 产出的 keycode —— 那是「产生的字符」(Ctrl+Shift+1 -> '!'),从字符反推不出
  // 数字键。GetKeyboardState 是 Win32 的硬保证。
  const bool ctrl_down = fluxing::IsKeyStateDown(_lpbKeyState, VK_CONTROL);
  const bool shift_down = fluxing::IsKeyStateDown(_lpbKeyState, VK_SHIFT);
  if (!ConvertKeyEvent(static_cast<UINT>(wParam), lParam, _lpbKeyState, ke)) {
    /* Unknown key event */
    *pfEaten = FALSE;
  } else {
    // cheet key code when vertical auto reverse happened, swap up and down
    if (_cand->GetIsReposition()) {
      if (ke.keycode == ibus::Up)
        ke.keycode = ibus::Down;
      else if (ke.keycode == ibus::Down)
        ke.keycode = ibus::Up;
    }
    // spec 077: Shift_L/R detection — eat the event entirely so it never
    // reaches m_client.ProcessKeyEvent (which would feed it to librime
    // and let librime's key_event.h:64 operator== mis-match the release
    // half of any Shift+letter combo). We forward both halves to the
    // Server-side state machine which knows when to actually select.
    // R10: we must return early — letter events that follow in the same
    // packet are unaffected because we don't touch _lpbKeyState.
    if (ke.keycode == ibus::Shift_L || ke.keycode == ibus::Shift_R) {
      BOOL isLeft = (ke.keycode == ibus::Shift_L);
      BOOL isDown = !(ke.mask & ibus::RELEASE_MASK);
      if (isDown) {
        // spec 094: Ctrl+Shift+数字 是删词快捷键。这个组合里的 Shift 绝不能
        // 原样转发给服务端 —— 服务端 Shift 状态机会把它当成「选词 Shift」,
        // 菜单打开时直接 commit_composition 把候选上屏(spec 077 / ADR 0023)。
        // 所以 Ctrl 按住时吞掉 Shift down,并记住配对的 up 也要吞。
        if (ctrl_down) {
          _shiftSuppressed = TRUE;
          *pfEaten = TRUE;
          return;
        }
        m_client.ShiftDown(isLeft ? TRUE : FALSE);
        _shiftDown = TRUE;
        _shiftDownIsLeft = isLeft ? TRUE : FALSE;
      } else {
        if (_shiftSuppressed) {
          _shiftSuppressed = FALSE;
          *pfEaten = TRUE;
          return;
        }
        m_client.ShiftUp(isLeft ? TRUE : FALSE);
        _shiftDown = FALSE;
        _shiftDownIsLeft = FALSE;
      }
      *pfEaten = TRUE;
      return;  // C6: never feed Shift to ProcessKeyEvent
    }
    // spec 094: Ctrl+Shift+<数字> 从用户词典里删除当前可见页的第 N 个候选。
    //
    // 只在「候选窗确实可见」且「N 落在当前可见页范围内」时才拦截:
    //   - 没有候选时按数字键本就该是普通输入,不能吞。
    //
    // spec 097 / L163: 这条拦截是「条件性」的 —— 条件不满足时按键按设计放行。
    //   放行之后, 若还有一条无条件兜底绑定留在同一个 chord 家族上, 每一次
    //   删词失败都会顺手动一次那个开关 (标点/简繁); 而开关状态被记忆, 于是
    //   「我按了删词键」和「我的标点变了」被绑成了一件事, 一直错下去。
    //   所以 ascii_punct / traditionalization 已移出 Ctrl+Shift+<数字> 家族
    //   (现在是 Control+period / Control+slash), 数字族专属于删词。
    if (!keyCountToSimulate && fluxing::IsKeyDownLParam(lParam)) {
      const int number = fluxing::MatchCandidateDeleteShortcut(
          static_cast<int>(wParam), ctrl_down, shift_down, true);
      if (number > 0 && fluxing::ShouldInterceptCandidateDelete(
                            number, _cand->GetVisibleCandidateCount())) {
        _DeleteCandidateOnCurrentPage(static_cast<size_t>(number - 1));
        *pfEaten = TRUE;
        return;
      }
    }
    if (!keyCountToSimulate)
      *pfEaten = (BOOL)m_client.ProcessKeyEvent(ke);

    if (ke.keycode == ibus::Caps_Lock) {
      if (prevKeyEvent.keycode == ibus::Caps_Lock && prevfEaten == TRUE &&
          (ke.mask & ibus::RELEASE_MASK) && (!keyCountToSimulate)) {
        if ((GetKeyState(VK_CAPITAL) & 0x01)) {
          if (_committed || (!*pfEaten && _status.composing)) {
            keyCountToSimulate = 2;
            INPUT inputs[2];
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki = {VK_CAPITAL, 0, 0, 0, 0};
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki = {VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0, 0};
            ::SendInput(sizeof(inputs) / sizeof(INPUT), inputs, sizeof(INPUT));
          }
        }
        *pfEaten = TRUE;
      }
      if (keyCountToSimulate)
        keyCountToSimulate--;
    }

    prevfEaten = *pfEaten;
    prevKeyEvent = ke;
  }
}

// spec 096 / ADR 0027: 普通窗口焦点事件带上「应用焦点」维度的位标志。
//
// 过去这里发 wParam=0 —— 与「用户切到火流猩」(PROFILE_ACTIVATED) 之外的
// 任何标志都无法区分,FocusIn 侧只能靠「有没有 bit0」反推事件种类。
// FocusOut 侧从 spec 091 起就有 PROFILE_DEACTIVATED 位做同一件事,
// 现在 FocusIn 侧补齐对称的一格(ADR 0027 协议表 bit1 = app_focus)。
//
// 取值变化不是线格式变化:命令号 / 字段布局不变,旧服务端只认 bit0,
// 收到 bit1 仍是旧语义 => 安全降级。
STDAPI WeaselTSF::OnSetFocus(BOOL fForeground) {
  if (fForeground)
    m_client.FocusIn(WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED);
  else {
    m_client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST);
    _AbortComposition();
  }

  return S_OK;
}

/* Some apps sends strange OnTestKeyDown/OnKeyDown combinations:
 *  Some sends OnKeyDown() only. (QQ2012)
 *  Some sends multiple OnTestKeyDown() for a single key event. (MS WORD 2010
 * x64)
 *
 * We assume every key event will eventually cause a OnKeyDown() call.
 * We use _fTestKeyDownPending to omit multiple OnTestKeyDown() calls,
 *  and for OnKeyDown() to check if the key has already been sent to the server.
 */

STDAPI WeaselTSF::OnTestKeyDown(ITfContext* pContext,
                                WPARAM wParam,
                                LPARAM lParam,
                                BOOL* pfEaten) {
  _fTestKeyUpPending = FALSE;
  if (_fTestKeyDownPending) {
    *pfEaten = TRUE;
    return S_OK;
  }
  _ProcessKeyEvent(wParam, lParam, pfEaten);
  _UpdateComposition(pContext);
  if (*pfEaten)
    _fTestKeyDownPending = TRUE;
  return S_OK;
}

STDAPI WeaselTSF::OnKeyDown(ITfContext* pContext,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL* pfEaten) {
  _fTestKeyUpPending = FALSE;
  if (_fTestKeyDownPending) {
    _fTestKeyDownPending = FALSE;
    *pfEaten = TRUE;
  } else {
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    _UpdateComposition(pContext);
  }
  return S_OK;
}

STDAPI WeaselTSF::OnTestKeyUp(ITfContext* pContext,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL* pfEaten) {
  _fTestKeyDownPending = FALSE;
  if (_fTestKeyUpPending) {
    *pfEaten = TRUE;
    return S_OK;
  }
  _ProcessKeyEvent(wParam, lParam, pfEaten);
  _UpdateComposition(pContext);
  if (*pfEaten)
    _fTestKeyUpPending = TRUE;
  return S_OK;
}

STDAPI WeaselTSF::OnKeyUp(ITfContext* pContext,
                          WPARAM wParam,
                          LPARAM lParam,
                          BOOL* pfEaten) {
  _fTestKeyDownPending = FALSE;
  if (_fTestKeyUpPending) {
    _fTestKeyUpPending = FALSE;
    *pfEaten = TRUE;
  } else {
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    if (!_async_edit)
      _UpdateComposition(pContext);
  }
  return S_OK;
}

STDAPI WeaselTSF::OnPreservedKey(ITfContext* pContext,
                                 REFGUID rguid,
                                 BOOL* pfEaten) {
  *pfEaten = FALSE;
  return S_OK;
}

BOOL WeaselTSF::_InitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
  HRESULT hr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return FALSE;

  hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink*)this,
                                         TRUE);

  return (hr == S_OK);
}

void WeaselTSF::_UninitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return;

  pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
}

BOOL WeaselTSF::_InitPreservedKey() {
  return TRUE;
#if 0
	com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
	if (_pThreadMgr->QueryInterface(pKeystrokeMgr.GetAddressOf()) != S_OK)
	{
		return FALSE;
	}
	TF_PRESERVEDKEY preservedKeyImeMode;

	/* Define SHIFT ONLY for now */
	preservedKeyImeMode.uVKey = VK_SHIFT;
	preservedKeyImeMode.uModifiers = TF_MOD_ON_KEYUP;

	auto hr = pKeystrokeMgr->PreserveKey(
		_tfClientId,
		GUID_IME_MODE_PRESERVED_KEY,
		&preservedKeyImeMode, L"", 0);
	
	return SUCCEEDED(hr);
#endif
}

void WeaselTSF::_UninitPreservedKey() {}
