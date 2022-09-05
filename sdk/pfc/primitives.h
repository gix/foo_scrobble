#pragma once

#include <functional>

#include "traits.h"
#include "bit_array.h"

#define tabsize(x) ((size_t)(sizeof(x)/sizeof(*x)))
#define PFC_TABSIZE(x) ((size_t)(sizeof(x)/sizeof(*x)))

// Retained for compatibility. Do not use. Use C++11 template<typename ... arg_t> instead.
#define TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD_WITH_INITIALIZER(THISCLASS,MEMBER,INITIALIZER)	\
																																				THISCLASS() :																																														MEMBER() INITIALIZER	\
	template<typename t_param1>																													THISCLASS(const t_param1 & p_param1) :																																								MEMBER(p_param1) INITIALIZER	\
	template<typename t_param1,typename t_param2>																								THISCLASS(const t_param1 & p_param1,const t_param2 & p_param2) :																																	MEMBER(p_param1,p_param2) INITIALIZER	\
	template<typename t_param1,typename t_param2,typename t_param3>																				THISCLASS(const t_param1 & p_param1,const t_param2 & p_param2,const t_param3 & p_param3) :																											MEMBER(p_param1,p_param2,p_param3) INITIALIZER	\
	template<typename t_param1,typename t_param2,typename t_param3,typename t_param4>															THISCLASS(const t_param1 & p_param1,const t_param2 & p_param2,const t_param3 & p_param3,const t_param4 & p_param4) :																				MEMBER(p_param1,p_param2,p_param3,p_param4) INITIALIZER	\
	template<typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5>											THISCLASS(const t_param1 & p_param1,const t_param2 & p_param2,const t_param3 & p_param3,const t_param4 & p_param4,const t_param5 & p_param5) :														MEMBER(p_param1,p_param2,p_param3,p_param4,p_param5) INITIALIZER	\
	template<typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5,typename t_param6>						THISCLASS(const t_param1 & p_param1,const t_param2 & p_param2,const t_param3 & p_param3,const t_param4 & p_param4,const t_param5 & p_param5,const t_param6 & p_param6) :							MEMBER(p_param1,p_param2,p_param3,p_param4,p_param5,p_param6) INITIALIZER	\
	template<typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5,typename t_param6, typename t_param7>	THISCLASS(const t_param1 & p_param1,const t_param2 & p_param2,const t_param3 & p_param3,const t_param4 & p_param4,const t_param5 & p_param5,const t_param6 & p_param6,const t_param7 & p_param7) :	MEMBER(p_param1,p_param2,p_param3,p_param4,p_param5,p_param6,p_param7) INITIALIZER	\
	template<typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5,typename t_param6, typename t_param7, typename t_param8>	THISCLASS(const t_param1 & p_param1,const t_param2 & p_param2,const t_param3 & p_param3,const t_param4 & p_param4,const t_param5 & p_param5,const t_param6 & p_param6,const t_param7 & p_param7, const t_param8 & p_param8) :	MEMBER(p_param1,p_param2,p_param3,p_param4,p_param5,p_param6,p_param7, p_param8) INITIALIZER

#define TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD(THISCLASS,MEMBER) TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD_WITH_INITIALIZER(THISCLASS,MEMBER,{})


#ifdef _WIN32

#ifndef _MSC_VER
#error MSVC expected
#endif

// MSVC specific - part of fb2k ABI - cannot ever change on MSVC/Windows

#define PFC_DECLARE_EXCEPTION(NAME,BASECLASS,DEFAULTMSG)	\
class NAME : public BASECLASS {	\
public:	\
	static const char * g_what() {return DEFAULTMSG;}	\
	NAME() : BASECLASS(DEFAULTMSG,0) {}	\
	NAME(const char * p_msg) : BASECLASS(p_msg) {}	\
	NAME(const char * p_msg,int) : BASECLASS(p_msg,0) {}	\
	NAME(const NAME & p_source) : BASECLASS(p_source) {}	\
};

namespace pfc {
	template<typename t_exception> PFC_NORETURN inline void throw_exception_with_message(const char * p_message) {
		throw t_exception(p_message);
	}
}

#else

#define PFC_DECLARE_EXCEPTION(NAME,BASECLASS,DEFAULTMSG)	\
class NAME : public BASECLASS { \
public:	\
	static const char * g_what() {return DEFAULTMSG;}	\
	const char* what() const throw() {return DEFAULTMSG;}	\
};

namespace pfc {
	template<typename t_base> class __exception_with_message_t : public t_base {
	private:	typedef __exception_with_message_t<t_base> t_self;
	public:
		__exception_with_message_t(const char * p_message) : m_message(NULL) {
			set_message(p_message);			
		}
		__exception_with_message_t() : m_message(NULL) {}
		__exception_with_message_t(const t_self & p_source) : m_message(NULL) {set_message(p_source.m_message);}

		const char* what() const throw() {return m_message != NULL ? m_message : "unnamed exception";}

		const t_self & operator=(const t_self & p_source) {set_message(p_source.m_message);}

		~__exception_with_message_t() throw() {cleanup();}

	private:
		void set_message(const char * p_message) throw() {
			cleanup();
			if (p_message != NULL) m_message = strdup(p_message);
		}
		void cleanup() throw() {
			if (m_message != NULL) {free(m_message); m_message = NULL;}
		}
		char * m_message;
	};
	template<typename t_exception> PFC_NORETURN void throw_exception_with_message(const char * p_message) {
		throw __exception_with_message_t<t_exception>(p_message);
	}
}
#endif

