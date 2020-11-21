#include "stdafx.h"

#include "InPlaceEdit.h"
#include "wtl-pp.h"
#include "win32_op.h"

#include "AutoComplete.h"
#include "CWindowCreateAndDelete.h"
#include "win32_utility.h"
#include "listview_helper.h" // ListView_GetColumnCount
#include "clipboard.h"

#include <forward_list>

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x20E
#endif

using namespace InPlaceEdit;


namespace {

	enum {
		MSG_COMPLETION = WM_USER,
		MSG_DISABLE_EDITING
	};


	// Rationale: more than one HWND on the list is extremely uncommon, hence forward_list
	static std::forward_list<HWND> g_editboxes;
	static HHOOK g_hook = NULL /*, g_keyHook = NULL*/;

	static void GAbortEditing(HWND edit, t_uint32 code) {
		CWindow parent = ::GetParent(edit);
		parent.SendMessage(MSG_DISABLE_EDITING);
		parent.PostMessage(MSG_COMPLETION, code, 0);
	}

	static void GAbortEditing(t_uint32 code) {
		for (auto walk = g_editboxes.begin(); walk != g_editboxes.end(); ++walk) {
			GAbortEditing(*walk, code);
		}
	}

	static bool IsSamePopup(CWindow wnd1, CWindow wnd2) {
		return pfc::findOwningPopup(wnd1) == pfc::findOwningPopup(wnd2);
	}

	static void MouseEventTest(HWND target, CPoint pt, bool isWheel) {
		for (auto walk = g_editboxes.begin(); walk != g_editboxes.end(); ++walk) {
			CWindow edit(*walk);
			bool cancel = false;
			if (target != edit && IsSamePopup(target, edit)) {
				cancel = true;
			} else if (isWheel) {
				CWindow target2 = WindowFromPoint(pt);
				if (target2 != edit && IsSamePopup(target2, edit)) {
					cancel = true;
				}
			}

			if (cancel) GAbortEditing(edit, KEditLostFocus);
		}
	}

