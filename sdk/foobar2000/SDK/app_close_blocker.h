#pragma once

#include <functional>

//! (DEPRECATED) This service is used to signal whether something is currently preventing main window from being closed and app from being shut down.
class NOVTABLE app_close_blocker : public service_base
{
public:
	//! Checks whether this service is currently preventing main window from being closed and app from being shut down.
	virtual bool query() = 0;

	//! Static helper function, checks whether any of registered app_close_blocker services is currently preventing main window from being closed and app from being shut down.
	static bool g_query();

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(app_close_blocker);
};

//! An interface encapsulating a task preventing the foobar2000 application from being closed. Instances of this class need to be registered using app_close_blocking_task_manager methods. \n
//! Implementation: it's recommended that you derive from app_close_blocking_task_impl class instead of deriving from app_close_blocking_task directly, it manages registration/unregistration behind-the-scenes.
class NOVTABLE app_close_blocking_task {
public:
	virtual void query_task_name(pfc::string_base & out) = 0;

protected:
	app_close_blocking_task() {}
	~app_close_blocking_task() {}

	PFC_CLASS_NOT_COPYABLE_EX(app_close_blocking_task);
};

//! Entrypoint class for registering app_close_blocking_task instances. Introduced in 0.9.5.1. \n
//! Usage: static_api_ptr_t<app_close_blocking_task_manager>(). May fail if user runs pre-0.9.5.1. It's recommended that you use app_close_blocking_task_impl class instead of calling app_close_blocking_task_manager directly.
class NOVTABLE app_close_blocking_task_manager : public service_base {
	FB2K_MAKE_SERVICE_COREAPI(app_close_blocking_task_manager);
public:
	virtual void register_task(app_close_blocking_task * task) = 0;
	virtual void unregister_task(app_close_blocking_task * task) = 0;
};

//! Helper; implements standard functionality required by app_close_blocking_task implementations - registers/unregisters the task on construction/destruction.
class app_close_blocking_task_impl : public app_close_blocking_task {
public:
	app_close_blocking_task_impl() { app_close_blocking_task_manager::get()->register_task(this);}
	~app_close_blocking_task_impl() { app_close_blocking_task_manager::get()->unregister_task(this);}

	void query_task_name(pfc::string_base & out) { out = "<unnamed task>"; }
};

class app_close_blocking_task_impl_dynamic : public app_close_blocking_task {
public:
	app_close_blocking_task_impl_dynamic() : m_taskActive() {}
	~app_close_blocking_task_impl_dynamic() { toggle_blocking(false); }

	void query_task_name(pfc::string_base & out) { out = "<unnamed task>"; }
	
protected:
	void toggle_blocking(bool state) {
		if (state != m_taskActive) {
			auto api = app_close_blocking_task_manager::get();
			if (state) api->register_task(this);
			else api->unregister_task(this);
			m_taskActive = state;
		}
	}
private:
	bool m_taskActive;
};


//! \since 1.4.5
//! Provides means for async tasks - running detached from any UI - to reliably finish before the app terminates. \n
//! During a late phase of app shutdown, past initquit ops, when no worker code should be still running - a core-managed instance of abort_callback is signaled, \n
//! then main thread stalls until all objects created with acquire() have been released. \n
//! As this API was introduced out-of-band with 1.4.5, you should use tryGet() to obtain it with FOOBAR2000_TARGET_VERSION < 80, but can safely use ::get() with FOOBAR2000_TARGET_VERSION >= 80.
class async_task_manager : public service_base {
	FB2K_MAKE_SERVICE_COREAPI( async_task_manager );
public:
	virtual abort_callback & get_aborter() = 0;
	virtual service_ptr acquire() = 0;

	//! acquire() helper; returns nullptr if the API isn't available due to old fb2k
	static service_ptr g_acquire();
};

namespace fb2k {
	//! pfc::splitThread() + async_task_manager::acquire
	void splitTask( std::function<void ()> );
}