namespace pfc {

	template<typename p_type1,typename p_type2> class assert_same_type;
	template<typename p_type> class assert_same_type<p_type,p_type> {};

	template<typename p_type1,typename p_type2>
	class is_same_type { public: enum {value = false}; };
	template<typename p_type>
	class is_same_type<p_type,p_type> { public: enum {value = true}; };

	template<bool val> class static_assert_t;
	template<> class static_assert_t<true> {};

#define PFC_STATIC_ASSERT(X) { ::pfc::static_assert_t<(X)>(); }

	template<typename t_type>
	void assert_raw_type() {static_assert_t< !traits_t<t_type>::needs_constructor && !traits_t<t_type>::needs_destructor >();}

	template<typename t_type> class assert_byte_type;
	template<> class assert_byte_type<char> {};
	template<> class assert_byte_type<unsigned char> {};
	template<> class assert_byte_type<signed char> {};


	template<typename t_type> void __unsafe__memcpy_t(t_type * p_dst,const t_type * p_src,t_size p_count) {
		::memcpy(reinterpret_cast<void*>(p_dst), reinterpret_cast<const void*>(p_src), p_count * sizeof(t_type));
	}

	template<typename t_type> void __unsafe__in_place_destructor_t(t_type & p_item) throw() {
		if (traits_t<t_type>::needs_destructor) try{ p_item.~t_type(); } catch(...) {}
	}
	
	template<typename t_type> void __unsafe__in_place_constructor_t(t_type & p_item) {
		if (traits_t<t_type>::needs_constructor) {
			t_type * ret = new(&p_item) t_type;
			PFC_ASSERT(ret == &p_item);
            (void) ret; // suppress warning
		}
	}

	template<typename t_type> void __unsafe__in_place_destructor_array_t(t_type * p_items, t_size p_count) throw() {
		if (traits_t<t_type>::needs_destructor) {
			t_type * walk = p_items;
			for(t_size n=p_count;n;--n) __unsafe__in_place_destructor_t(*(walk++));
		}
	}
	
	template<typename t_type> t_type * __unsafe__in_place_constructor_array_t(t_type * p_items,t_size p_count) {
		if (traits_t<t_type>::needs_constructor) {
			t_size walkptr = 0;
			try {
				for(walkptr=0;walkptr<p_count;++walkptr) __unsafe__in_place_constructor_t(p_items[walkptr]);
			} catch(...) {
				__unsafe__in_place_destructor_array_t(p_items,walkptr);
				throw;
			}
		}
		return p_items;
	}

	template<typename t_type> t_type * __unsafe__in_place_resize_array_t(t_type * p_items,t_size p_from,t_size p_to) {
		if (p_from < p_to) __unsafe__in_place_constructor_array_t(p_items + p_from, p_to - p_from);
		else if (p_from > p_to) __unsafe__in_place_destructor_array_t(p_items + p_to, p_from - p_to);
		return p_items;
	}

	template<typename t_type,typename t_copy> void __unsafe__in_place_constructor_copy_t(t_type & p_item,const t_copy & p_copyfrom) {
		if (traits_t<t_type>::needs_constructor) {
			t_type * ret = new(&p_item) t_type(p_copyfrom);
			PFC_ASSERT(ret == &p_item);
            (void) ret; // suppress warning
		} else {
			p_item = p_copyfrom;
		}
	}

	template<typename t_type,typename t_copy> t_type * __unsafe__in_place_constructor_array_copy_t(t_type * p_items,t_size p_count, const t_copy * p_copyfrom) {
		t_size walkptr = 0;
		try {
			for(walkptr=0;walkptr<p_count;++walkptr) __unsafe__in_place_constructor_copy_t(p_items[walkptr],p_copyfrom[walkptr]);
		} catch(...) {
			__unsafe__in_place_destructor_array_t(p_items,walkptr);
			throw;
		}
		return p_items;
	}

	template<typename t_type,typename t_copy> t_type * __unsafe__in_place_constructor_array_copy_partial_t(t_type * p_items,t_size p_count, const t_copy * p_copyfrom,t_size p_copyfrom_count) {
		if (p_copyfrom_count > p_count) p_copyfrom_count = p_count;
		__unsafe__in_place_constructor_array_copy_t(p_items,p_copyfrom_count,p_copyfrom);
		try {
			__unsafe__in_place_constructor_array_t(p_items + p_copyfrom_count,p_count - p_copyfrom_count);
		} catch(...) {
			__unsafe__in_place_destructor_array_t(p_items,p_copyfrom_count);
			throw;
		}
		return p_items;
	}