	static LRESULT CALLBACK GMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
		if (nCode == HC_ACTION) {
			const MOUSEHOOKSTRUCT * mhs = reinterpret_cast<const MOUSEHOOKSTRUCT *>(lParam);
			switch (wParam) {
			case WM_NCLBUTTONDOWN:
			case WM_NCRBUTTONDOWN:
			case WM_NCMBUTTONDOWN:
			case WM_NCXBUTTONDOWN:
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_XBUTTONDOWN:
				MouseEventTest(mhs->hwnd, mhs->pt, false);
				break;
			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL:
				MouseEventTest(mhs->hwnd, mhs->pt, true);
				break;
			}
		}
		return CallNextHookEx(g_hook, nCode, wParam, lParam);
	}

	static void on_editbox_creation(HWND p_editbox) {
		// PFC_ASSERT(core_api::is_main_thread());
		g_editboxes.push_front(p_editbox);
		if (g_hook == NULL) {
			g_hook = SetWindowsHookEx(WH_MOUSE, GMouseProc, NULL, GetCurrentThreadId());
		}
		/*if (g_keyHook == NULL) {
			g_keyHook = SetWindowsHookEx(WH_KEYBOARD, GKeyboardProc, NULL, GetCurrentThreadId());
		}*/
	}
	static void UnhookHelper(HHOOK & hook) {
		HHOOK v = pfc::replace_null_t(hook);
		if (v != NULL) UnhookWindowsHookEx(v);
	}
	static void on_editbox_destruction(HWND p_editbox) {
		// PFC_ASSERT(core_api::is_main_thread());
		g_editboxes.remove(p_editbox);
		if (g_editboxes.empty()) {
			UnhookHelper(g_hook); /*UnhookHelper(g_keyHook);*/
		}
	}

	class CInPlaceEditBox : public CContainedWindowSimpleT<CEdit> {
	public:
		CInPlaceEditBox(uint32_t flags) : m_flags(flags) {}
		BEGIN_MSG_MAP_EX(CInPlaceEditBox)
			//MSG_WM_CREATE(OnCreate)
			MSG_WM_DESTROY(OnDestroy)
			MSG_WM_GETDLGCODE(OnGetDlgCode)
			MSG_WM_KILLFOCUS(OnKillFocus)
			MSG_WM_CHAR(OnChar)
			MSG_WM_KEYDOWN(OnKeyDown)
			MSG_WM_PASTE(OnPaste)
		END_MSG_MAP()

		void OnCreation() {
			on_editbox_creation(m_hWnd);
		}
	private:
		void OnDestroy() {
			m_selfDestruct = true;
			on_editbox_destruction(m_hWnd);
			SetMsgHandled(FALSE);
		}
		int OnCreate(LPCREATESTRUCT lpCreateStruct) {
			OnCreation();
			SetMsgHandled(FALSE);
			return 0;
		}
		UINT OnGetDlgCode(LPMSG lpMsg) {
			if (lpMsg == NULL) {
				SetMsgHandled(FALSE); return 0;
			} else {
				switch (lpMsg->message) {
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
					switch (lpMsg->wParam) {
					case VK_TAB:
					case VK_ESCAPE:
					case VK_RETURN:
						return DLGC_WANTMESSAGE;
					default:
						SetMsgHandled(FALSE); return 0;
					}
				default:
					SetMsgHandled(FALSE); return 0;

				}
			}
		}
		void OnKillFocus(CWindow wndFocus) {
			if ( wndFocus != NULL ) ForwardCompletion(KEditLostFocus);
			SetMsgHandled(FALSE);
		}

		bool testPaste(const char* str) {
			if (m_flags & InPlaceEdit::KFlagNumberSigned) {
				if (pfc::string_is_numeric(str)) return true;
				if (str[0] == '-' && pfc::string_is_numeric(str + 1) && GetWindowTextLength() == 0) return true;
				return false;
			}
			if (m_flags & InPlaceEdit::KFlagNumber) {
				return pfc::string_is_numeric(str);
			}
			return true;
		}

		void OnPaste() {
			if (m_flags & (InPlaceEdit::KFlagNumber | InPlaceEdit::KFlagNumberSigned)) {
				pfc::string8 temp;
				ClipboardHelper::OpenScope scope; scope.Open(m_hWnd);
				if (ClipboardHelper::GetString(temp)) {
					if (!testPaste(temp)) return;
				}
			}
			// Let edit box handle it
			SetMsgHandled(FALSE);
		}
		bool testChar(UINT nChar) {
			// Allow various non text characters
			if (nChar < ' ') return true;

			if (m_flags & InPlaceEdit::KFlagNumberSigned) {
				if (pfc::char_is_numeric(nChar)) return true;
				if (nChar == '-') {
					return GetWindowTextLength() == 0;
				}
				return false;
			}
			if (m_flags & InPlaceEdit::KFlagNumber) {
				return pfc::char_is_numeric(nChar);
			}
			return true;
		}
		void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) {
			if (m_suppressChar != 0) {
				UINT code = nFlags & 0xFF;
				if (code == m_suppressChar) return;
			}

			if (!testChar(nChar)) {
				MessageBeep(0);
				return;
			}

			SetMsgHandled(FALSE);
		}
		void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
			m_suppressChar = nFlags & 0xFF;
			switch (nChar) {
			case VK_BACK:
				if (GetHotkeyModifierFlags() == MOD_CONTROL) {
					CEditPPHooks::DeleteLastWord(*this);
					return;
				}
				break;
			case 'A':
				if (GetHotkeyModifierFlags() == MOD_CONTROL) {
					this->SetSelAll(); return;
				}
				break;
			case VK_RETURN:
				if (!IsKeyPressed(VK_LCONTROL) && !IsKeyPressed(VK_RCONTROL)) {
					ForwardCompletion(KEditEnter);
					return;
				}
				break;
			case VK_TAB:
				ForwardCompletion(IsKeyPressed(VK_SHIFT) ? KEditShiftTab : KEditTab);
				return;
			case VK_ESCAPE:
				ForwardCompletion(KEditAborted);
				return;
			}
			m_suppressChar = 0;
			SetMsgHandled(FALSE);
		}

		void ForwardCompletion(t_uint32 code) {
			if (IsWindowEnabled()) {
				CWindow owner = GetParent();
				owner.SendMessage(MSG_DISABLE_EDITING);
				owner.PostMessage(MSG_COMPLETION, code, 0);
				EnableWindow(FALSE);
			}
		}

		const uint32_t m_flags;
		bool m_selfDestruct = false;
		UINT m_suppressChar = 0;
	};

	class InPlaceEditContainer : public CWindowImpl<InPlaceEditContainer> {
	public:
		DECLARE_WND_CLASS_EX(_T("{54340C80-248C-4b8e-8CD4-D624A8E9377B}"), 0, -1);


		HWND Create(CWindow parent) {

			RECT rect_cropped;
			{
				RECT client;
				WIN32_OP_D(parent.GetClientRect(&client));
				IntersectRect(&rect_cropped, &client, &m_initRect);
			}
			const DWORD containerStyle = WS_BORDER | WS_CHILD;
			AdjustWindowRect(&rect_cropped, containerStyle, FALSE);



			WIN32_OP(__super::Create(parent, rect_cropped, NULL, containerStyle) != NULL);

			try {
				CRect rcClient;
				WIN32_OP_D(GetClientRect(rcClient));


				DWORD style = WS_CHILD | WS_VISIBLE;//parent is invisible now
				if (m_flags & KFlagMultiLine) style |= WS_VSCROLL | ES_MULTILINE;
				else style |= ES_AUTOHSCROLL;
				if (m_flags & KFlagReadOnly) style |= ES_READONLY;
				if (m_flags & KFlagAlignCenter) style |= ES_CENTER;
				else if (m_flags & KFlagAlignRight) style |= ES_RIGHT;
				else style |= ES_LEFT;

				// ES_NUMBER is buggy in many ways (repaint glitches after balloon popup) and does not allow signed numbers.
				// We implement number handling by filtering WM_CHAR instead.
				// if (m_flags & KFlagNumber) style |= ES_NUMBER;


				CEdit edit;

				WIN32_OP(edit.Create(*this, rcClient, NULL, style, 0, ID_MYEDIT) != NULL);
				edit.SetFont(parent.GetFont());

				if (m_ACData.is_valid()) InitializeSimpleAC(edit, m_ACData.get_ptr(), m_ACOpts);
				m_edit.SubclassWindow(edit);
				m_edit.OnCreation();

				pfc::setWindowText(m_edit, *m_content);
				m_edit.SetSelAll();
			} catch (...) {
				PostMessage(MSG_COMPLETION, InPlaceEdit::KEditAborted, 0);
				return m_hWnd;
			}

			ShowWindow(SW_SHOW);
			m_edit.SetFocus();

			m_initialized = true;

			PFC_ASSERT(m_hWnd != NULL);

			return m_hWnd;
		}

		InPlaceEditContainer(const RECT & p_rect, t_uint32 p_flags, pfc::rcptr_t<pfc::string_base> p_content, reply_t p_notify, IUnknown * ACData, DWORD ACOpts)
			: m_content(p_content), m_notify(p_notify), m_completed(false), m_initialized(false), m_changed(false), m_disable_editing(false), m_initRect(p_rect), 
			m_flags(p_flags), m_selfDestruct(), m_ACData(ACData), m_ACOpts(ACOpts),
			m_edit(p_flags)
		{
		}

		enum { ID_MYEDIT = 666 };

		BEGIN_MSG_MAP_EX(InPlaceEditContainer)
			MESSAGE_HANDLER_EX(WM_CTLCOLOREDIT, MsgForwardToParent)
			MESSAGE_HANDLER_EX(WM_CTLCOLORSTATIC, MsgForwardToParent)
			MESSAGE_HANDLER_EX(WM_MOUSEWHEEL, MsgLostFocus)
			MESSAGE_HANDLER_EX(WM_MOUSEHWHEEL, MsgLostFocus)
			MESSAGE_HANDLER_SIMPLE(MSG_DISABLE_EDITING, OnMsgDisableEditing)
			MESSAGE_HANDLER_EX(MSG_COMPLETION, OnMsgCompletion)
			COMMAND_HANDLER_EX(ID_MYEDIT, EN_CHANGE, OnEditChange)
			MSG_WM_DESTROY(OnDestroy)
		END_MSG_MAP()

		HWND GetEditBox() const { return m_edit; }

	private:
		void OnDestroy() { m_selfDestruct = true; }

		LRESULT MsgForwardToParent(UINT msg, WPARAM wParam, LPARAM lParam) {
			return GetParent().SendMessage(msg, wParam, lParam);
		}
		LRESULT MsgLostFocus(UINT, WPARAM, LPARAM) {
			PostMessage(MSG_COMPLETION, InPlaceEdit::KEditLostFocus, 0);
			return 0;
		}
		void OnMsgDisableEditing() {
			ShowWindow(SW_HIDE);
			GetParent().UpdateWindow();
			m_disable_editing = true;
		}
		LRESULT OnMsgCompletion(UINT, WPARAM wParam, LPARAM lParam) {
			PFC_ASSERT(m_initialized);
			if ((wParam & KEditMaskReason) != KEditLostFocus) {
				GetParent().SetFocus();
			}
			if ( m_changed && m_edit != NULL ) {
				*m_content = pfc::getWindowText(m_edit);
			}
			OnCompletion((unsigned)wParam);
			if (!m_selfDestruct) {
				m_selfDestruct = true;
				DestroyWindow();
			}
			return 0;
		}
		void OnEditChange(UINT, int, CWindow source) {
			if (m_initialized && !m_disable_editing) {
				*m_content = pfc::getWindowText(source);
				m_changed = true;
			}
		}

	private:

		void OnCompletion(unsigned p_status) {
			if (!m_completed) {
				m_completed = true;
				p_status &= KEditMaskReason;
				unsigned code = p_status;
				if (m_changed && p_status != KEditAborted) code |= KEditFlagContentChanged;
				if (m_notify) m_notify(code);
			}
		}

		const pfc::rcptr_t<pfc::string_base> m_content;
		const reply_t m_notify;
		bool m_completed;
		bool m_initialized, m_changed;
		bool m_disable_editing;
		bool m_selfDestruct;
		const CRect m_initRect;
		const t_uint32 m_flags;
		CInPlaceEditBox m_edit;

		const pfc::com_ptr_t<IUnknown> m_ACData;
		const DWORD m_ACOpts;
	};

}

