#include "foobar2000.h"

namespace {
	struct sysColorMapping_t {
		GUID guid; int idx;
	};
	static const sysColorMapping_t sysColorMapping[] = {
		{ ui_color_text, COLOR_WINDOWTEXT },
		{ ui_color_background, COLOR_WINDOW },
		{ ui_color_highlight, COLOR_HOTLIGHT },
		{ui_color_selection, COLOR_HIGHLIGHT},
	};
}
int ui_color_to_sys_color_index(const GUID & p_guid) {
	for( unsigned i = 0; i < PFC_TABSIZE( sysColorMapping ); ++ i ) {
		if ( p_guid == sysColorMapping[i].guid ) return sysColorMapping[i].idx;
	}
	return -1;
}
GUID ui_color_from_sys_color_index(int idx) {
	for (unsigned i = 0; i < PFC_TABSIZE(sysColorMapping); ++i) {
		if (idx == sysColorMapping[i].idx) return sysColorMapping[i].guid;
	}
	return pfc::guid_null;
}

namespace {
	class ui_element_config_impl : public ui_element_config {
	public:
		ui_element_config_impl(const GUID & guid) : m_guid(guid) {}
		ui_element_config_impl(const GUID & guid, const void * buffer, t_size size) : m_guid(guid) {
			m_content.set_data_fromptr(reinterpret_cast<const t_uint8*>(buffer),size);
		}

		void * get_data_var() {return m_content.get_ptr();}
		void set_data_size(t_size size) {m_content.set_size(size);}

		GUID get_guid() const {return m_guid;}
		const void * get_data() const {return m_content.get_ptr();}
		t_size get_data_size() const {return m_content.get_size();}
	private:
		const GUID m_guid;
		pfc::array_t<t_uint8> m_content;
	};

}

service_ptr_t<ui_element_config> ui_element_config::g_create(const GUID& id, const void * data, t_size size) {
	return new service_impl_t<ui_element_config_impl>(id,data,size);
}

service_ptr_t<ui_element_config> ui_element_config::g_create(const GUID & id, stream_reader * in, t_size bytes, abort_callback & abort) {
	service_ptr_t<ui_element_config_impl> data = new service_impl_t<ui_element_config_impl>(id);
	data->set_data_size(bytes);
	in->read_object(data->get_data_var(),bytes,abort);
	return data;
}

service_ptr_t<ui_element_config> ui_element_config::g_create(stream_reader * in, t_size bytes, abort_callback & abort) {
	if (bytes < sizeof(GUID)) throw exception_io_data_truncation();
	GUID id; 
	{ stream_reader_formatter<> in(*in,abort); in >> id;}
	return g_create(id,in,bytes - sizeof(GUID),abort);
}

ui_element_config::ptr ui_element_config_parser::subelement(t_size size) {
	return ui_element_config::g_create(&m_stream, size, m_abort);
}
ui_element_config::ptr ui_element_config_parser::subelement(const GUID & id, t_size dataSize) {
	return ui_element_config::g_create(id, &m_stream, dataSize, m_abort);
}

service_ptr_t<ui_element_config> ui_element_config::g_create(const void * data, t_size size) {
	stream_reader_memblock_ref stream(data,size);
	return g_create(&stream,size,fb2k::noAbort);
}

bool ui_element_subclass_description(const GUID & id, pfc::string_base & p_out) {
	if (id == ui_element_subclass_playlist_renderers) {
		p_out = "Playlist Renderers"; return true;
	} else if (id == ui_element_subclass_media_library_viewers) {
		p_out = "Media Library Viewers"; return true;
	} else if (id == ui_element_subclass_selection_information) {
		p_out = "Selection Information"; return true;
	} else if (id == ui_element_subclass_playback_visualisation) {
		p_out = "Playback Visualization"; return true;
	} else if (id == ui_element_subclass_playback_information) {
		p_out = "Playback Information"; return true;
	} else if (id == ui_element_subclass_utility) {
		p_out = "Utility"; return true;
	} else if (id == ui_element_subclass_containers) {
		p_out = "Containers"; return true;
	} else if ( id == ui_element_subclass_dsp ) {
		p_out = "DSP"; return true;
	} else {
		return false;
	}
}

bool ui_element::get_element_group(pfc::string_base & p_out) {
	return ui_element_subclass_description(get_subclass(),p_out);
}

t_ui_color ui_element_instance_callback::query_std_color(const GUID & p_what) {
#ifdef _WIN32
	t_ui_color ret;
	if (query_color(p_what,ret)) return ret;
	int idx = ui_color_to_sys_color_index(p_what);
	if (idx < 0) return 0;//should not be triggerable
	return GetSysColor(idx);
#else
#error portme
#endif
}
#ifdef _WIN32
t_ui_color ui_element_instance_callback::getSysColor(int sysColorIndex) {
	GUID guid = ui_color_from_sys_color_index( sysColorIndex );
	if ( guid != pfc::guid_null ) return query_std_color(guid);
	return GetSysColor(sysColorIndex);
}
#endif

