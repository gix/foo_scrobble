#include "pfc-lite.h"

#ifdef _WIN32
#include "win-objects.h"
#include "array.h"
#include "pp-winapi.h"
#include "string_conv.h"
#include "string_base.h"
#include "debug.h"
#include "string-conv-lite.h"

#include "pfc-fb2k-hooks.h"

namespace pfc {

BOOL winFormatSystemErrorMessageImpl(pfc::string_base & p_out,DWORD p_code) {
	switch(p_code) {
	case ERROR_CHILD_NOT_COMPLETE:
		p_out = "Application cannot be run in Win32 mode.";
		return TRUE;
	case ERROR_INVALID_ORDINAL:
		p_out = "Invalid ordinal.";
		return TRUE;
	case ERROR_INVALID_STARTING_CODESEG:
		p_out = "Invalid code segment.";
		return TRUE;
	case ERROR_INVALID_STACKSEG:
		p_out = "Invalid stack segment.";
		return TRUE;
	case ERROR_INVALID_MODULETYPE:
		p_out = "Invalid module type.";
		return TRUE;
	case ERROR_INVALID_EXE_SIGNATURE:
		p_out = "Invalid executable signature.";
		return TRUE;
	case ERROR_BAD_EXE_FORMAT:
		p_out = "Not a valid Win32 application.";
		return TRUE;
	case ERROR_EXE_MACHINE_TYPE_MISMATCH:
		p_out = "Machine type mismatch.";
		return TRUE;
	case ERROR_EXE_CANNOT_MODIFY_SIGNED_BINARY:
	case ERROR_EXE_CANNOT_MODIFY_STRONG_SIGNED_BINARY:
		p_out = "Unable to modify a signed binary.";
		return TRUE;
	default:
		{
#ifdef PFC_WINDOWS_DESKTOP_APP
			TCHAR temp[512];
			if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,0,p_code,0,temp,_countof(temp),0) == 0) return FALSE;
			for(t_size n=0;n<_countof(temp);n++) {
				switch(temp[n]) {
				case '\n':
				case '\r':
					temp[n] = ' ';
					break;
				}
			}
			p_out = stringcvt::string_utf8_from_os(temp,_countof(temp));
			return TRUE;
#else
			return FALSE;
#endif
		}
	}
}
void winPrefixPath(pfc::string_base & out, const char * p_path) {
	if (pfc::string_has_prefix(p_path, "..\\") || strstr(p_path, "\\..\\") ) {
		// do not touch relative paths if we somehow got them here
		out = p_path;
		return;
	}
	const char * prepend_header = "\\\\?\\";
	const char * prepend_header_net = "\\\\?\\UNC\\";
	if (pfc::strcmp_partial( p_path, prepend_header ) == 0) { out = p_path; return; }
	out.reset();
	if (pfc::strcmp_partial(p_path,"\\\\") != 0) {
		out << prepend_header << p_path;
	} else {
		out << prepend_header_net << (p_path+2);
	}
};

BOOL winFormatSystemErrorMessage(pfc::string_base & p_out, DWORD p_code) {
	return winFormatSystemErrorMessageHook( p_out, p_code );
}
void winUnPrefixPath(pfc::string_base & out, const char * p_path) {
	const char * prepend_header = "\\\\?\\";
	const char * prepend_header_net = "\\\\?\\UNC\\";
	if (pfc::strcmp_partial(p_path, prepend_header_net) == 0) {
		out = PFC_string_formatter() << "\\\\" << (p_path + strlen(prepend_header_net) );
		return;
	}
	if (pfc::strcmp_partial(p_path, prepend_header) == 0) {
		out = (p_path + strlen(prepend_header));
		return;
	}
	out = p_path;
}

string8 winPrefixPath(const char * in) {
	string8 temp; winPrefixPath(temp, in); return temp;
}
string8 winUnPrefixPath(const char * in) {
	string8 temp; winUnPrefixPath(temp, in); return temp;
}

} // namespace pfc

pfc::string8 format_win32_error(DWORD p_code) {
	pfc::LastErrorRevertScope revert;
	pfc::string8 buffer;
	if (p_code == 0) buffer = "Undefined error";
	else if (!pfc::winFormatSystemErrorMessage(buffer,p_code)) buffer << "Unknown error code (" << (unsigned)p_code << ")";
	return buffer;
}

