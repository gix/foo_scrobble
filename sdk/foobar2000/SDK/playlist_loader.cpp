#include "foobar2000.h"

#if FOOBAR2000_TARGET_VERSION >= 76
static void process_path_internal(const char * p_path,const service_ptr_t<file> & p_reader,playlist_loader_callback::ptr callback, abort_callback & abort,playlist_loader_callback::t_entry_type type,const t_filestats & p_stats);

namespace {
	class archive_callback_impl : public archive_callback {
	public:
		archive_callback_impl(playlist_loader_callback::ptr p_callback, abort_callback & p_abort) : m_callback(p_callback), m_abort(p_abort) {}
		bool on_entry(archive * owner,const char * p_path,const t_filestats & p_stats,const service_ptr_t<file> & p_reader)
		{
			process_path_internal(p_path,p_reader,m_callback,m_abort,playlist_loader_callback::entry_directory_enumerated,p_stats);
			return !m_abort.is_aborting();
		}
		bool is_aborting() const {return m_abort.is_aborting();}
		abort_callback_event get_abort_event() const {return m_abort.get_abort_event();}
	private:
		const playlist_loader_callback::ptr m_callback;
		abort_callback & m_abort;
	};
}

bool playlist_loader::g_try_load_playlist(file::ptr fileHint,const char * p_path,playlist_loader_callback::ptr p_callback, abort_callback & p_abort) {
	pfc::string8 filepath;

	filesystem::g_get_canonical_path(p_path,filepath);
	
	auto extension = pfc::string_extension(filepath);

	service_ptr_t<file> l_file = fileHint;

	if (l_file.is_empty()) {
		filesystem::ptr fs;
		if (filesystem::g_get_interface(fs,filepath)) {
			if (fs->supports_content_types()) {
				fs->open(l_file,filepath,filesystem::open_mode_read,p_abort);
			}
		}
	}

	{
		service_enum_t<playlist_loader> e;

		if (l_file.is_valid()) {
			pfc::string8 content_type;
			if (l_file->get_content_type(content_type)) {
				service_ptr_t<playlist_loader> l;
				e.reset(); while(e.next(l)) {
					if (l->is_our_content_type(content_type)) {
						try {
							TRACK_CODE("playlist_loader::open",l->open(filepath,l_file,p_callback, p_abort));
							return true;
						} catch(exception_io_unsupported_format) {
							l_file->reopen(p_abort);
						}
					}
				}
			}
		}

		if (extension.length()>0) {
			playlist_loader::ptr l;
			e.reset(); while(e.next(l)) {
				if (stricmp_utf8(l->get_extension(),extension) == 0) {
					if (l_file.is_empty()) filesystem::g_open_read(l_file,filepath,p_abort);
					try {
						TRACK_CODE("playlist_loader::open",l->open(filepath,l_file,p_callback,p_abort));
						return true;
					} catch(exception_io_unsupported_format) {
						l_file->reopen(p_abort);
					}
				}
			}
		}
	}

	return false;
}

void playlist_loader::g_load_playlist_filehint(file::ptr fileHint,const char * p_path,playlist_loader_callback::ptr p_callback, abort_callback & p_abort) {
	if (!g_try_load_playlist(fileHint, p_path, p_callback, p_abort)) throw exception_io_unsupported_format();
}

void playlist_loader::g_load_playlist(const char * p_path,playlist_loader_callback::ptr callback, abort_callback & abort) {
	g_load_playlist_filehint(NULL,p_path,callback,abort);
}

static void index_tracks_helper(const char * p_path,const service_ptr_t<file> & p_reader,const t_filestats & p_stats,playlist_loader_callback::t_entry_type p_type,playlist_loader_callback::ptr p_callback, abort_callback & p_abort,bool & p_got_input)
{
	TRACK_CALL_TEXT("index_tracks_helper");
	if (p_reader.is_empty() && filesystem::g_is_remote_safe(p_path))
	{
		TRACK_CALL_TEXT("remote");
		metadb_handle_ptr handle;
		p_callback->handle_create(handle,make_playable_location(p_path,0));
		p_got_input = true;
		p_callback->on_entry(handle,p_type,p_stats,true);
	} else {
		TRACK_CALL_TEXT("hintable");
		service_ptr_t<input_info_reader> instance;
		try {
			input_entry::g_open_for_info_read(instance,p_reader,p_path,p_abort);
		} catch(exception_io_unsupported_format) {
			// specifically bail
			throw;
		} catch(exception_io) {
			// broken file or some other error, open() failed - show it anyway
			metadb_handle_ptr handle;
			p_callback->handle_create(handle, make_playable_location(p_path, 0));
			p_callback->on_entry(handle, p_type, p_stats, true);
			return;
		}

		t_filestats stats = instance->get_file_stats(p_abort);

		t_uint32 subsong,subsong_count = instance->get_subsong_count();
		bool bInfoGetError = false;
		for(subsong=0;subsong<subsong_count;subsong++)
		{
			TRACK_CALL_TEXT("subsong-loop");
			p_abort.check();
			metadb_handle_ptr handle;
			t_uint32 index = instance->get_subsong(subsong);
			p_callback->handle_create(handle,make_playable_location(p_path,index));

			p_got_input = true;
			if (! bInfoGetError && p_callback->want_info(handle,p_type,stats,true) )
			{
				file_info_impl info;
				try {
					TRACK_CODE("get_info",instance->get_info(index,info,p_abort));
				} catch(...) {
					bInfoGetError = true;
				}
				if (! bInfoGetError ) {
					p_callback->on_entry_info(handle,p_type,stats,info,true);
				}
			}
			else
			{
				p_callback->on_entry(handle,p_type,stats,true);
			}
		}
	}
}