	template<typename t_ret> t_ret implicit_cast(t_ret val) {return val;}

	template<typename t_ret,typename t_param>
	t_ret * safe_ptr_cast(t_param * p_param) {
		if (pfc::is_same_type<t_ret,t_param>::value) return p_param;
		else {
			if (p_param == NULL) return NULL;
			else return p_param;
		}
	}

	typedef std::exception exception;

	PFC_DECLARE_EXCEPTION(exception_overflow,exception,"Overflow");
	PFC_DECLARE_EXCEPTION(exception_bug_check,exception,"Bug check");
	PFC_DECLARE_EXCEPTION(exception_invalid_params,exception_bug_check,"Invalid parameters");
	PFC_DECLARE_EXCEPTION(exception_unexpected_recursion,exception_bug_check,"Unexpected recursion");
	PFC_DECLARE_EXCEPTION(exception_not_implemented,exception_bug_check,"Feature not implemented");
	PFC_DECLARE_EXCEPTION(exception_dynamic_assert,exception_bug_check,"dynamic_assert failure");

	template<typename t_ret,typename t_param>
	t_ret downcast_guarded(const t_param & p_param) {
		t_ret temp = (t_ret) p_param;
		if ((t_param) temp != p_param) throw exception_overflow();
		return temp;
	}
	
	template<typename t_exception,typename t_ret,typename t_param>
	t_ret downcast_guarded_ex(const t_param & p_param) {
		t_ret temp = (t_ret) p_param;
		if ((t_param) temp != p_param) throw t_exception();
		return temp;
	}

	template<typename t_acc,typename t_add>
	void accumulate_guarded(t_acc & p_acc, const t_add & p_add) {
		t_acc delta = downcast_guarded<t_acc>(p_add);
		delta += p_acc;
		if (delta < p_acc) throw exception_overflow();
		p_acc = delta;
	}

	//deprecated
	inline void bug_check_assert(bool p_condition, const char * p_msg) {
		if (!p_condition) {
			PFC_ASSERT(0);
			throw_exception_with_message<exception_bug_check>(p_msg);
		}
	}
	//deprecated
	inline void bug_check_assert(bool p_condition) {
		if (!p_condition) {
			PFC_ASSERT(0);
			throw exception_bug_check();
		}
	}

	inline void dynamic_assert(bool p_condition, const char * p_msg) {
		if (!p_condition) {
			PFC_ASSERT(0);
			throw_exception_with_message<exception_dynamic_assert>(p_msg);
		}
	}
	inline void dynamic_assert(bool p_condition) {
		if (!p_condition) {
			PFC_ASSERT(0);
			throw exception_dynamic_assert();
		}
	}

	template<typename T>
	inline void swap_multi_t(T * p_buffer1,T * p_buffer2,t_size p_size) {
		T * walk1 = p_buffer1, * walk2 = p_buffer2;
		for(t_size n=p_size;n;--n) {
			T temp (* walk1);
			*walk1 = *walk2;
			*walk2 = temp;
			walk1++; walk2++;
		}
	}

	template<typename T,t_size p_size>
	inline void swap_multi_t(T * p_buffer1,T * p_buffer2) {
		T * walk1 = p_buffer1, * walk2 = p_buffer2;
		for(t_size n=p_size;n;--n) {
			T temp (* walk1);
			*walk1 = *walk2;
			*walk2 = temp;
			walk1++; walk2++;
		}
	}


	template<t_size p_size>
	inline void __unsafe__swap_raw_t(void * p_object1, void * p_object2) {
		if (p_size % sizeof(t_size) == 0) {
			swap_multi_t<t_size,p_size/sizeof(t_size)>(reinterpret_cast<t_size*>(p_object1),reinterpret_cast<t_size*>(p_object2));
		} else {
			swap_multi_t<t_uint8,p_size>(reinterpret_cast<t_uint8*>(p_object1),reinterpret_cast<t_uint8*>(p_object2));
		}
	}

	template<typename T>
	inline void swap_t(T & p_item1, T & p_item2) {
		if (traits_t<T>::realloc_safe) {
			__unsafe__swap_raw_t<sizeof(T)>( reinterpret_cast<void*>( &p_item1 ), reinterpret_cast<void*>( &p_item2 ) );
		} else {
			T temp( std::move(p_item2) );
			p_item2 = std::move(p_item1);
			p_item1 = std::move(temp);
		}
	}

	//! This is similar to plain p_item1 = p_item2; assignment, but optimized for the case where p_item2 content is no longer needed later on. This can be overridden for specific classes for optimal performance. \n
	//! p_item2 value is undefined after performing a move_t. For an example, in certain cases move_t will fall back to swap_t.
	template<typename T>
	inline void move_t(T & p_item1, T & p_item2) {
		p_item1 = std::move(p_item2);
	}

	template<typename t_array>
	t_size array_size_t(const t_array & p_array) {return p_array.get_size();}

	template<typename t_item, t_size p_width>
	t_size array_size_t(const t_item (&p_array)[p_width]) {return p_width;}

