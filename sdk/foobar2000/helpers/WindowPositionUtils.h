#pragma once

#include "win32_misc.h"

static BOOL AdjustWindowRectHelper(CWindow wnd, CRect & rc) {
	const DWORD style = wnd.GetWindowLong(GWL_STYLE), exstyle = wnd.GetWindowLong(GWL_EXSTYLE);
	return AdjustWindowRectEx(&rc,style,(style & WS_POPUP) ? wnd.GetMenu() != NULL : FALSE, exstyle);
}

static void AdjustRectToScreenArea(CRect & rc, CRect rcParent) {
	HMONITOR monitor = MonitorFromRect(rcParent,MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi = {sizeof(MONITORINFO)};
	if (GetMonitorInfo(monitor,&mi)) {
		const CRect clip = mi.rcWork;
		if (rc.right > clip.right) rc.OffsetRect(clip.right - rc.right, 0);
		if (rc.bottom > clip.bottom) rc.OffsetRect(0, clip.bottom - rc.bottom);
		if (rc.left < clip.left) rc.OffsetRect(clip.left - rc.left, 0);
		if (rc.top < clip.top) rc.OffsetRect(0, clip.top - rc.top);
	}
}

static BOOL GetClientRectAsSC(CWindow wnd, CRect & rc) {
	CRect temp;
	if (!wnd.GetClientRect(temp)) return FALSE;
	if (temp.IsRectNull()) return FALSE;
	if (!wnd.ClientToScreen(temp)) return FALSE;
	rc = temp;
	return TRUE;
}


static BOOL CenterWindowGetRect(CWindow wnd, CWindow wndParent, CRect & out) {
	CRect parent, child;
	if (!wndParent.GetWindowRect(&parent) || !wnd.GetWindowRect(&child)) return FALSE;
	{
		CPoint origin = parent.CenterPoint();
		origin.Offset( - child.Width() / 2, - child.Height() / 2);
		child.OffsetRect( origin - child.TopLeft() );
	}
	AdjustRectToScreenArea(child, parent);
	out = child;
	return TRUE;
}

static BOOL CenterWindowAbove(CWindow wnd, CWindow wndParent) {
	CRect rc;
	if (!CenterWindowGetRect(wnd, wndParent, rc)) return FALSE;
	return wnd.SetWindowPos(NULL,rc,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static BOOL ShowWindowCentered(CWindow wnd,CWindow wndParent) {
	CRect rc;
	if (!CenterWindowGetRect(wnd, wndParent, rc)) return FALSE;
	return wnd.SetWindowPos(HWND_TOP,rc,SWP_NOSIZE | SWP_SHOWWINDOW);
}

class cfgWindowSize : public cfg_var {
public:
	cfgWindowSize(const GUID & p_guid) : cfg_var(p_guid) {}
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {
		stream_writer_formatter<> str(*p_stream,p_abort); str << m_width << m_height;
	}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		stream_reader_formatter<> str(*p_stream,p_abort); str >> m_width >> m_height;
	}

	uint32_t m_width = UINT32_MAX, m_height = UINT32_MAX;
};

class cfgWindowSizeTracker {
public:
	cfgWindowSizeTracker(cfgWindowSize & p_var) : m_var(p_var) {}

	bool Apply(HWND p_wnd) {
		bool retVal = false;
		m_applied = false;
		if (m_var.m_width != ~0 && m_var.m_height != ~0) {
			CRect rect (0,0,m_var.m_width,m_var.m_height);
			if (AdjustWindowRectHelper(p_wnd, rect)) {
				SetWindowPos(p_wnd,NULL,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
				retVal = true;
			}
		}
		m_applied = true;
		return retVal;
	}

	BOOL ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT & lResult) {
		if (uMsg == WM_SIZE && m_applied) {
			if (lParam != 0) {
				m_var.m_width = (short)LOWORD(lParam); m_var.m_height = (short)HIWORD(lParam);
			}
		}
		return FALSE;
	}
private:
	cfgWindowSize & m_var;
	bool m_applied = false;
};

class cfgDialogSizeTracker : public cfgWindowSizeTracker {
public:
	cfgDialogSizeTracker(cfgWindowSize & p_var) : cfgWindowSizeTracker(p_var) {}
	BOOL ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT & lResult) {
		if (cfgWindowSizeTracker::ProcessWindowMessage(hWnd, uMsg, wParam, lParam, lResult)) return TRUE;
		if (uMsg == WM_INITDIALOG) Apply(hWnd);
		return FALSE;
	}
};

class cfgDialogPositionData {
public:
	cfgDialogPositionData() : m_width(sizeInvalid), m_height(sizeInvalid), m_posX(posInvalid), m_posY(posInvalid) {}
	
	void OverrideDefaultSize(t_uint32 width, t_uint32 height) {
		if (m_width == sizeInvalid && m_height == sizeInvalid) {
			m_width = width; m_height = height; m_posX = m_posY = posInvalid;
			m_dpiX = m_dpiY = 96;
		}
	}

	void AddWindow(CWindow wnd) {
		TryFetchConfig();
		m_windows += wnd;
		ApplyConfig(wnd);
	}
	void RemoveWindow(CWindow wnd) {
		if (m_windows.contains(wnd)) {
			StoreConfig(wnd); m_windows -= wnd;
		}
	}
	void FetchConfig() {TryFetchConfig();}

private:
	BOOL ApplyConfig(CWindow wnd) {
		ApplyDPI();
		CWindow wndParent = wnd.GetParent();
		UINT flags = SWP_NOACTIVATE | SWP_NOZORDER;
		CRect rc;
		if (!GetClientRectAsSC(wnd,rc)) return FALSE;
		if (m_width != sizeInvalid && m_height != sizeInvalid && (wnd.GetWindowLong(GWL_STYLE) & WS_SIZEBOX) != 0) {
			rc.right = rc.left + m_width;
			rc.bottom = rc.top + m_height;
		} else {
			flags |= SWP_NOSIZE;
		}
		if (wndParent != NULL) {
			CRect rcParent;
			if (GetParentWndRect(wndParent, rcParent)) {
				if (m_posX != posInvalid && m_posY != posInvalid) {
					rc.MoveToXY( rcParent.TopLeft() + CPoint(m_posX, m_posY) );
				} else {
					CPoint center = rcParent.CenterPoint();
					rc.MoveToXY( center.x - rc.Width() / 2, center.y - rc.Height() / 2);
				}
			}
		}
		if (!AdjustWindowRectHelper(wnd, rc)) return FALSE;

		DeOverlap(wnd, rc);

		{
			CRect rcAdjust(0,0,1,1);
			if (wndParent != NULL) {
				CRect temp;
				if (wndParent.GetWindowRect(temp)) rcAdjust = temp;
			}
			AdjustRectToScreenArea(rc, rcAdjust);
		}
		
		
		return wnd.SetWindowPos(NULL, rc, flags);
	}
	struct DeOverlapState {
		CWindow m_thisWnd;
		CPoint m_topLeft;
		bool m_match;
	};
	static BOOL CALLBACK MyEnumChildProc(HWND wnd, LPARAM param) {
		DeOverlapState * state = reinterpret_cast<DeOverlapState*>(param);
		if (wnd != state->m_thisWnd && IsWindowVisible(wnd) ) {
			CRect rc;
			if (GetWindowRect(wnd, rc)) {
				if (rc.TopLeft() == state->m_topLeft) {
					state->m_match = true; return FALSE;
				}
			}
		}
		return TRUE;
	}
	static bool DeOverlapTest(CWindow wnd, CPoint topLeft) {
		DeOverlapState state = {};
		state.m_thisWnd = wnd; state.m_topLeft = topLeft; state.m_match = false;
		EnumThreadWindows(GetCurrentThreadId(), MyEnumChildProc, reinterpret_cast<LPARAM>(&state));
		return state.m_match;
	}
	static int DeOverlapDelta() {
		return pfc::max_t<int>(GetSystemMetrics(SM_CYCAPTION),1);
	}
	static void DeOverlap(CWindow wnd, CRect & rc) {
		const int delta = DeOverlapDelta();
		for(;;) {
			if (!DeOverlapTest(wnd, rc.TopLeft())) break;
			rc.OffsetRect(delta,delta);
		}
	}
	BOOL StoreConfig(CWindow wnd) {
		CRect rc;
		if (!GetClientRectAsSC(wnd, rc)) return FALSE;
		const CSize DPI = QueryScreenDPIEx();
		m_dpiX = DPI.cx; m_dpiY = DPI.cy;
		m_width = rc.Width(); m_height = rc.Height();
		m_posX = m_posY = posInvalid;
		CWindow parent = wnd.GetParent();
		if (parent != NULL) {
			CRect rcParent;
			if (GetParentWndRect(parent, rcParent)) {
				m_posX = rc.left - rcParent.left;
				m_posY = rc.top - rcParent.top;
			}
		}
		return TRUE;
	}
	void TryFetchConfig() {
		for(auto walk = m_windows.cfirst(); walk.is_valid(); ++walk) {
			if (StoreConfig(*walk)) break;
		}
	}

	void ApplyDPI() {
		const CSize screenDPI = QueryScreenDPIEx();
		if (screenDPI.cx == 0 || screenDPI.cy == 0) {
			PFC_ASSERT(!"Should not get here - something seriously wrong with the OS");
			return;
		}
		if (m_dpiX != dpiInvalid && m_dpiX != screenDPI.cx) {
			if (m_width != sizeInvalid) m_width = MulDiv(m_width, screenDPI.cx, m_dpiX);
			if (m_posX != posInvalid) m_posX = MulDiv(m_posX, screenDPI.cx, m_dpiX);
		}
		if (m_dpiY != dpiInvalid && m_dpiY != screenDPI.cy) {
			if (m_height != sizeInvalid) m_height = MulDiv(m_height, screenDPI.cy, m_dpiY);
			if (m_posY != posInvalid) m_posY = MulDiv(m_posY, screenDPI.cy, m_dpiY);
		}
		m_dpiX = screenDPI.cx;
		m_dpiY = screenDPI.cy;
	}
	CSize GrabDPI() const {
		CSize DPI(96,96);
		if (m_dpiX != dpiInvalid) DPI.cx = m_dpiX;
		if (m_dpiY != dpiInvalid) DPI.cy = m_dpiY;
		return DPI;
	}

	static BOOL GetParentWndRect(CWindow wndParent, CRect & rc) {
		if (!wndParent.IsIconic()) {
			return wndParent.GetWindowRect(rc);
		}
		WINDOWPLACEMENT pl = {sizeof(pl)};
		if (!wndParent.GetWindowPlacement(&pl)) return FALSE;
		rc = pl.rcNormalPosition;
		return TRUE;
	}

	pfc::avltree_t<CWindow> m_windows;
public:
	t_uint32 m_width, m_height;
	t_int32 m_posX, m_posY;
	t_uint32 m_dpiX, m_dpiY;
	enum {
		posInvalid = 0x80000000,
		sizeInvalid = 0xFFFFFFFF,
		dpiInvalid = 0,
	};
};

FB2K_STREAM_READER_OVERLOAD(cfgDialogPositionData) {
	stream >> value.m_width >> value.m_height;
	try {
		stream >> value.m_posX >> value.m_posY >> value.m_dpiX >> value.m_dpiY;
	} catch(exception_io_data) {
		value.m_posX = value.m_posY = cfgDialogPositionData::posInvalid;
		value.m_dpiX = value.m_dpiY = cfgDialogPositionData::dpiInvalid;
	}
	return stream;
}
FB2K_STREAM_WRITER_OVERLOAD(cfgDialogPositionData) {
	return stream << value.m_width << value.m_height << value.m_posX << value.m_posY << value.m_dpiX << value.m_dpiY;
}

class cfgDialogPosition : public cfgDialogPositionData, public cfg_var {
public:
	cfgDialogPosition(const GUID & id) : cfg_var(id) {}
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {FetchConfig(); stream_writer_formatter<> str(*p_stream, p_abort); str << *pfc::implicit_cast<cfgDialogPositionData*>(this);}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {stream_reader_formatter<> str(*p_stream, p_abort); str >> *pfc::implicit_cast<cfgDialogPositionData*>(this);}
};

class cfgDialogPositionTracker {
public:
	cfgDialogPositionTracker(cfgDialogPosition & p_var) : m_var(p_var) {}
	~cfgDialogPositionTracker() {Cleanup();}

	BOOL ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT & lResult) {
		if (uMsg == WM_CREATE || uMsg == WM_INITDIALOG) {
			Cleanup();
			m_wnd = hWnd;
			m_var.AddWindow(m_wnd);
		} else if (uMsg == WM_DESTROY) {
			PFC_ASSERT( hWnd == m_wnd );
			Cleanup();
		}
		return FALSE;
	}

private:
	void Cleanup() {
		if (m_wnd != NULL) {
			m_var.RemoveWindow(m_wnd);
			m_wnd = NULL;
		}
	}
	cfgDialogPosition & m_var;
	CWindow m_wnd;
};

