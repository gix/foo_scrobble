#pragma once

#include "ref_counter.h"

namespace pfc {
	BOOL winFormatSystemErrorMessage(pfc::string_base & p_out,DWORD p_code);

	// Prefix filesystem paths with \\?\ and \\?\UNC where appropriate.
	void winPrefixPath(pfc::string_base & out, const char * p_path);
	// Reverse winPrefixPath
	void winUnPrefixPath(pfc::string_base & out, const char * p_path);

	string8 winPrefixPath( const char * in );
	string8 winUnPrefixPath( const char * in );

	class LastErrorRevertScope {
	public:
		LastErrorRevertScope() : m_val(GetLastError()) {}
		~LastErrorRevertScope() { SetLastError(m_val); }

	private:
		const DWORD m_val;
	};

	string8 getWindowText(HWND wnd);
	void setWindowText(HWND wnd, const char * txt); 
	string8 getWindowClassName( HWND wnd );
	HWND findOwningPopup(HWND wnd);
}

pfc::string8 format_win32_error(DWORD code);
pfc::string8 format_hresult(HRESULT code);
pfc::string8 format_hresult(HRESULT code, const char * msgOverride);

class exception_win32 : public std::exception {
public:
	exception_win32(DWORD p_code) : std::exception(format_win32_error(p_code)), m_code(p_code) {}
	DWORD get_code() const {return m_code;}
private:
	DWORD m_code;
};

#ifdef PFC_WINDOWS_DESKTOP_APP

void uAddWindowStyle(HWND p_wnd,LONG p_style);
void uRemoveWindowStyle(HWND p_wnd,LONG p_style);
void uAddWindowExStyle(HWND p_wnd,LONG p_style);
void uRemoveWindowExStyle(HWND p_wnd,LONG p_style);
unsigned MapDialogWidth(HWND p_dialog,unsigned p_value);
bool IsKeyPressed(unsigned vk);

//! Returns current modifier keys pressed, using win32 MOD_* flags.
unsigned GetHotkeyModifierFlags();

class CClipboardOpenScope {
public:
	CClipboardOpenScope() : m_open(false) {}
	~CClipboardOpenScope() {Close();}
	bool Open(HWND p_owner);
	void Close();
private:
	bool m_open;
	
	PFC_CLASS_NOT_COPYABLE_EX(CClipboardOpenScope)
};

class CGlobalLockScope {
public:
	CGlobalLockScope(HGLOBAL p_handle);
	~CGlobalLockScope();
	void * GetPtr() const {return m_ptr;}
	t_size GetSize() const {return GlobalSize(m_handle);}
private:
	void * m_ptr;
	HGLOBAL m_handle;

	PFC_CLASS_NOT_COPYABLE_EX(CGlobalLockScope)
};

template<typename TItem> class CGlobalLockScopeT {
public:
	CGlobalLockScopeT(HGLOBAL handle) : m_scope(handle) {}
	TItem * GetPtr() const {return reinterpret_cast<TItem*>(m_scope.GetPtr());}
	t_size GetSize() const {
		const t_size val = m_scope.GetSize();
		PFC_ASSERT( val % sizeof(TItem) == 0 );
		return val / sizeof(TItem);
	}
private:
	CGlobalLockScope m_scope;
};

//! Resigns active window status passing it to the parent window, if wnd or a child popup of is active. \n
//! Use this to mitigate Windows 10 1809 active window handling bugs - call prior to DestroyWindow()
void ResignActiveWindow(HWND wnd);
//! Is point inside a control?
bool IsPointInsideControl(const POINT& pt, HWND wnd);
//! Is <child> a control inside <parent> window? Also returns true if child==parent.
bool IsWindowChildOf(HWND child, HWND parent);
//! Is <child> window a child (popup or control) of <parent> window? Also returns true if child==parent.
bool IsPopupWindowChildOf(HWND child, HWND parent);