	template<typename t_array, typename t_item> static bool array_isLast(const t_array & arr, const t_item & item) {
		const t_size size = pfc::array_size_t(arr);
		return size > 0 && arr[size-1] == item;
	}
	template<typename t_array, typename t_item> static bool array_isFirst(const t_array & arr, const t_item & item) {
		const t_size size = pfc::array_size_t(arr);
		return size > 0 && arr[0] == item;
	}

	template<typename t_array,typename t_filler>
	inline void fill_t(t_array & p_buffer,const t_size p_count, const t_filler & p_filler) {
		for(t_size n=0;n<p_count;n++)
			p_buffer[n] = p_filler;
	}

	template<typename t_array,typename t_filler>
	inline void fill_ptr_t(t_array * p_buffer,const t_size p_count, const t_filler & p_filler) {
		for(t_size n=0;n<p_count;n++)
			p_buffer[n] = p_filler;
	}

	template<typename t_item1, typename t_item2>
	inline int compare_t(const t_item1 & p_item1, const t_item2 & p_item2) {
		if (p_item1 < p_item2) return -1;
		else if (p_item1 > p_item2) return 1;
		else return 0;
	}

	//! For use with avltree/map etc.
	class comparator_default {
	public:
		template<typename t_item1,typename t_item2>
		inline static int compare(const t_item1 & p_item1,const t_item2 & p_item2) {return pfc::compare_t(p_item1,p_item2);}
	};

	template<typename t_comparator = pfc::comparator_default> class comparator_pointer { public:
		template<typename t_item1,typename t_item2> static int compare(const t_item1 & p_item1,const t_item2 & p_item2) {return t_comparator::compare(*p_item1,*p_item2);}
	};

	template<typename t_primary,typename t_secondary> class comparator_dual { public:
		template<typename t_item1,typename t_item2> static int compare(const t_item1 & p_item1,const t_item2 & p_item2) {
			int state = t_primary::compare(p_item1,p_item2);
			if (state != 0) return state;
			return t_secondary::compare(p_item1,p_item2);
		}
	};

	class comparator_memcmp {
	public:
		template<typename t_item1,typename t_item2>
		inline static int compare(const t_item1 & p_item1,const t_item2 & p_item2) {
			static_assert_t<sizeof(t_item1) == sizeof(t_item2)>();
			return memcmp(&p_item1,&p_item2,sizeof(t_item1));
		}
	};

	template<typename t_source1, typename t_source2>
	t_size subtract_sorted_lists_calculate_count(const t_source1 & p_source1, const t_source2 & p_source2) {
		t_size walk1 = 0, walk2 = 0, walk_out = 0;
		const t_size max1 = p_source1.get_size(), max2 = p_source2.get_size();
		for(;;) {
			int state;
			if (walk1 < max1 && walk2 < max2) {
				state = pfc::compare_t(p_source1[walk1],p_source2[walk2]);
			} else if (walk1 < max1) {
				state = -1;
			} else if (walk2 < max2) {
				state = 1;
			} else {
				break;
			}
			if (state < 0) walk_out++;
			if (state <= 0) walk1++;
			if (state >= 0) walk2++;
		}
		return walk_out;
	}

	//! Subtracts p_source2 contents from p_source1 and stores result in p_destination. Both source lists must be sorted.
	//! Note: duplicates will be carried over (and ignored for p_source2).
	template<typename t_destination, typename t_source1, typename t_source2>
	void subtract_sorted_lists(t_destination & p_destination,const t_source1 & p_source1, const t_source2 & p_source2) {
		p_destination.set_size(subtract_sorted_lists_calculate_count(p_source1,p_source2));
		t_size walk1 = 0, walk2 = 0, walk_out = 0;
		const t_size max1 = p_source1.get_size(), max2 = p_source2.get_size();
		for(;;) {
			int state;
			if (walk1 < max1 && walk2 < max2) {
				state = pfc::compare_t(p_source1[walk1],p_source2[walk2]);
			} else if (walk1 < max1) {
				state = -1;
			} else if (walk2 < max2) {
				state = 1;
			} else {
				break;
			}
			
			
			if (state < 0) p_destination[walk_out++] = p_source1[walk1];
			if (state <= 0) walk1++;
			if (state >= 0) walk2++;
		}
	}

	template<typename t_source1, typename t_source2>
	t_size merge_sorted_lists_calculate_count(const t_source1 & p_source1, const t_source2 & p_source2) {
		t_size walk1 = 0, walk2 = 0, walk_out = 0;
		const t_size max1 = p_source1.get_size(), max2 = p_source2.get_size();
		for(;;) {
			int state;
			if (walk1 < max1 && walk2 < max2) {
				state = pfc::compare_t(p_source1[walk1],p_source2[walk2]);
			} else if (walk1 < max1) {
				state = -1;
			} else if (walk2 < max2) {
				state = 1;
			} else {
				break;
			}
			if (state <= 0) walk1++;
			if (state >= 0) walk2++;
			walk_out++;
		}
		return walk_out;
	}