static void fail(reply_t p_notify) {
	p_notify(KEditAborted);
	// completion_notify::g_signal_completion_async(p_notify, KEditAborted);
}

HWND InPlaceEdit::Start(HWND p_parentwnd, const RECT & p_rect, bool p_multiline, pfc::rcptr_t<pfc::string_base> p_content, reply_t p_notify) {
	return StartEx(p_parentwnd, p_rect, p_multiline ? KFlagMultiLine : 0, p_content, p_notify);
}

void InPlaceEdit::Start_FromListView(HWND p_listview, unsigned p_item, unsigned p_subitem, unsigned p_linecount, pfc::rcptr_t<pfc::string_base> p_content, reply_t p_notify) {
	Start_FromListViewEx(p_listview, p_item, p_subitem, p_linecount, 0, p_content, p_notify);
}

bool InPlaceEdit::TableEditAdvance_ListView(HWND p_listview, unsigned p_column_base, unsigned & p_item, unsigned & p_column, unsigned p_item_count, unsigned p_column_count, unsigned p_whathappened) {
	if (p_column >= p_column_count) return false;


	pfc::array_t<t_size> orderRev;
	{
		pfc::array_t<unsigned> order;
		const unsigned orderExCount = /*p_column_base + p_column_count*/ ListView_GetColumnCount(p_listview);
		PFC_ASSERT(orderExCount >= p_column_base + p_column_count);
		pfc::array_t<int> orderEx; orderEx.set_size(orderExCount);
		if (!ListView_GetColumnOrderArray(p_listview, orderExCount, orderEx.get_ptr())) {
			PFC_ASSERT(!"Should not get here - probably mis-calculated column count");
			return false;
		}
		order.set_size(p_column_count);
		for (unsigned walk = 0; walk < p_column_count; ++walk) order[walk] = orderEx[p_column_base + walk];

		orderRev.set_size(p_column_count); order_helper::g_fill(orderRev);
		pfc::sort_get_permutation_t(order, pfc::compare_t<unsigned, unsigned>, p_column_count, orderRev.get_ptr());
	}

	unsigned columnVisible = (unsigned)orderRev[p_column];


	if (!TableEditAdvance(p_item, columnVisible, p_item_count, p_column_count, p_whathappened)) return false;

	p_column = (unsigned)order_helper::g_find_reverse(orderRev.get_ptr(), columnVisible);

	return true;
}