class win32_menu {
public:
	win32_menu(HMENU p_initval) : m_menu(p_initval) {}
	win32_menu() : m_menu(NULL) {}
	~win32_menu() {release();}
	void release();
	void set(HMENU p_menu) {release(); m_menu = p_menu;}
	void create_popup();
	HMENU get() const {return m_menu;}
	HMENU detach() {return pfc::replace_t(m_menu,(HMENU)NULL);}
	
	bool is_valid() const {return m_menu != NULL;}
private:
	win32_menu(const win32_menu &) = delete;
	void operator=(const win32_menu &) = delete;

	HMENU m_menu;
};

#endif

class win32_event {
public:
	win32_event() : m_handle(NULL) {}
	~win32_event() {release();}

	void create(bool p_manualreset,bool p_initialstate);
	
	void set(HANDLE p_handle) {release(); m_handle = p_handle;}
	HANDLE get() const {return m_handle;}
	HANDLE get_handle() const {return m_handle;}
	HANDLE detach() {return pfc::replace_t(m_handle,(HANDLE)NULL);}
	bool is_valid() const {return m_handle != NULL;}
	
	void release();

	//! Returns true when signaled, false on timeout
	bool wait_for(double p_timeout_seconds) {return g_wait_for(get(),p_timeout_seconds);}
	
	static DWORD g_calculate_wait_time(double p_seconds);

	//! Returns true when signaled, false on timeout
	static bool g_wait_for(HANDLE p_event,double p_timeout_seconds);

	void set_state(bool p_state);
	bool is_set() { return wait_for(0); }

    // Two-wait event functions, return 0 on timeout, 1 on evt1 set, 2 on evt2 set
    static int g_twoEventWait( win32_event & ev1, win32_event & ev2, double timeout );
    static int g_twoEventWait( HANDLE ev1, HANDLE ev2, double timeout );

	// Multi-wait. Returns SIZE_MAX on timeout, 0 based event index if either event becomes set.
	static size_t g_multiWait(const HANDLE* events, size_t count, double timeout);
private:
	win32_event(const win32_event&) = delete;
	void operator=(const win32_event &) = delete;

	HANDLE m_handle;
};

namespace pfc {
	typedef HANDLE eventHandle_t;

	static const eventHandle_t eventInvalid = NULL;

	class event : public win32_event {
	public:
		event() { create(true, false); }

		HANDLE get_handle() const { return win32_event::get(); }
	};
}

void uSleepSeconds(double p_time,bool p_alertable);

#ifdef PFC_WINDOWS_DESKTOP_APP

class win32_icon {
public:
	win32_icon(HICON p_initval) : m_icon(p_initval) {}
	win32_icon() : m_icon(NULL) {}
	~win32_icon() {release();}

	void release();

	void set(HICON p_icon) {release(); m_icon = p_icon;}
	HICON get() const {return m_icon;}
	HICON detach() {return pfc::replace_t(m_icon,(HICON)NULL);}

	bool is_valid() const {return m_icon != NULL;}

private:
	win32_icon(const win32_icon&) = delete;
	const win32_icon & operator=(const win32_icon &) = delete;

	HICON m_icon;
};

class win32_accelerator {
public:
	win32_accelerator() : m_accel(NULL) {}
	~win32_accelerator() {release();}
	HACCEL get() const {return m_accel;}

	void load(HINSTANCE p_inst,const TCHAR * p_id);
	void release();
private:
	HACCEL m_accel;
	PFC_CLASS_NOT_COPYABLE_EX(win32_accelerator);
};

class SelectObjectScope {
public:
	SelectObjectScope(HDC p_dc,HGDIOBJ p_obj) throw() : m_dc(p_dc), m_obj(SelectObject(p_dc,p_obj)) {}
	~SelectObjectScope() throw() {SelectObject(m_dc,m_obj);}
private:
	PFC_CLASS_NOT_COPYABLE_EX(SelectObjectScope)
	HDC m_dc;
	HGDIOBJ m_obj;
};