	//! Merges p_source1 and p_source2, storing content in p_destination. Both source lists must be sorted.
	//! Note: duplicates will be carried over.
	template<typename t_destination, typename t_source1, typename t_source2>
	void merge_sorted_lists(t_destination & p_destination,const t_source1 & p_source1, const t_source2 & p_source2) {
		p_destination.set_size(merge_sorted_lists_calculate_count(p_source1,p_source2));
		t_size walk1 = 0, walk2 = 0, walk_out = 0;
		const t_size max1 = p_source1.get_size(), max2 = p_source2.get_size();
		for(;;) {
			int state;
			if (walk1 < max1 && walk2 < max2) {
				state = pfc::compare_t(p_source1[walk1],p_source2[walk2]);
			} else if (walk1 < max1) {
				state = -1;
			} else if (walk2 < max2) {
				state = 1;
			} else {
				break;
			}
			if (state < 0) {
				p_destination[walk_out] = p_source1[walk1++];
			} else if (state > 0) {
				p_destination[walk_out] = p_source2[walk2++];
			} else {
				p_destination[walk_out] = p_source1[walk1];
				walk1++; walk2++;
			}
			walk_out++;
		}
	}

	

	template<typename t_array,typename T>
	inline t_size append_t(t_array & p_array,const T & p_item)
	{
		t_size old_count = p_array.get_size();
		p_array.set_size(old_count + 1);
		p_array[old_count] = p_item;
		return old_count;
	}
	template<typename t_array, typename T>
	inline t_size append_t(t_array & p_array, T && p_item)
	{
		t_size old_count = p_array.get_size();
		p_array.set_size(old_count + 1);
		p_array[old_count] = std::move(p_item);
		return old_count;
	}

	template<typename t_array,typename T>
	inline t_size append_swap_t(t_array & p_array,T & p_item)
	{
		t_size old_count = p_array.get_size();
		p_array.set_size(old_count + 1);
		swap_t(p_array[old_count],p_item);
		return old_count;
	}

	template<typename t_array>
	inline t_size insert_uninitialized_t(t_array & p_array,t_size p_index) {
		t_size old_count = p_array.get_size();
		if (p_index > old_count) p_index = old_count;
		p_array.set_size(old_count + 1);
		for(t_size n=old_count;n>p_index;n--) move_t(p_array[n], p_array[n-1]);
		return p_index;
	}

	template<typename t_array,typename T>
	inline t_size insert_t(t_array & p_array,const T & p_item,t_size p_index) {
		t_size old_count = p_array.get_size();
		if (p_index > old_count) p_index = old_count;
		p_array.set_size(old_count + 1);
		for(t_size n=old_count;n>p_index;n--)
			move_t(p_array[n], p_array[n-1]);
		p_array[p_index] = p_item;
		return p_index;
	}
	template<typename array1_t, typename array2_t>
	void insert_array_t( array1_t & outArray, size_t insertAt, array2_t const & inArray, size_t inArraySize) {
		const size_t oldSize = outArray.get_size();
		if (insertAt > oldSize) insertAt = oldSize;
		const size_t newSize = oldSize + inArraySize;
		outArray.set_size( newSize );
		for(size_t m = oldSize; m != insertAt; --m) {
			move_t( outArray[ m - 1 + inArraySize], outArray[m - 1] );
		}
		for(size_t w = 0; w < inArraySize; ++w) {
			outArray[ insertAt + w ]  = inArray[ w ];
		}
	}

	template<typename t_array,typename in_array_t>
	inline t_size insert_multi_t(t_array & p_array,const in_array_t & p_items, size_t p_itemCount, t_size p_index) {
		const t_size old_count = p_array.get_size();
		const size_t new_count = old_count + p_itemCount;
		if (p_index > old_count) p_index = old_count;
		p_array.set_size(new_count);
		size_t toMove = old_count - p_index;
		for(size_t w = 0; w < toMove; ++w) {
			move_t( p_array[new_count - 1 - w], p_array[old_count - 1 - w] );
		}
		
		for(size_t w = 0; w < p_itemCount; ++w) {
			p_array[p_index+w] = p_items[w];
		}

		return p_index;
	}
	template<typename t_array,typename T>
	inline t_size insert_swap_t(t_array & p_array,T & p_item,t_size p_index) {
		t_size old_count = p_array.get_size();
		if (p_index > old_count) p_index = old_count;
		p_array.set_size(old_count + 1);
		for(t_size n=old_count;n>p_index;n--)
			swap_t(p_array[n],p_array[n-1]);
		swap_t(p_array[p_index],p_item);
		return p_index;
	}

	
	template<typename T>
	inline T max_t(const T & item1, const T & item2) {return item1 > item2 ? item1 : item2;};

	template<typename T>
	inline T min_t(const T & item1, const T & item2) {return item1 < item2 ? item1 : item2;};

	template<typename T>
	inline T abs_t(T item) {return item<0 ? -item : item;}

	template<typename T>
	inline T sqr_t(T item) {return item * item;}

	template<typename T>
	inline T clip_t(const T & p_item, const T & p_min, const T & p_max) {
		if (p_item < p_min) return p_min;
		else if (p_item <= p_max) return p_item;
		else return p_max;
	}