bool ui_element::g_find(service_ptr_t<ui_element> & out, const GUID & id) {
	return service_by_guid(out, id);
}

bool ui_element::g_get_name(pfc::string_base & p_out,const GUID & p_guid) {
	ui_element::ptr ptr; if (!g_find(ptr,p_guid)) return false;
	ptr->get_name(p_out); return true;
}

bool ui_element_instance_callback::is_elem_visible_(service_ptr_t<class ui_element_instance> elem) {
	ui_element_instance_callback_v2::ptr v2;
	if (!this->service_query_t(v2)) {
		PFC_ASSERT(!"Should not get here - somebody implemented ui_element_instance_callback but not ui_element_instance_callback_v2.");
		return true;
	}
	return v2->is_elem_visible(elem);
}

bool ui_element_instance_callback::set_elem_label(ui_element_instance * source, const char * label) {
	return notify_(source, ui_element_host_notify_set_elem_label, 0, label, strlen(label)) != 0;
}

t_uint32 ui_element_instance_callback::get_dialog_texture(ui_element_instance * source) {
	return (t_uint32) notify_(source, ui_element_host_notify_get_dialog_texture, 0, NULL, 0);
}

bool ui_element_instance_callback::is_border_needed(ui_element_instance * source) {
	return notify_(source, ui_element_host_notify_is_border_needed, 0, NULL, 0) != 0;
}

t_size ui_element_instance_callback::notify_(ui_element_instance * source, const GUID & what, t_size param1, const void * param2, t_size param2size) {
	ui_element_instance_callback_v3::ptr v3;
	if (!this->service_query_t(v3)) { PFC_ASSERT(!"Outdated UI Element host implementation"); return 0; }
	return v3->notify(source, what, param1, param2, param2size);
}


const ui_element_min_max_info & ui_element_min_max_info::operator|=(const ui_element_min_max_info & p_other) {
	m_min_width = pfc::max_t(m_min_width,p_other.m_min_width);
	m_min_height = pfc::max_t(m_min_height,p_other.m_min_height);
	m_max_width = pfc::min_t(m_max_width,p_other.m_max_width);
	m_max_height = pfc::min_t(m_max_height,p_other.m_max_height);
	return *this;
}
ui_element_min_max_info ui_element_min_max_info::operator|(const ui_element_min_max_info & p_other) const {
	ui_element_min_max_info ret(*this);
	ret |= p_other;
	return ret;
}

void ui_element_min_max_info::adjustForWindow(HWND wnd) {
	RECT client = {0,0,10,10};
	RECT adjusted = client;
	BOOL bMenu = FALSE;
	const DWORD style = (DWORD) GetWindowLong( wnd, GWL_STYLE );
	if ( style & WS_POPUP ) {
		bMenu = GetMenu( wnd ) != NULL;
	}
	if (AdjustWindowRectEx( &adjusted, style, bMenu, GetWindowLong(wnd, GWL_EXSTYLE) )) {
		int dx = (adjusted.right - adjusted.left) - (client.right - client.left);
		int dy = (adjusted.bottom - adjusted.top) - (client.bottom - client.top);
		if ( dx < 0 ) dx = 0;
		if ( dy < 0 ) dy = 0;
		m_min_width += dx;
		m_min_height += dy;
		if ( m_max_width != ~0 ) m_max_width += dx;
		if ( m_max_height != ~0 ) m_max_height += dy;
	}
}
//! Retrieves element's minimum/maximum window size. Default implementation will fall back to WM_GETMINMAXINFO.
ui_element_min_max_info ui_element_instance::get_min_max_info() {
	ui_element_min_max_info ret;
	MINMAXINFO temp = {};
	temp.ptMaxTrackSize.x = 1024*1024;//arbitrary huge number
	temp.ptMaxTrackSize.y = 1024*1024;
	SendMessage(this->get_wnd(),WM_GETMINMAXINFO,0,(LPARAM)&temp);
	if (temp.ptMinTrackSize.x >= 0) ret.m_min_width = temp.ptMinTrackSize.x;
	if (temp.ptMaxTrackSize.x > 0) ret.m_max_width = temp.ptMaxTrackSize.x;
	if (temp.ptMinTrackSize.y >= 0) ret.m_min_height = temp.ptMinTrackSize.y;
	if (temp.ptMaxTrackSize.y > 0) ret.m_max_height = temp.ptMaxTrackSize.y;
	return ret;
}


namespace {
	class ui_element_replace_dialog_notify_impl : public ui_element_replace_dialog_notify {
	public:
		void on_cancelled() {
			reply(pfc::guid_null);
		}
		void on_ok(const GUID & guid) {
			reply(guid);
		}
		std::function<void(GUID)> reply;
	};
}
ui_element_replace_dialog_notify::ptr ui_element_replace_dialog_notify::create(std::function<void(GUID)> reply) {
	auto obj = fb2k::service_new<ui_element_replace_dialog_notify_impl>();
	obj->reply = reply;
	return obj;
}