static void format_hresult_stamp_hex(pfc::string8 & buffer, HRESULT p_code) {
	buffer << " (0x" << pfc::format_hex((t_uint32)p_code, 8) << ")";
}

pfc::string8 format_hresult(HRESULT p_code) {
	pfc::string8 buffer;
	if (!pfc::winFormatSystemErrorMessage(buffer,(DWORD)p_code)) buffer = "Unknown error code";
	format_hresult_stamp_hex(buffer, p_code);
	return buffer;
}
pfc::string8 format_hresult(HRESULT p_code, const char * msgOverride) {
	pfc::string8 buffer = msgOverride;
	format_hresult_stamp_hex(buffer, p_code);
	return buffer;
}


#ifdef PFC_WINDOWS_DESKTOP_APP

namespace pfc {
	HWND findOwningPopup(HWND p_wnd)
	{
		HWND walk = p_wnd;
		while (walk != 0 && (GetWindowLong(walk, GWL_STYLE) & WS_CHILD) != 0)
			walk = GetParent(walk);
		return walk ? walk : p_wnd;
	}
	string8 getWindowClassName(HWND wnd) {
		TCHAR temp[1024] = {};
		if (GetClassName(wnd, temp, PFC_TABSIZE(temp)) == 0) {
			PFC_ASSERT(!"Should not get here");
			return "";
		}
		return pfc::stringcvt::string_utf8_from_os(temp).get_ptr();
	}
	void setWindowText(HWND wnd, const char * txt) {
		SetWindowText(wnd, stringcvt::string_os_from_utf8(txt));
	}
	string8 getWindowText(HWND wnd) {
		PFC_ASSERT(wnd != NULL);
		int len = GetWindowTextLength(wnd);
		if (len >= 0)
		{
			len++;
			pfc::array_t<TCHAR> temp;
			temp.set_size(len);
			temp[0] = 0;
			if (GetWindowText(wnd, temp.get_ptr(), len) > 0)
			{
				return stringcvt::string_utf8_from_os(temp.get_ptr(), len).get_ptr();
			}
		}
		return "";
	}
}

void uAddWindowStyle(HWND p_wnd,LONG p_style) {
	SetWindowLong(p_wnd,GWL_STYLE, GetWindowLong(p_wnd,GWL_STYLE) | p_style);
}

void uRemoveWindowStyle(HWND p_wnd,LONG p_style) {
	SetWindowLong(p_wnd,GWL_STYLE, GetWindowLong(p_wnd,GWL_STYLE) & ~p_style);
}

void uAddWindowExStyle(HWND p_wnd,LONG p_style) {
	SetWindowLong(p_wnd,GWL_EXSTYLE, GetWindowLong(p_wnd,GWL_EXSTYLE) | p_style);
}

void uRemoveWindowExStyle(HWND p_wnd,LONG p_style) {
	SetWindowLong(p_wnd,GWL_EXSTYLE, GetWindowLong(p_wnd,GWL_EXSTYLE) & ~p_style);
}

unsigned MapDialogWidth(HWND p_dialog,unsigned p_value) {
	RECT temp;
	temp.left = 0; temp.right = p_value; temp.top = temp.bottom = 0;
	if (!MapDialogRect(p_dialog,&temp)) return 0;
	return temp.right;
}

bool IsKeyPressed(unsigned vk) {
	return (GetKeyState(vk) & 0x8000) ? true : false;
}

//! Returns current modifier keys pressed, using win32 MOD_* flags.
unsigned GetHotkeyModifierFlags() {
	unsigned ret = 0;
	if (IsKeyPressed(VK_CONTROL)) ret |= MOD_CONTROL;
	if (IsKeyPressed(VK_SHIFT)) ret |= MOD_SHIFT;
	if (IsKeyPressed(VK_MENU)) ret |= MOD_ALT;
	if (IsKeyPressed(VK_LWIN) || IsKeyPressed(VK_RWIN)) ret |= MOD_WIN;
	return ret;
}



bool CClipboardOpenScope::Open(HWND p_owner) {
	Close();
	if (OpenClipboard(p_owner)) {
		m_open = true;
		return true;
	} else {
		return false;
	}
}
void CClipboardOpenScope::Close() {
	if (m_open) {
		m_open = false;
		CloseClipboard();
	}
}


