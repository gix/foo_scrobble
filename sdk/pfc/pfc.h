#ifndef ___PFC_H___
#define ___PFC_H___

// Global flag - whether it's OK to leak static objects as they'll be released anyway by process death
#ifndef PFC_LEAK_STATIC_OBJECTS
#define PFC_LEAK_STATIC_OBJECTS 1
#endif


#ifdef __clang__
// Suppress a warning for a common practice in pfc/fb2k code
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif

#if !defined(_WINDOWS) && (defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64) || defined(_WIN32_WCE))
#define _WINDOWS
#endif


#ifdef _WINDOWS
#include "targetver.h"

#ifndef STRICT
#define STRICT
#endif

#ifndef _SYS_GUID_OPERATOR_EQ_
#define _NO_SYS_GUID_OPERATOR_EQ_	//fix retarded warning with operator== on GUID returning int
#endif

// WinSock2.h *before* Windows.h or else VS2017 15.3 breaks
#include <WinSock2.h>
#include <windows.h>

#if !defined(PFC_WINDOWS_STORE_APP) && !defined(PFC_WINDOWS_DESKTOP_APP)

#ifdef WINAPI_FAMILY_PARTITION
#if ! WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define PFC_WINDOWS_STORE_APP // Windows store or Windows phone app, not a desktop app
#endif // #if ! WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#endif // #ifdef WINAPI_FAMILY_PARTITION

#ifndef PFC_WINDOWS_STORE_APP
#define PFC_WINDOWS_DESKTOP_APP
#endif

#endif // #if !defined(PFC_WINDOWS_STORE_APP) && !defined(PFC_WINDOWS_DESKTOP_APP)

#ifndef _SYS_GUID_OPERATOR_EQ_
__inline bool __InlineIsEqualGUID(REFGUID rguid1, REFGUID rguid2)
{
   return (
      ((unsigned long *) &rguid1)[0] == ((unsigned long *) &rguid2)[0] &&
      ((unsigned long *) &rguid1)[1] == ((unsigned long *) &rguid2)[1] &&
      ((unsigned long *) &rguid1)[2] == ((unsigned long *) &rguid2)[2] &&
      ((unsigned long *) &rguid1)[3] == ((unsigned long *) &rguid2)[3]);
}

inline bool operator==(REFGUID guidOne, REFGUID guidOther) {return __InlineIsEqualGUID(guidOne,guidOther);}
inline bool operator!=(REFGUID guidOne, REFGUID guidOther) {return !__InlineIsEqualGUID(guidOne,guidOther);}
#endif

#include <tchar.h>

#else // not Windows

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h> // memcmp

typedef struct {
        uint32_t Data1;
        uint16_t Data2;
        uint16_t Data3;
        uint8_t  Data4[ 8 ];
    } GUID; //same as win32 GUID

inline bool operator==(const GUID & p_item1,const GUID & p_item2) {
	return memcmp(&p_item1,&p_item2,sizeof(GUID)) == 0;
}

inline bool operator!=(const GUID & p_item1,const GUID & p_item2) {
	return memcmp(&p_item1,&p_item2,sizeof(GUID)) != 0;
}

#endif // Windows vs not Windows



#define PFC_MEMORY_SPACE_LIMIT ((t_uint64)1<<(sizeof(void*)*8-1))

#define PFC_ALLOCA_LIMIT (4096)

#define INDEX_INVALID ((unsigned)(-1))


#include <exception>
#include <stdexcept>
#include <new>

#define _PFC_WIDESTRING(_String) L ## _String
#define PFC_WIDESTRING(_String) _PFC_WIDESTRING(_String)

#if defined(_DEBUG) || defined(DEBUG)
#define PFC_DEBUG 1
#else
#define PFC_DEBUG 0
#endif

#if ! PFC_DEBUG

#ifndef NDEBUG
#pragma message("WARNING: release build without NDEBUG")
#endif

#define PFC_ASSERT(_Expression)     ((void)0)
#define PFC_ASSERT_SUCCESS(_Expression) (void)( (_Expression), 0)
#define PFC_ASSERT_NO_EXCEPTION(_Expression) { _Expression; }
#else

#ifdef _WIN32
namespace pfc { void myassert_win32(const wchar_t * _Message, const wchar_t *_File, unsigned _Line); }
#define PFC_ASSERT(_Expression) (void)( (!!(_Expression)) || (pfc::myassert_win32(PFC_WIDESTRING(#_Expression), PFC_WIDESTRING(__FILE__), __LINE__), 0) )
#define PFC_ASSERT_SUCCESS(_Expression) PFC_ASSERT(_Expression)
#else
namespace pfc { void myassert (const char * _Message, const char *_File, unsigned _Line); }
#define PFC_ASSERT(_Expression) (void)( (!!(_Expression)) || (pfc::myassert(#_Expression, __FILE__, __LINE__), 0) )
#define PFC_ASSERT_SUCCESS(_Expression) PFC_ASSERT( _Expression )
#endif

#define PFC_ASSERT_NO_EXCEPTION(_Expression) { try { _Expression; } catch(...) { PFC_ASSERT(!"Should not get here - unexpected exception"); } }
#endif

#ifdef _MSC_VER

#if PFC_DEBUG
#define NOVTABLE
#else
#define NOVTABLE _declspec(novtable)
#endif

#if PFC_DEBUG
#define ASSUME(X) PFC_ASSERT(X)
#else
#define ASSUME(X) __assume(X)
#endif

#define PFC_DEPRECATE(X) // __declspec(deprecated(X))   don't do this since VS2015 defaults to erroring these
#define PFC_NORETURN __declspec(noreturn)
#define PFC_NOINLINE __declspec(noinline)
#else

#define NOVTABLE
#define ASSUME(X) PFC_ASSERT(X)
#define PFC_DEPRECATE(X)
#define PFC_NORETURN __attribute__ ((noreturn))
#define PFC_NOINLINE

#endif

namespace pfc {
	void selftest();
}

#include "int_types.h"
#include "traits.h"
#include "bit_array.h"
#include "primitives.h"
#include "alloc.h"
#include "array.h"
#include "bit_array_impl.h"
#include "binary_search.h"
#include "bsearch_inline.h"
#include "bsearch.h"
#include "sort.h"
#include "order_helper.h"
#include "list.h"
#include "ptr_list.h"
#include "string_base.h"
#include "string_list.h"
#include "lockless.h"
#include "ref_counter.h"
#include "iterators.h"
#include "avltree.h"
#include "map.h"
#include "bit_array_impl_part2.h"
#include "timers.h"
#include "guid.h"
#include "byte_order.h"
#include "other.h"
#include "chain_list_v2.h"
#include "rcptr.h"
#include "com_ptr_t.h"
#include "string_conv.h"
#include "stringNew.h"
#include "pathUtils.h"
#include "instance_tracker.h"
#include "threads.h"
#include "base64.h"
#include "primitives_part2.h"
#include "cpuid.h"
#include "memalign.h"

#ifdef _WIN32
#include "synchro_win.h"
#else
#include "synchro_nix.h"
#endif

#include "syncd_storage.h"

#ifdef _WIN32
#include "win-objects.h"
#else
#include "nix-objects.h"
#endif

#include "event.h"

#include "audio_sample.h"
#include "wildcard.h"
#include "filehandle.h"

#define PFC_INCLUDED 1

#ifndef PFC_SET_THREAD_DESCRIPTION
#define PFC_SET_THREAD_DESCRIPTION(X)
#endif

#endif //___PFC_H___
