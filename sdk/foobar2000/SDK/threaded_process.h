#pragma once
#include <functional>

//! Callback class passed to your threaded_process client code; allows you to give various visual feedback to the user.
class threaded_process_status {
public:
	enum {progress_min = 0, progress_max = 5000};
	
	//! Sets the primary progress bar state; scale from progress_min to progress_max.
	virtual void set_progress(t_size p_state) {}
	//! Sets the secondary progress bar state; scale from progress_min to progress_max.
	virtual void set_progress_secondary(t_size p_state) {}
	//! Sets the currently progressed item label. When working with files, you should use set_file_path() instead.
	virtual void set_item(const char * p_item,t_size p_item_len = ~0) {}
	//! Sets the currently progressed item label; treats the label as a file path.
	virtual void set_item_path(const char * p_item,t_size p_item_len = ~0) {}
	//! Sets the title of the dialog. You normally don't need this function unless you want to override the title you set when initializing the threaded_process.
	virtual void set_title(const char * p_title,t_size p_title_len = ~0) {}
	//! Should not be used.
	virtual void force_update() {}
	//! Returns whether the process is paused.
	virtual bool is_paused() {return false;}
	
	//! Checks if process is paused and sleeps if needed; returns false when process should be aborted, true on success. \n
	//! You should use poll_pause() instead of calling this directly.
	virtual bool process_pause() {return true;}

	//! Automatically sleeps if the process is paused.
	void poll_pause() {if (!process_pause()) throw exception_aborted();}

	//! Helper; sets primary progress with a custom scale.
	void set_progress(t_size p_state,t_size p_max);
	//! Helper; sets secondary progress with a custom scale.
	void set_progress_secondary(t_size p_state,t_size p_max);
	//! Helper; sets primary progress with a float 0..1 scale.
	void set_progress_float(double p_state);
	//! Helper; sets secondary progress with a float 0..1 scale.
	void set_progress_secondary_float(double p_state);
	//! Helper; gracefully reports multiple items being concurrently worked on.
	void set_items( metadb_handle_list_cref items );
	void set_items( pfc::list_base_const_t<const char*> const & paths );
};

//! Fb2k mobile compatibility
class threaded_process_context {
public:
	static HWND g_default() { return core_api::get_main_window(); }
};

//! Callback class for the threaded_process API. You must implement this to create your own threaded_process client.
class NOVTABLE threaded_process_callback : public service_base {
public:
	typedef HWND ctx_t; // fb2k mobile compatibility

	//! Called from the main thread before spawning the worker thread. \n
	//! Note that you should not access the window handle passed to on_init() in the worker thread later on.
	virtual void on_init(ctx_t p_wnd) {}
	//! Called from the worker thread. Do all the hard work here.
	virtual void run(threaded_process_status & p_status,abort_callback & p_abort) = 0;
	//! Called after the worker thread has finished executing.
	virtual void on_done(ctx_t p_wnd,bool p_was_aborted) {}
	
	//! Safely prevent destruction from worker threads.
	static bool serviceRequiresMainThreadDestructor() { return true; }

	FB2K_MAKE_SERVICE_INTERFACE(threaded_process_callback,service_base);
};


//! The threaded_process API allows you to easily put timeconsuming tasks in worker threads, with progress dialog giving nice feedback to the user. \n
//! Thanks to this API you can perform such tasks with no user interface related programming at all.
class NOVTABLE threaded_process : public service_base {
public:
	enum {
		//! Shows the "abort" button.
		flag_show_abort			= 1,
		//! Obsolete, do not use.
		flag_show_minimize		= 1 << 1,
		//! Shows a progress bar.
		flag_show_progress		= 1 << 2,
		//! Shows dual progress bars; implies flag_show_progress.
		flag_show_progress_dual	= 1 << 3,
		//! Shows the item being currently processed.
		flag_show_item			= 1 << 4,
		//! Shows the "pause" button.
		flag_show_pause			= 1 << 5,
		//! Obsolete, do not use.
		flag_high_priority		= 1 << 6,
		//! Make the dialog hidden by default and show it only if the operation could not be completed after 500ms. Implies flag_no_focus. Relevant only to modeless dialogs.
		flag_show_delayed		= 1 << 7,
		//! Do not focus the dialog by default.
		flag_no_focus			= 1 << 8,
	};

	//! Runs a synchronous threaded_process operation - the function does not return until the operation has completed, though the app UI is not frozen and the operation is abortable. \n
	//! This API is obsolete and should not be used. Please use run_modeless() instead if possible.
	//! @param p_callback Interface to your threaded_process client.
	//! @param p_flags Flags describing requested dialog functionality. See threaded_process::flag_* constants.
	//! @param p_parent Parent window for the progress dialog - typically core_api::get_main_window().
	//! @param p_title Initial title of the dialog.
	//! @returns True if the operation has completed normally, false if the user has aborted the operation. In case of a catastrophic failure such as dialog creation failure, exceptions will be thrown.
	virtual bool run_modal(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len = ~0) = 0;
	//! Runs an asynchronous threaded_process operation.
	//! @param p_callback Interface to your threaded_process client.
	//! @param p_flags Flags describing requested dialog functionality. See threaded_process::flag_* constants.
	//! @param p_parent Parent window for the progress dialog - typically core_api::get_main_window().
	//! @param p_title Initial title of the dialog.
	//! @returns True, always; the return value should be ignored. In case of a catastrophic failure such as dialog creation failure, exceptions will be thrown.
	virtual bool run_modeless(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len = ~0) = 0;


	//! Helper invoking run_modal().
	static bool g_run_modal(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len = ~0);
	//! Helper invoking run_modeless().
	static bool g_run_modeless(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len = ~0);

	//! Queries user settings; returns whether various timeconsuming tasks should be blocking machine standby.
	static bool g_query_preventStandby();

	FB2K_MAKE_SERVICE_COREAPI(threaded_process);
};


//! Helper - forward threaded_process_callback calls to a service object that for whatever reason cannot publish threaded_process_callback API by itself.
template<typename TTarget> class threaded_process_callback_redir : public threaded_process_callback {
public:
	threaded_process_callback_redir(TTarget * target) : m_target(target) {}
	void on_init(HWND p_wnd) {m_target->tpc_on_init(p_wnd);}
	void run(threaded_process_status & p_status,abort_callback & p_abort) {m_target->tpc_run(p_status, p_abort);}
	void on_done(HWND p_wnd,bool p_was_aborted) {m_target->tpc_on_done(p_wnd, p_was_aborted);	}
private:
	const service_ptr_t<TTarget> m_target;
};

//! Helper - lambda based threaded_process_callback implementation
class threaded_process_callback_lambda : public threaded_process_callback {
public:
	typedef std::function<void(ctx_t)> on_init_t;
	typedef std::function<void(threaded_process_status&, abort_callback&)> run_t;
	typedef std::function<void(ctx_t, bool)> on_done_t;

	static service_ptr_t<threaded_process_callback_lambda> create();
	static service_ptr_t<threaded_process_callback_lambda> create(run_t f);
	static service_ptr_t<threaded_process_callback_lambda> create(on_init_t f1, run_t f2, on_done_t f3);

	on_init_t m_on_init;
	run_t m_run;
	on_done_t m_on_done;

	void on_init(ctx_t p_ctx);
	void run(threaded_process_status & p_status, abort_callback & p_abort);
	void on_done(ctx_t p_ctx, bool p_was_aborted);
};