static void track_indexer__g_get_tracks_wrap(const char * p_path,const service_ptr_t<file> & p_reader,const t_filestats & p_stats,playlist_loader_callback::t_entry_type p_type,playlist_loader_callback::ptr p_callback, abort_callback & p_abort) {
	bool got_input = false;
	bool fail = false;
	try {
		index_tracks_helper(p_path,p_reader,p_stats,p_type,p_callback,p_abort, got_input);
	} catch(exception_aborted) {
		throw;
	} catch(exception_io_unsupported_format) {
		fail = true;
	} catch(std::exception const & e) {
		fail = true;
		FB2K_console_formatter() << "could not enumerate tracks (" << e << ") on:\n" << file_path_display(p_path);
	}
	if (fail) {
		if (!got_input && !p_abort.is_aborting()) {
			if (p_type == playlist_loader_callback::entry_user_requested)
			{
				metadb_handle_ptr handle;
				p_callback->handle_create(handle,make_playable_location(p_path,0));
				p_callback->on_entry(handle,p_type,p_stats,true);
			}
		}
	}
}

namespace {

	static bool queryAddHidden() {
		// {2F9F4956-363F-4045-9531-603B1BF39BA8}
		static const GUID guid_cfg_addhidden =
		{ 0x2f9f4956, 0x363f, 0x4045,{ 0x95, 0x31, 0x60, 0x3b, 0x1b, 0xf3, 0x9b, 0xa8 } };

		advconfig_entry_checkbox::ptr ptr;
		if (advconfig_entry::g_find_t(ptr, guid_cfg_addhidden)) {
			return ptr->get_state();
		}
		return false;
	}

	// SPECIAL HACK
	// filesystem service does not present file hidden attrib but we want to weed files/folders out
	// so check separately on all native paths (inefficient but meh)
	class directory_callback_myimpl : public directory_callback
	{
	public:
		directory_callback_myimpl() : m_addHidden(queryAddHidden()) {}

		bool on_entry(filesystem * owner,abort_callback & p_abort,const char * url,bool is_subdirectory,const t_filestats & p_stats) {
			p_abort.check();

			filesystem_v2::ptr v2;
			v2 &= owner;

			if ( is_subdirectory ) {
				try {
					if (v2.is_valid()) {
						v2->list_directory_ex(url, *this, flags(), p_abort);
					} else {
						owner->list_directory(url, *this, p_abort);
					}
				} catch (exception_io const& e) {
					FB2K_console_formatter() << "Error walking directory (" << e << "): " << url;
				}
			} else {
				// In fb2k 1.4 the default filesystem is v2 and performs hidden file checks
#if FOOBAR2000_TARGET_VERSION < 79
				if ( ! m_addHidden && v2.is_empty() ) {
					const char * n = url;
					if (_extract_native_path_ptr(n)) {
						DWORD att = uGetFileAttributes(n);
						if (att == ~0 || (att & FILE_ATTRIBUTE_HIDDEN) != 0) return true;
					}
				}
#endif
				auto i = m_entries.insert_last();
				i->m_path = url;
				i->m_stats = p_stats;
			}
			return true;
		}

		uint32_t flags() const {
			uint32_t flags = listMode::filesAndFolders;
			if (m_addHidden) flags |= listMode::hidden;
			return flags;
		}

		const bool m_addHidden;
		struct entry_t {
			pfc::string8 m_path;
			t_filestats m_stats;
		};
		pfc::chain_list_v2_t<entry_t> m_entries;

	};
}


