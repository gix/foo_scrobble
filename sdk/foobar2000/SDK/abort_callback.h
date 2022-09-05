#pragma once

namespace foobar2000_io {

PFC_DECLARE_EXCEPTION(exception_aborted,pfc::exception,"User abort");

typedef pfc::eventHandle_t abort_callback_event;

#ifdef check
#undef check
#endif
//! This class is used to signal underlying worker code whether user has decided to abort a potentially time-consuming operation. \n
//! It is commonly required by all filesystem related or decoding-related operations. \n
//! Code that receives an abort_callback object should periodically check it and abort any operations being performed if it is signaled, typically throwing exception_aborted. \n
//! See abort_callback_impl for an implementation.
class NOVTABLE abort_callback
{
public:
	//! Returns whether user has requested the operation to be aborted.
	virtual bool is_aborting() const = 0;

	inline bool is_set() const {return is_aborting();}

	//! Retrieves event object that can be used with some OS calls. The even object becomes signaled when abort is triggered. On win32, this is equivalent to win32 event handle (see: CreateEvent). \n
	//! You must not close this handle or call any methods that change this handle's state (SetEvent() or ResetEvent()), you can only wait for it.
	virtual abort_callback_event get_abort_event() const = 0;

	inline abort_callback_event get_handle() const {return get_abort_event();}
	
	//! Checks if user has requested the operation to be aborted, and throws exception_aborted if so.
	void check() const;

	//! For compatibility with old code. Do not call.
	inline void check_e() const {check();}

	
	//! Sleeps p_timeout_seconds or less when aborted, throws exception_aborted on abort.
	void sleep(double p_timeout_seconds) const;
	//! Sleeps p_timeout_seconds or less when aborted, returns true when execution should continue, false when not.
	bool sleep_ex(double p_timeout_seconds) const;
    
	//! Waits for an event. Returns true if event is now signaled, false if the specified period has elapsed and the event did not become signaled. \n
	//! Throws exception_aborted if aborted.
    bool waitForEvent( pfc::eventHandle_t evtHandle, double timeOut );
	//! Waits for an event. Returns true if event is now signaled, false if the specified period has elapsed and the event did not become signaled. \n
	//! Throws exception_aborted if aborted.
	bool waitForEvent(pfc::event& evt, double timeOut);

	//! Waits for an event. Returns once the event became signaled; throw exception_aborted if abort occurred first.
	void waitForEvent(pfc::eventHandle_t evtHandle);
	//! Waits for an event. Returns once the event became signaled; throw exception_aborted if abort occurred first.
	void waitForEvent(pfc::event& evt);
protected:
	abort_callback() {}
	~abort_callback() {}
};



//! Implementation of abort_callback interface.
class abort_callback_impl : public abort_callback {
public:
	abort_callback_impl() : m_aborting(false) {}
	inline void abort() {set_state(true);}
	inline void set() {set_state(true);}
	inline void reset() {set_state(false);}

	void set_state(bool p_state) {m_aborting = p_state; m_event.set_state(p_state);}

	bool is_aborting() const {return m_aborting;}

	abort_callback_event get_abort_event() const {return m_event.get_handle();}

private:
	abort_callback_impl(const abort_callback_impl &) = delete;
	const abort_callback_impl & operator=(const abort_callback_impl&) = delete;
	
	volatile bool m_aborting;
	pfc::event m_event;
};

    
class abort_callback_usehandle : public abort_callback {
public:
    abort_callback_usehandle( abort_callback_event handle ) : m_handle(handle) {}
    
    bool is_aborting() const;
    abort_callback_event get_abort_event() const { return m_handle; }
private:
    const abort_callback_event m_handle;
};
    
//! Dummy abort_callback that never gets aborted. \n
//! Slightly more efficient than the regular one especially when you need to regularly create temporary instances of it.
class abort_callback_dummy : public abort_callback {
public:
	abort_callback_dummy() : m_event(GetInfiniteWaitEvent()) {}
	bool is_aborting() const { return false; }

	abort_callback_event get_abort_event() const { return m_event;}
private:
	const abort_callback_event m_event;
};

}
typedef foobar2000_io::abort_callback_event fb2k_event_handle;
typedef foobar2000_io::abort_callback fb2k_event;
typedef foobar2000_io::abort_callback_impl fb2k_event_impl;

using namespace foobar2000_io;

#define FB2K_PFCv2_ABORTER_SCOPE( abortObj ) \
    (abortObj).check(); \
    PP::waitableReadRef_t aborterRef = {(abortObj).get_abort_event()}; \
    PP::aborter aborter_pfcv2( aborterRef );    \
    PP::aborterScope l_aborterScope( aborter_pfcv2 );


namespace fb2k {
	// A shared abort_callback_dummy instance
	extern abort_callback_dummy noAbort;
}