CGlobalLockScope::CGlobalLockScope(HGLOBAL p_handle) : m_ptr(GlobalLock(p_handle)), m_handle(p_handle) {
	if (m_ptr == NULL) throw std::bad_alloc();
}
CGlobalLockScope::~CGlobalLockScope() {
	if (m_ptr != NULL) GlobalUnlock(m_handle);
}

bool IsPointInsideControl(const POINT& pt, HWND wnd) {
	HWND walk = WindowFromPoint(pt);
	for(;;) {
		if (walk == NULL) return false;
		if (walk == wnd) return true;
		if (GetWindowLong(walk,GWL_STYLE) & WS_POPUP) return false;
		walk = GetParent(walk);
	}
}
bool IsPopupWindowChildOf(HWND child, HWND parent) {
	HWND walk = child;
	while (walk != parent && walk != NULL) {
		walk = GetParent(walk);
	}
	return walk == parent;
}
bool IsWindowChildOf(HWND child, HWND parent) {
	HWND walk = child;
	while(walk != parent && walk != NULL && (GetWindowLong(walk,GWL_STYLE) & WS_CHILD) != 0) {
		walk = GetParent(walk);
	}
	return walk == parent;
}
void ResignActiveWindow(HWND wnd) {
	if (IsPopupWindowChildOf(GetActiveWindow(), wnd)) {
		HWND parent = GetParent(wnd);
		if ( parent != NULL ) SetActiveWindow(parent);
	}
}
void win32_menu::release() {
	if (m_menu != NULL) {
		DestroyMenu(m_menu);
		m_menu = NULL;
	}
}

void win32_menu::create_popup() {
	release();
	SetLastError(NO_ERROR);
	m_menu = CreatePopupMenu();
	if (m_menu == NULL) throw exception_win32(GetLastError());
}

#endif // #ifdef PFC_WINDOWS_DESKTOP_APP

void win32_event::create(bool p_manualreset,bool p_initialstate) {
	release();
	SetLastError(NO_ERROR);
	m_handle = CreateEvent(NULL,p_manualreset ? TRUE : FALSE, p_initialstate ? TRUE : FALSE,NULL);
	if (m_handle == NULL) throw exception_win32(GetLastError());
}

void win32_event::release() {
	HANDLE temp = detach();
	if (temp != NULL) CloseHandle(temp);
}


DWORD win32_event::g_calculate_wait_time(double p_seconds) {
	DWORD time = 0;
	if (p_seconds> 0) {
		time = pfc::rint32(p_seconds * 1000.0);
		if (time == 0) time = 1;
	} else if (p_seconds < 0) {
		time = INFINITE;
	}
	return time;
}

//! Returns true when signaled, false on timeout
bool win32_event::g_wait_for(HANDLE p_event,double p_timeout_seconds) {
	SetLastError(NO_ERROR);
 	DWORD status = WaitForSingleObject(p_event,g_calculate_wait_time(p_timeout_seconds));
	switch(status) {
	case WAIT_FAILED:
		throw exception_win32(GetLastError());
	case WAIT_OBJECT_0:
		return true;
	case WAIT_TIMEOUT:
		return false;
	default:
		pfc::crash();
	}
}

void win32_event::set_state(bool p_state) {
	PFC_ASSERT(m_handle != NULL);
	if (p_state) SetEvent(m_handle);
	else ResetEvent(m_handle);
}

size_t win32_event::g_multiWait(const HANDLE* events, size_t count, double timeout) {
	auto status = WaitForMultipleObjects((DWORD)count, events, FALSE, g_calculate_wait_time(timeout));
	size_t idx = (size_t)(status - WAIT_OBJECT_0);
	if (idx < count) {
		return idx;
	}
	if (status == WAIT_TIMEOUT) return SIZE_MAX;
	pfc::crash();
}

int win32_event::g_twoEventWait( HANDLE ev1, HANDLE ev2, double timeout ) {
    HANDLE h[2] = {ev1, ev2};
    switch(WaitForMultipleObjects(2, h, FALSE, g_calculate_wait_time( timeout ) )) {
		default:
			pfc::crash();
        case WAIT_TIMEOUT:
            return 0;
		case WAIT_OBJECT_0:
			return 1;
		case WAIT_OBJECT_0 + 1:
            return 2;
    }
}