bool InPlaceEdit::TableEditAdvance(unsigned & p_item, unsigned & p_column, unsigned p_item_count, unsigned p_column_count, unsigned p_whathappened) {
	if (p_item >= p_item_count || p_column >= p_column_count) return false;
	int delta = 0;

	switch (p_whathappened & KEditMaskReason) {
	case KEditEnter:
		delta = (int)p_column_count;
		break;
	case KEditTab:
		delta = 1;
		break;
	case KEditShiftTab:
		delta = -1;
		break;
	default:
		return false;
	}
	while (delta > 0) {
		p_column++;
		if (p_column >= p_column_count) {
			p_column = 0;
			p_item++;
			if (p_item >= p_item_count) return false;
		}
		delta--;
	}
	while (delta < 0) {
		if (p_column == 0) {
			if (p_item == 0) return false;
			p_item--;
			p_column = p_column_count;
		}
		p_column--;
		delta++;
	}
	return true;
}

HWND InPlaceEdit::StartEx(HWND p_parentwnd, const RECT & p_rect, unsigned p_flags, pfc::rcptr_t<pfc::string_base> p_content, reply_t p_notify, IUnknown * ACData, DWORD ACOpts) {
	try {
		PFC_ASSERT((CWindow(p_parentwnd).GetWindowLong(GWL_STYLE) & WS_CLIPCHILDREN) != 0);
		return (new CWindowCreateAndDelete<InPlaceEditContainer>(p_parentwnd, p_rect, p_flags, p_content, p_notify, ACData, ACOpts))->GetEditBox();
	} catch (...) {
		fail(p_notify);
		return NULL;
	}
}

void InPlaceEdit::Start_FromListViewEx(HWND p_listview, unsigned p_item, unsigned p_subitem, unsigned p_linecount, unsigned p_flags, pfc::rcptr_t<pfc::string_base> p_content, reply_t p_notify) {
	try {
		ListView_EnsureVisible(p_listview, p_item, FALSE);
		RECT itemrect;
		WIN32_OP_D(ListView_GetSubItemRect(p_listview, p_item, p_subitem, LVIR_LABEL, &itemrect));

		const bool multiline = p_linecount > 1;
		if (multiline) {
			itemrect.bottom = itemrect.top + (itemrect.bottom - itemrect.top) * p_linecount;
		}

		StartEx(p_listview, itemrect, p_flags | (multiline ? KFlagMultiLine : 0), p_content, p_notify);
	} catch (...) {
		fail(p_notify);
	}
}