	template<typename T>
	inline void delete_t(T* ptr) {delete ptr;}

	template<typename T>
	inline void delete_array_t(T* ptr) {delete[] ptr;}

	template<typename T>
	inline T* clone_t(T* ptr) {return new T(*ptr);}


	template<typename t_exception,typename t_int>
	inline t_int mul_safe_t(t_int p_val1,t_int p_val2) {
		if (p_val1 == 0 || p_val2 == 0) return 0;
		t_int temp = (t_int) (p_val1 * p_val2);
		if (temp / p_val1 != p_val2) throw t_exception();
		return temp;
	}
	template<typename t_int>
	t_int multiply_guarded(t_int v1, t_int v2) {
		return mul_safe_t<exception_overflow>(v1, v2);
	}
	template<typename t_int> t_int add_unsigned_clipped(t_int v1, t_int v2) {
		t_int v = v1 + v2;
		if (v < v1) return ~0;
		return v;
	}
	template<typename t_int> t_int sub_unsigned_clipped(t_int v1, t_int v2) {
		t_int v = v1 - v2;
		if (v > v1) return 0;
		return v;
	}
	template<typename t_int> void acc_unsigned_clipped(t_int & v1, t_int v2) {
		v1 = add_unsigned_clipped(v1, v2);
	}

	template<typename t_src,typename t_dst>
	void memcpy_t(t_dst* p_dst,const t_src* p_src,t_size p_count) {
		for(t_size n=0;n<p_count;n++) p_dst[n] = p_src[n];
	}

	template<typename t_dst,typename t_src>
	void copy_array_loop_t(t_dst & p_dst,const t_src & p_src,t_size p_count) {
		for(t_size n=0;n<p_count;n++) p_dst[n] = p_src[n];
	}

	template<typename t_src,typename t_dst>
	void memcpy_backwards_t(t_dst * p_dst,const t_src * p_src,t_size p_count) {
		p_dst += p_count; p_src += p_count;
		for(t_size n=0;n<p_count;n++) *(--p_dst) = *(--p_src);
	}

	template<typename T,typename t_val>
	void memset_t(T * p_buffer,const t_val & p_val,t_size p_count) {
		for(t_size n=0;n<p_count;n++) p_buffer[n] = p_val;
	}

	template<typename T,typename t_val>
	void memset_t(T &p_buffer,const t_val & p_val) {
		const t_size width = pfc::array_size_t(p_buffer);
		for(t_size n=0;n<width;n++) p_buffer[n] = p_val;
	}

	template<typename T>
	void memset_null_t(T * p_buffer,t_size p_count) {
		for(t_size n=0;n<p_count;n++) p_buffer[n] = 0;
	}

	template<typename T>
	void memset_null_t(T &p_buffer) {
		const t_size width = pfc::array_size_t(p_buffer);
		for(t_size n=0;n<width;n++) p_buffer[n] = 0;
	}

	template<typename T>
	void memmove_t(T* p_dst,const T* p_src,t_size p_count) {
		if (p_dst == p_src) {/*do nothing*/}
		else if (p_dst > p_src && p_dst < p_src + p_count) memcpy_backwards_t<T>(p_dst,p_src,p_count);
		else memcpy_t<T>(p_dst,p_src,p_count);
	}

	template<typename TVal> void memxor_t(TVal * out, const TVal * s1, const TVal * s2, t_size count) {
		for(t_size walk = 0; walk < count; ++walk) out[walk] = s1[walk] ^ s2[walk];
	}
	inline static void memxor(void * target, const void * source1, const void * source2, t_size size) {
		memxor_t( reinterpret_cast<t_uint8*>(target), reinterpret_cast<const t_uint8*>(source1), reinterpret_cast<const t_uint8*>(source2), size);
	}

	template<typename T>
	T* new_ptr_check_t(T* p_ptr) {
		if (p_ptr == NULL) throw std::bad_alloc();
		return p_ptr;
	}

	template<typename T>
	int sgn_t(const T & p_val) {
		if (p_val < 0) return -1;
		else if (p_val > 0) return 1;
		else return 0;
	}

	template<typename T> const T* empty_string_t();

	template<> inline const char * empty_string_t<char>() {return "";}
	template<> inline const wchar_t * empty_string_t<wchar_t>() {return L"";}


	template<typename type_t, typename arg_t>
	type_t replace_t(type_t & p_var,arg_t && p_newval) {
		auto oldval = std::move(p_var);
		p_var = std::forward<arg_t>(p_newval);
		return oldval;
	}

	template<typename t_type>
	t_type replace_null_t(t_type & p_var) {
		t_type ret = std::move(p_var);
		p_var = 0;
		return ret;
	}

	template<t_size p_size_pow2>
	inline bool is_ptr_aligned_t(const void * p_ptr) {
		static_assert_t< (p_size_pow2 & (p_size_pow2 - 1)) == 0 >();
		return ( ((t_size)p_ptr) & (p_size_pow2-1) ) == 0;
	}