int win32_event::g_twoEventWait( win32_event & ev1, win32_event & ev2, double timeout ) {
    return g_twoEventWait( ev1.get_handle(), ev2.get_handle(), timeout );
}

#ifdef PFC_WINDOWS_DESKTOP_APP

void win32_icon::release() {
	HICON temp = detach();
	if (temp != NULL) DestroyIcon(temp);
}


void win32_accelerator::load(HINSTANCE p_inst,const TCHAR * p_id) {
	release();
	SetLastError(NO_ERROR);
	m_accel = LoadAccelerators(p_inst,p_id);
	if (m_accel == NULL) {
		throw exception_win32(GetLastError());
	}
}
	
void win32_accelerator::release() {
	if (m_accel != NULL) {
		DestroyAcceleratorTable(m_accel);
		m_accel = NULL;
	}
}

#endif // #ifdef PFC_WINDOWS_DESKTOP_APP

void uSleepSeconds(double p_time,bool p_alertable) {
	SleepEx(win32_event::g_calculate_wait_time(p_time),p_alertable ? TRUE : FALSE);
}


#ifdef PFC_WINDOWS_DESKTOP_APP

WORD GetWindowsVersionCode() throw() {
	const DWORD ver = GetVersion();
	return (WORD)HIBYTE(LOWORD(ver)) | ((WORD)LOBYTE(LOWORD(ver)) << 8);
}


namespace pfc {
    bool isShiftKeyPressed() {
        return IsKeyPressed(VK_SHIFT);
    }
    bool isCtrlKeyPressed() {
        return IsKeyPressed(VK_CONTROL);
    }
    bool isAltKeyPressed() {
        return IsKeyPressed(VK_MENU);
    }

	void winSetThreadDescription(HANDLE hThread, const wchar_t * desc) {
#if _WIN32_WINNT >= 0xA00 
		SetThreadDescription(hThread, desc);
#else
		auto proc = GetProcAddress(GetModuleHandle(L"KernelBase.dll"), "SetThreadDescription");
		if (proc == nullptr) {
			proc = GetProcAddress(GetModuleHandle(L"kernel32.dll"), "SetThreadDescription");
		}
		if (proc != nullptr) {
			typedef HRESULT(__stdcall * pSetThreadDescription_t)(HANDLE hThread, PCWSTR lpThreadDescription);
			auto proc2 = reinterpret_cast<pSetThreadDescription_t>(proc);
			proc2(hThread, desc);
		}
#endif
	}
}

#else
// If unknown / not available on this architecture, return false always
namespace pfc {
	bool isShiftKeyPressed() {
		return false;
	}
	bool isCtrlKeyPressed() {
		return false;
	}
	bool isAltKeyPressed() {
		return false;
	}
}

#endif // #ifdef PFC_WINDOWS_DESKTOP_APP

namespace pfc {
    void winSleep( double seconds ) {
        DWORD ms = INFINITE;
        if (seconds > 0) {
            ms = rint32(seconds * 1000);
            if (ms < 1) ms = 1;
        } else if (seconds == 0) {
            ms = 0;
        }
        Sleep(ms);
    }
    void sleepSeconds(double seconds) {
        winSleep(seconds);
    }
    void yield() {
        Sleep(1);
    }

	static pfc::string8 winUnicodeNormalize(const char* str, NORM_FORM form) {
		pfc::string8 ret;
		if (str != nullptr && *str != 0) {
			auto w = wideFromUTF8(str);
			int needed = NormalizeString(form, w, -1, nullptr, 0);
			if (needed > 0) {
				pfc::array_t<wchar_t> buf; buf.resize(needed);
				int status = NormalizeString(form, w, -1, buf.get_ptr(), needed);
				if (status > 0) {
					ret = utf8FromWide(buf.get_ptr());
				}
			}
		}
		return ret;
	}
	pfc::string8 unicodeNormalizeD(const char* str) {
		return winUnicodeNormalize(str, NormalizationD);
	}
	pfc::string8 unicodeNormalizeC(const char* str) {
		return winUnicodeNormalize(str, NormalizationC);
	}
}

#endif // _WIN32