static void process_path_internal(const char * p_path,const service_ptr_t<file> & p_reader,playlist_loader_callback::ptr callback, abort_callback & abort,playlist_loader_callback::t_entry_type type,const t_filestats & p_stats)
{
	//p_path must be canonical

	abort.check();

	callback->on_progress(p_path);

	
	{
		if (p_reader.is_empty() && type != playlist_loader_callback::entry_directory_enumerated) {
			try {
				directory_callback_myimpl results;
				auto fs = filesystem::get(p_path);
				filesystem_v2::ptr v2;
				if ( v2 &= fs ) v2->list_directory_ex(p_path, results, results.flags(), abort );
				else fs->list_directory(p_path, results, abort);
				for( auto i = results.m_entries.first(); i.is_valid(); ++i ) {
					try {
						process_path_internal(i->m_path, 0, callback, abort, playlist_loader_callback::entry_directory_enumerated, i->m_stats);
					} catch (exception_aborted) {
						throw;
					} catch (std::exception const& e) {
						FB2K_console_formatter() << "Error walking path (" << e << "): " << file_path_display(i->m_path);
					} catch (...) {
						FB2K_console_formatter() << "Error walking path (bad exception): " << file_path_display(i->m_path);
					}
				}
				return; // successfully enumerated directory - go no further
			} catch(exception_aborted) {
				throw;
			} catch (exception_io_not_directory) {
				// disregard
			} catch(exception_io_not_found) {
				// disregard
			} catch (std::exception const& e) {
				FB2K_console_formatter() << "Error walking directory (" << e << "): " << p_path;
			} catch (...) {
				FB2K_console_formatter() << "Error walking directory (bad exception): " << p_path;
			}
		}

		bool found = false;


		{
			archive_callback_impl archive_results(callback, abort);
			service_enum_t<filesystem> e;
			service_ptr_t<filesystem> f;
			while(e.next(f)) {
				abort.check();
				service_ptr_t<archive> arch;
				if (f->service_query_t(arch) && arch->is_our_archive(p_path)) {
					if (p_reader.is_valid()) p_reader->reopen(abort);

					try {
						TRACK_CODE("archive::archive_list",arch->archive_list(p_path,p_reader,archive_results,true));
						return;
					} catch(exception_aborted) {throw;} 
					catch(...) {}
				}
			} 
		}
	}

	

	{
		service_ptr_t<link_resolver> ptr;
		if (link_resolver::g_find(ptr,p_path))
		{
			if (p_reader.is_valid()) p_reader->reopen(abort);

			pfc::string8 temp;
			try {
				TRACK_CODE("link_resolver::resolve",ptr->resolve(p_reader,p_path,temp,abort));

				track_indexer__g_get_tracks_wrap(temp,0,filestats_invalid,playlist_loader_callback::entry_from_playlist,callback, abort);
				return;//success
			} catch(exception_aborted) {throw;}
			catch(...) {}
		}
	}

	if (callback->is_path_wanted(p_path,type)) {
		track_indexer__g_get_tracks_wrap(p_path,p_reader,p_stats,type,callback, abort);
	}
}

void playlist_loader::g_process_path(const char * p_filename,playlist_loader_callback::ptr callback, abort_callback & abort,playlist_loader_callback::t_entry_type type)
{
	TRACK_CALL_TEXT("playlist_loader::g_process_path");

	auto filename = file_path_canonical(p_filename);

	process_path_internal(filename,0,callback,abort, type,filestats_invalid);
}

void playlist_loader::g_save_playlist(const char * p_filename,const pfc::list_base_const_t<metadb_handle_ptr> & data,abort_callback & p_abort)
{
	TRACK_CALL_TEXT("playlist_loader::g_save_playlist");
	pfc::string8 filename;
	filesystem::g_get_canonical_path(p_filename,filename);
	try {
		service_ptr_t<file> r;
		filesystem::g_open(r,filename,filesystem::open_mode_write_new,p_abort);

		auto ext = pfc::string_extension(filename);
		
		service_enum_t<playlist_loader> e;
		service_ptr_t<playlist_loader> l;
		if (e.first(l)) do {
			if (l->can_write() && !stricmp_utf8(ext,l->get_extension())) {
				try {
					TRACK_CODE("playlist_loader::write",l->write(filename,r,data,p_abort));
					return;
				} catch(exception_io_data) {}
			}
		} while(e.next(l));
		throw exception_io_data();
	} catch(...) {
		try {filesystem::g_remove(filename,p_abort);} catch(...) {}
		throw;
	}
}


bool playlist_loader::g_process_path_ex(const char * filename,playlist_loader_callback::ptr callback, abort_callback & abort,playlist_loader_callback::t_entry_type type)
{
	if (g_try_load_playlist(NULL, filename, callback, abort)) return true;
	//not a playlist format
	g_process_path(filename,callback,abort,type);
	return false;
}

#endif