	template<typename t_array>
	void array_rangecheck_t(const t_array & p_array,t_size p_index) {
		if (p_index >= pfc::array_size_t(p_array)) throw pfc::exception_overflow();
	}

	template<typename t_array>
	void array_rangecheck_t(const t_array & p_array,t_size p_from,t_size p_to) {
		if (p_from > p_to) throw pfc::exception_overflow();
		array_rangecheck_t(p_array,p_from); array_rangecheck_t(p_array,p_to);
	}

	t_int32 rint32(double p_val);
	t_int64 rint64(double p_val);



	template<typename array_t, typename pred_t>
	inline size_t remove_if_t( array_t & arr, pred_t pred ) {
		const size_t inCount = arr.size();
		size_t walk = 0;


		for( walk = 0; walk < inCount; ++ walk ) {
			if ( pred(arr[walk]) ) break;
		}

		size_t total = walk;

		for( ; walk < inCount; ++ walk ) {
			if ( !pred(arr[walk] ) ) {
				move_t(arr[total++], arr[walk]);
			}
		}
		arr.resize(total);

		return total;
	}

	template<typename t_array>
	inline t_size remove_mask_t(t_array & p_array,const bit_array & p_mask)//returns amount of items left
	{
		t_size n,count = p_array.size(), total = 0;

		n = total = p_mask.find(true,0,count);

		if (n<count)
		{
			for(n=p_mask.find(false,n+1,count-n-1);n<count;n=p_mask.find(false,n+1,count-n-1))
				move_t(p_array[total++],p_array[n]);

			p_array.resize(total);
			
			return total;
		}
		else return count;
	}

	template<typename t_array,typename t_compare>
	t_size find_duplicates_sorted_t(t_array p_array,t_size p_count,t_compare p_compare,bit_array_var & p_out) {
		t_size ret = 0;
		t_size n;
		if (p_count > 0)
		{
			p_out.set(0,false);
			for(n=1;n<p_count;n++)
			{
				bool found = p_compare(p_array[n-1],p_array[n]) == 0;
				if (found) ret++;
				p_out.set(n,found);
			}
		}
		return ret;
	}

	template<typename t_array,typename t_compare,typename t_permutation>
	t_size find_duplicates_sorted_permutation_t(t_array p_array,t_size p_count,t_compare p_compare,t_permutation const & p_permutation,bit_array_var & p_out) {
		t_size ret = 0;
		t_size n;
		if (p_count > 0) {
			p_out.set(p_permutation[0],false);
			for(n=1;n<p_count;n++)
			{
				bool found = p_compare(p_array[p_permutation[n-1]],p_array[p_permutation[n]]) == 0;
				if (found) ret++;
				p_out.set(p_permutation[n],found);
			}
		}
		return ret;
	}

	template<typename t_char>
	t_size strlen_t(const t_char * p_string,t_size p_length = ~0) {
		for(t_size walk = 0;;walk++) {
			if (walk >= p_length || p_string[walk] == 0) return walk;
		}
	}


	template<typename t_array>
	class __list_to_array_enumerator {
	public:
		__list_to_array_enumerator(t_array & p_array) : m_walk(), m_array(p_array) {}
		template<typename t_item>
		void operator() (const t_item & p_item) {
			PFC_ASSERT(m_walk < m_array.get_size());
			m_array[m_walk++] = p_item;
		}
		void finalize() {
			PFC_ASSERT(m_walk == m_array.get_size());
		}
	private:
		t_size m_walk;
		t_array & m_array;
	};

	template<typename t_list,typename t_array>
	void list_to_array(t_array & p_array,const t_list & p_list) {
		p_array.set_size(p_list.get_count());
		__list_to_array_enumerator<t_array> enumerator(p_array);
		p_list.enumerate(enumerator);
		enumerator.finalize();
	}

	template<typename t_receiver>
	class enumerator_add_item {
	public:
		enumerator_add_item(t_receiver & p_receiver) : m_receiver(p_receiver) {}
		template<typename t_item> void operator() (const t_item & p_item) {m_receiver.add_item(p_item);}
	private:
		t_receiver & m_receiver;
	};

	template<typename t_receiver,typename t_giver>
	void overwrite_list_enumerated(t_receiver & p_receiver,const t_giver & p_giver) {
		enumerator_add_item<t_receiver> wrapper(p_receiver);
		p_giver.enumerate(wrapper);
	}

	template<typename t_receiver,typename t_giver>
	void copy_list_enumerated(t_receiver & p_receiver,const t_giver & p_giver) {
		p_receiver.remove_all();
		overwrite_list_enumerated(p_receiver,p_giver);
	}

	inline bool lxor(bool p_val1,bool p_val2) {
		return p_val1 == !p_val2;
	}

	template<typename t_val>
	inline void min_acc(t_val & p_acc,const t_val & p_val) {
		if (p_val < p_acc) p_acc = p_val;
	}

	template<typename t_val>
	inline void max_acc(t_val & p_acc,const t_val & p_val) {
		if (p_val > p_acc) p_acc = p_val;
	}

