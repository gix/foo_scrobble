#pragma once

// since fb2k 1.5
namespace fb2k {
	class panicHandler {
	public:
		virtual void onPanic() = 0;
	};
}

// since fb2k 1.5
extern "C" 
{
	void SHARED_EXPORT uAddPanicHandler(fb2k::panicHandler*);
	void SHARED_EXPORT uRemovePanicHandler(fb2k::panicHandler*);
}

extern "C"
{
	LPCSTR SHARED_EXPORT uGetCallStackPath();
	LONG SHARED_EXPORT uExceptFilterProc(LPEXCEPTION_POINTERS param);
	PFC_NORETURN void SHARED_EXPORT uBugCheck();

#ifdef _DEBUG
	void SHARED_EXPORT fb2kDebugSelfTest();
#endif
}

class uCallStackTracker
{
	t_size param;
public:
	explicit SHARED_EXPORT uCallStackTracker(const char * name);
	SHARED_EXPORT ~uCallStackTracker();
};



#if FB2K_SUPPORT_CRASH_LOGS
#define TRACK_CALL(X) uCallStackTracker TRACKER__##X(#X)
#define TRACK_CALL_TEXT(X) uCallStackTracker TRACKER__BLAH(X)
#define TRACK_CODE(description,code) {uCallStackTracker __call_tracker(description); code;}
#else
#define TRACK_CALL(X)
#define TRACK_CALL_TEXT(X)
#define TRACK_CODE(description,code) {code;}
#endif

#if FB2K_SUPPORT_CRASH_LOGS
static int uExceptFilterProc_inline(LPEXCEPTION_POINTERS param) {
	uDumpCrashInfo(param);
	TerminateProcess(GetCurrentProcess(), 0);
	return 0;// never reached
}
#endif


#define FB2K_DYNAMIC_ASSERT( X ) { if (!(X)) uBugCheck(); }

#define __except_instacrash __except(uExceptFilterProc(GetExceptionInformation()))
#if FB2K_SUPPORT_CRASH_LOGS
#define fb2k_instacrash_scope(X) __try { X; } __except_instacrash {}
#else
#define fb2k_instacrash_scope(X) {X;}
#endif

PFC_NORETURN static void fb2kCriticalError(DWORD code, DWORD argCount = 0, const ULONG_PTR * args = NULL) {
	fb2k_instacrash_scope( RaiseException(code,EXCEPTION_NONCONTINUABLE,argCount,args); );
}
PFC_NORETURN static void fb2kDeadlock() {
	fb2kCriticalError(0x63d81b66);
}

static void fb2kWaitForCompletion(HANDLE hEvent) {
	switch(WaitForSingleObject(hEvent, INFINITE)) {
	case WAIT_OBJECT_0:
		return;
	default:
		uBugCheck();
	}
}

static void fb2kWaitForThreadCompletion(HANDLE hWaitFor, DWORD threadID) {
	(void) threadID;
	switch(WaitForSingleObject(hWaitFor, INFINITE)) {
	case WAIT_OBJECT_0:
		return;
	default:
		uBugCheck();
	}
}

static void fb2kWaitForThreadCompletion2(HANDLE hWaitFor, HANDLE hThread, DWORD threadID) {
	(void) threadID;
	switch(WaitForSingleObject(hWaitFor, INFINITE)) {
	case WAIT_OBJECT_0:
		return;
	default:
		uBugCheck();
	}
}


static void __cdecl _OverrideCrtAbort_handler(int signal) {
	const ULONG_PTR args[] = {(ULONG_PTR)signal};
	RaiseException(0x6F8E1DC8 /* random GUID */, EXCEPTION_NONCONTINUABLE, _countof(args), args);
}

static void __cdecl _PureCallHandler() {
	RaiseException(0xf6538887 /* random GUID */, EXCEPTION_NONCONTINUABLE, 0, 0);
}

static void _InvalidParameter(
   const wchar_t * expression,
   const wchar_t * function, 
   const wchar_t * file, 
   unsigned int line,
   uintptr_t pReserved
) {
	(void)pReserved; (void) line; (void) file; (void) function; (void) expression;
	RaiseException(0xd142b808 /* random GUID */, EXCEPTION_NONCONTINUABLE, 0, 0);
}

static void OverrideCrtAbort() {
	const int signals[] = {SIGINT, SIGTERM, SIGBREAK, SIGABRT};
	for(size_t i=0; i<_countof(signals); i++) signal(signals[i], _OverrideCrtAbort_handler);
	_set_abort_behavior(0, UINT_MAX);

	_set_purecall_handler(_PureCallHandler);
	_set_invalid_parameter_handler(_InvalidParameter);
}