//! DPI-safe window size var \n
//! Stores size in pixel and original DPI\n
//! Use with cfgWindowSizeTracker2
class cfgWindowSize2 : public cfg_var {
public:
	cfgWindowSize2(const GUID & p_guid) : cfg_var(p_guid) {}
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {
		stream_writer_formatter<> str(*p_stream,p_abort); str << m_size.cx << m_size.cy << m_dpi.cx << m_dpi.cy;
	}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		stream_reader_formatter<> str(*p_stream,p_abort); str >> m_size.cx >> m_size.cy >> m_dpi.cx >> m_dpi.cy;
	}

	bool is_valid() const {
		return m_size.cx > 0 && m_size.cy > 0;
	}

	CSize get( CSize forDPI ) const {
		if ( forDPI == m_dpi ) return m_size;

		CSize ret;
		ret.cx = MulDiv( m_size.cx, forDPI.cx, m_dpi.cx );
		ret.cy = MulDiv( m_size.cy, forDPI.cy, m_dpi.cy );
		return ret;
	}

	CSize m_size = CSize(0,0), m_dpi = CSize(0,0);
};

//! Forward messages to this class to utilize cfgWindowSize2
class cfgWindowSizeTracker2 : public CMessageMap {
public:
	cfgWindowSizeTracker2( cfgWindowSize2 & var ) : m_var(var) {}

	BEGIN_MSG_MAP_EX(cfgWindowSizeTracker2)
		if (uMsg == WM_CREATE || uMsg == WM_INITDIALOG) {
			Apply(hWnd);
		}
		MSG_WM_SIZE( OnSize )
	END_MSG_MAP()

	bool Apply(HWND p_wnd) {
		bool retVal = false;
		m_applied = false;
		if (m_var.is_valid()) {
			CRect rect( CPoint(0,0), m_var.get( m_DPI ) );
			if (AdjustWindowRectHelper(p_wnd, rect)) {
				SetWindowPos(p_wnd,NULL,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
				retVal = true;
			}
		}
		m_applied = true;
		return retVal;
	}

private:
	void OnSize(UINT nType, CSize size) {
		if ( m_applied && size.cx > 0 && size.cy > 0 ) {
			m_var.m_size = size;
			m_var.m_dpi = m_DPI;
		}
		SetMsgHandled(FALSE);
	}
	cfgWindowSize2 & m_var;
	bool m_applied = false;
	const CSize m_DPI = QueryScreenDPIEx();
};