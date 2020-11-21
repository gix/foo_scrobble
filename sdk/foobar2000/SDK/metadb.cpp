#include "foobar2000.h"
#include "file_info_filter_impl.h"

void metadb::handle_create_replace_path_canonical(metadb_handle_ptr & p_out,const metadb_handle_ptr & p_source,const char * p_new_path) {
	handle_create(p_out,make_playable_location(p_new_path,p_source->get_subsong_index()));
}

void metadb::handle_create_replace_path(metadb_handle_ptr & p_out,const metadb_handle_ptr & p_source,const char * p_new_path) {
	pfc::string8 path;
	filesystem::g_get_canonical_path(p_new_path,path);
	handle_create_replace_path_canonical(p_out,p_source,path);
}

void metadb::handle_replace_path_canonical(metadb_handle_ptr & p_out,const char * p_new_path) {
	metadb_handle_ptr temp;
	handle_create_replace_path_canonical(temp,p_out,p_new_path);
	p_out = temp;
}


metadb_io::t_load_info_state metadb_io::load_info(metadb_handle_ptr p_item,t_load_info_type p_type,HWND p_parent_window,bool p_show_errors) {
	return load_info_multi(pfc::list_single_ref_t<metadb_handle_ptr>(p_item),p_type,p_parent_window,p_show_errors);
}

metadb_io::t_update_info_state metadb_io::update_info(metadb_handle_ptr p_item,file_info & p_info,HWND p_parent_window,bool p_show_errors)
{
	file_info * blah = &p_info;
	return update_info_multi(pfc::list_single_ref_t<metadb_handle_ptr>(p_item),pfc::list_single_ref_t<file_info*>(blah),p_parent_window,p_show_errors);
}


void metadb_io::hint_async(metadb_handle_ptr p_item,const file_info & p_info,const t_filestats & p_stats,bool p_fresh)
{
	const file_info * blargh = &p_info;
	hint_multi_async(pfc::list_single_ref_t<metadb_handle_ptr>(p_item),pfc::list_single_ref_t<const file_info *>(blargh),pfc::list_single_ref_t<t_filestats>(p_stats), pfc::bit_array_val(p_fresh));
}


bool metadb::g_get_random_handle(metadb_handle_ptr & p_out) {
	if (playback_control::get()->get_now_playing(p_out)) return true;

	{
		auto api = playlist_manager::get();

		t_size playlist_count = api->get_playlist_count();
		t_size active_playlist = api->get_active_playlist();
		if (active_playlist != ~0) {
			if (api->playlist_get_focus_item_handle(p_out,active_playlist)) return true;
		}

		for(t_size n = 0; n < playlist_count; n++) {
			if (api->playlist_get_focus_item_handle(p_out,n)) return true;
		}

		if (active_playlist != ~0) {
			t_size item_count = api->playlist_get_item_count(active_playlist);
			if (item_count > 0) {
				if (api->playlist_get_item_handle(p_out,active_playlist,0)) return true;
			}
		}
		
		for(t_size n = 0; n < playlist_count; n++) {
			t_size item_count = api->playlist_get_item_count(n);
			if (item_count > 0) {
				if (api->playlist_get_item_handle(p_out,n,0)) return true;
			}
		}
	}

	return false;
}


void metadb_io_v2::update_info_async_simple(const pfc::list_base_const_t<metadb_handle_ptr> & p_list,const pfc::list_base_const_t<const file_info*> & p_new_info, HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) {
	update_info_async(p_list,new service_impl_t<file_info_filter_impl>(p_list,p_new_info),p_parent_window,p_op_flags,p_notify);
}

void metadb_io_v2::on_file_rechaptered( const char * path, metadb_handle_list_cref newItems ) {
	metadb_handle_list handles( newItems );
	pfc::string8 pathLocal( path );
	auto notify = fb2k::makeCompletionNotify( [handles, pathLocal] (unsigned) {
		playlist_manager::get()->on_file_rechaptered( pathLocal, handles );
	} );

	load_info_async( handles, metadb_io::load_info_force, core_api::get_main_window(), metadb_io_v3::op_flag_delay_ui, notify );
}

void metadb_io_v2::on_files_rechaptered( metadb_handle_list_cref newHandles ) {
	metadb_handle_list local ( newHandles );
	auto notify = fb2k::makeCompletionNotify( [local] (unsigned) {
		playlist_manager::get()->on_files_rechaptered(local);
	} );

	load_info_async( newHandles, metadb_io::load_info_force, core_api::get_main_window(), metadb_io_v3::op_flag_delay_ui, notify );
}


metadb_hint_list::ptr metadb_hint_list::create() {
	return metadb_io_v2::get()->create_hint_list();
}

metadb_hint_list_v2::ptr metadb_hint_list_v2::create() {
	metadb_hint_list_v2::ptr ret;
	ret ^= metadb_hint_list::create();
	return ret;
}

metadb_hint_list_v3::ptr metadb_hint_list_v3::create() {
	metadb_hint_list_v3::ptr ret;
	ret ^= metadb_hint_list::create();
	return ret;
}