// WARNING: Windows is known to truncate the coordinates to float32 internally instead of retaining original int
// With large values, this OffsetWindowOrgEx behaves erratically
class OffsetWindowOrgScope {
public:
	OffsetWindowOrgScope(HDC dc, const POINT & pt) throw() : m_dc(dc), m_pt(pt) {
		OffsetWindowOrgEx(m_dc, m_pt.x, m_pt.y, NULL);
	}
	~OffsetWindowOrgScope() throw() {
		OffsetWindowOrgEx(m_dc, -m_pt.x, -m_pt.y, NULL);
	}

private:
	const HDC m_dc;
	const POINT m_pt;
};
class DCStateScope {
public:
	DCStateScope(HDC p_dc) throw() : m_dc(p_dc) {
		m_state = SaveDC(m_dc);
	}
	~DCStateScope() throw() {
		RestoreDC(m_dc,m_state);
	}
private:
	const HDC m_dc;
	int m_state;
};
#endif // #ifdef PFC_WINDOWS_DESKTOP_APP

class exception_com : public std::exception {
public:
	exception_com(HRESULT p_code) : std::exception(format_hresult(p_code)), m_code(p_code) {}
	exception_com(HRESULT p_code, const char * msg) : std::exception(format_hresult(p_code, msg)), m_code(p_code) {}
	HRESULT get_code() const {return m_code;}
private:
	HRESULT m_code;
};

#ifdef PFC_WINDOWS_DESKTOP_APP

// Same format as _WIN32_WINNT macro.
WORD GetWindowsVersionCode() throw();

#endif

//! Simple implementation of a COM reference counter. The initial reference count is zero, so it can be used with pfc::com_ptr_t<> with plain operator=/constructor rather than attach().
template<typename TBase> class ImplementCOMRefCounter : public TBase {
public:
    template<typename ... arg_t> ImplementCOMRefCounter(arg_t && ... arg) : TBase(std::forward<arg_t>(arg) ...) {}

	ULONG STDMETHODCALLTYPE AddRef() override {
		return ++m_refcounter;
	}
	ULONG STDMETHODCALLTYPE Release() override {
		long val = --m_refcounter;
		if (val == 0) delete this;
		return val;
	}
protected:
	virtual ~ImplementCOMRefCounter() {}
private:
	pfc::refcounter m_refcounter;
};



template<typename TPtr>
class CoTaskMemObject {
public:
	CoTaskMemObject() : m_ptr() {}

	~CoTaskMemObject() {CoTaskMemFree(m_ptr);}
	void Reset() {CoTaskMemFree(pfc::replace_null_t(m_ptr));}
	TPtr * Receive() {Reset(); return &m_ptr;}

	TPtr m_ptr;
	PFC_CLASS_NOT_COPYABLE(CoTaskMemObject, CoTaskMemObject<TPtr> );
};


namespace pfc {
    bool isShiftKeyPressed();
    bool isCtrlKeyPressed();
    bool isAltKeyPressed();

	class winHandle {
	public:
		winHandle(HANDLE h_ = INVALID_HANDLE_VALUE) : h(h_) {}
		~winHandle() { Close(); }
		void Close() {
			if (h != INVALID_HANDLE_VALUE && h != NULL ) { CloseHandle(h); h = INVALID_HANDLE_VALUE; }
		}

		void Attach(HANDLE h_) { Close(); h = h_; }
		HANDLE Detach() { HANDLE t = h; h = INVALID_HANDLE_VALUE; return t; }

		HANDLE Get() const { return h; }
		operator HANDLE() const { return h; }

		HANDLE h;
	private:
		winHandle(const winHandle&) = delete;
		void operator=(const winHandle&) = delete;
	};
    
    void winSleep( double seconds );
    void sleepSeconds(double seconds);
    void yield();

#ifdef PFC_WINDOWS_DESKTOP_APP
	void winSetThreadDescription(HANDLE hThread, const wchar_t * desc);
#endif // PFC_WINDOWS_DESKTOP_APP
}