	t_uint64 pow_int(t_uint64 base, t_uint64 exp) noexcept;
	double exp_int(double base, int exp) noexcept;


	template<typename t_val>
	class incrementScope {
	public:
		incrementScope(t_val & i) : v(i) {++v;}
		~incrementScope() {--v;}
	private:
		t_val & v;
	};
	template<typename obj_t>
	incrementScope<obj_t> autoIncrement(obj_t& v) { return incrementScope<obj_t>(v); }

	inline unsigned countBits32(uint32_t i) {
		const uint32_t mask = 0x11111111;
		uint32_t acc = i & mask;
		acc += (i >> 1) & mask;
		acc += (i >> 2) & mask;
		acc += (i >> 3) & mask;

		const uint32_t mask2 = 0x0F0F0F0F;
		uint32_t acc2 = acc & mask2;
		acc2 += (acc >> 4) & mask2;
	
		const uint32_t mask3 = 0x00FF00FF;
		uint32_t acc3 = acc2 & mask3;
		acc3 += (acc2 >> 8) & mask3;

		return (acc3 & 0xFFFF) + ((acc3 >> 16) & 0xFFFF);
	}

    // Forward declarations
    template<typename t_to,typename t_from>
	void copy_array_t(t_to & p_to,const t_from & p_from);

	template<typename t_array,typename t_value>
	void fill_array_t(t_array & p_array,const t_value & p_value);

	// Generic no-op for breakpointing stuff
	inline void nop() {}

	template<class T>
	class vartoggle_t {
		T oldval; T& var;
	public:
		vartoggle_t(const vartoggle_t&) = delete;
		void operator=(const vartoggle_t&) = delete;
		template<typename arg_t>
		vartoggle_t(T& p_var, arg_t&& val) : var(p_var) {
			oldval = std::move(var);
			var = std::forward<arg_t>(val);
		}
		~vartoggle_t() { var = std::move(oldval); }
	};

	template<typename T, typename arg_t>
	vartoggle_t<T> autoToggle(T& p_var, arg_t&& val) {
		return vartoggle_t<T>(p_var, std::forward<arg_t>(val));
	}

	template<class T>
	class vartoggle_volatile_t {
		T oldval; volatile T& var;
	public:
		template<typename arg_t>
		vartoggle_volatile_t(volatile T& p_var, arg_t && val) : var(p_var) {
			oldval = std::move(var);
			var = std::forward<arg_t>(val);
		}
		~vartoggle_volatile_t() { var = std::move(oldval); }
	};

	typedef vartoggle_t<bool> booltoggle;

	template<typename obj_t>
	class singleton {
	public:
		static obj_t instance;
	};
	template<typename obj_t>
	obj_t singleton<obj_t>::instance;

};
#define PFC_SINGLETON(X) ::pfc::singleton<X>::instance


#define PFC_CLASS_NOT_COPYABLE(THISCLASSNAME,THISTYPE) \
	THISCLASSNAME(const THISTYPE&) = delete; \
	const THISTYPE & operator=(const THISTYPE &) = delete;

#define PFC_CLASS_NOT_COPYABLE_EX(THISTYPE) PFC_CLASS_NOT_COPYABLE(THISTYPE,THISTYPE)


namespace pfc {
	template<typename t_char>
	t_size strlen_max_t(const t_char* ptr, t_size max) noexcept {
		PFC_ASSERT(ptr != NULL || max == 0);
		t_size n = 0;
		while (n < max && ptr[n] != 0) n++;
		return n;
	}

	inline t_size strlen_max(const char* ptr, t_size max) noexcept { return strlen_max_t(ptr, max); }
	inline t_size wcslen_max(const wchar_t* ptr, t_size max) noexcept { return strlen_max_t(ptr, max); }

#ifdef _WINDOWS
	inline t_size tcslen_max(const TCHAR* ptr, t_size max) noexcept { return strlen_max_t(ptr, max); }
#endif
}

namespace pfc {
	class autoScope {
	public:
		autoScope() {}
		autoScope(std::function<void()>&& f) : m_cleanup(std::move(f)) {}

		template<typename what_t> void increment(what_t& obj) {
			reset();
			++obj;
			m_cleanup = [&obj] { --obj; };
		}
		template<typename what_t, typename arg_t> void toggle(what_t& obj, arg_t && val) {
			reset();
			what_t old = obj;
			obj = std::forward<arg_t>(val);
			m_cleanup = [v = std::move(old), &obj]{
				obj = std::move(v);
			};
		}
		void operator() (std::function<void()>&& f) {
			reset(); m_cleanup = std::move(f);
		}

		~autoScope() {
			if (m_cleanup) m_cleanup();
		}

		void cancel() {
			m_cleanup = nullptr;
		}

		void reset() {
			if (m_cleanup) {
				m_cleanup();
				m_cleanup = nullptr;
			}
		}

		autoScope(const autoScope&) = delete;
		void operator=(const autoScope&) = delete;

		operator bool() const { return !!m_cleanup; }
	private:
		std::function<void()> m_cleanup;
	};
	typedef autoScope onLeaving;
}