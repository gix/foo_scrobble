#pragma once
#include "iterators.h"

namespace pfc {	

	template<typename t_item>
	class __chain_list_elem : public _list_node<t_item> {
	public:
		typedef _list_node<t_item> t_node;
        template<typename ... arg_t > __chain_list_elem( arg_t && ... args ) : t_node( std::forward<arg_t>(args) ... ) {m_prev = m_next = NULL;}

		typedef __chain_list_elem<t_item> t_self;

		t_self * m_prev, * m_next;

		t_node * prev() throw() {return m_prev;}
		t_node * next() throw() {return m_next;}

		//helper wrappers
		void add_ref() throw() {this->refcount_add_ref();}
		void release() throw() {this->refcount_release();}
		//workaround for cross-list-relinking case - never actually deletes p_elem
		void __release_temporary() throw() {this->_refcount_release_temporary();}
	};

	//! Differences between chain_list_v2_t<> and old chain_list_t<>: \n
	//! Iterators pointing to removed items as well as to items belonging to no longer existing list objects remain valid but they're no longer walkable - as if the referenced item was the only item in the list. The old class invalidated iterators on deletion instead.
	template<typename _t_item>
	class chain_list_v2_t {
	public:
		typedef _t_item t_item;
		typedef chain_list_v2_t<t_item> t_self;
		typedef ::pfc::iterator<t_item> iterator;
		typedef ::pfc::const_iterator<t_item> const_iterator;
		typedef ::pfc::forward_iterator<t_item> forward_iterator;
		typedef ::pfc::forward_const_iterator<t_item> forward_const_iterator;
		typedef __chain_list_elem<t_item> t_elem;

		chain_list_v2_t() {}
		chain_list_v2_t(const t_self & p_source) {
			try {
				*this = p_source;
			} catch(...) {
				remove_all();
				throw;
			}
		}

		chain_list_v2_t(t_self && p_source) {
			append_move_from( p_source );
		}

		template<typename t_in> void _set(const t_in & in) {
			remove_all(); _add(in);
		}
		template<typename t_in> void _add(const t_in & in) {
			for(typename t_in::const_iterator iter = in.first(); iter.is_valid(); ++in) add_item(*iter);
		}
		template<typename t_in> t_self & operator=(const t_in & in) {_set(in); return *this;}

		t_self & operator=(const t_self & p_other) {
			remove_all();
			for(t_elem * walk = p_other.m_first; walk != NULL; walk = walk->m_next) {
				add_item(walk->m_content);
			}
			return *this;
		}
		t_self & operator=(t_self && p_other) {
			move_from(p_other); return *this;
		}

		// templated constructors = spawn of satan
		// template<typename t_other> chain_list_v2_t(const t_other & in) { try {_add(in);} catch(...) {remove_all(); throw;} }

		t_size get_count() const {return m_count;}

		iterator first() {return iterator(m_first);}
		iterator last() {return iterator(m_last);}
		const_iterator first() const {return const_iterator(m_first);}
		const_iterator last() const {return const_iterator(m_last);}
		const_iterator cfirst() const {return const_iterator(m_first);}
		const_iterator clast() const {return const_iterator(m_last);}

		forward_iterator begin() { return first(); }
		forward_const_iterator begin() const { return first(); }
		forward_iterator end() {return forward_iterator(); }
		forward_const_iterator end() const { return forward_const_iterator(); }

		void remove_single(const_iterator const & p_iter) {
			PFC_ASSERT(p_iter.is_valid());
			__unlink(_elem(p_iter));
		}

		void remove(const_iterator const & p_iter) {
			PFC_ASSERT(p_iter.is_valid());
			__unlink(_elem(p_iter));
		}

		void remove_all() throw() {
			while(m_first != NULL) __unlink(m_first);
			PFC_ASSERT(m_count == 0);
		}
		void remove_range(const_iterator const & p_from,const_iterator const & p_to) {
			for(t_elem * walk = _elem(p_from);;) {
				if (walk == NULL) {PFC_ASSERT(!"Should not get here"); break;}//should not happen unless there is a bug in calling code
				t_elem * next = walk->m_next;
				__unlink(walk);
				if (walk == _elem(p_to)) break;
				walk = next;
			}
		}

		template<typename t_callback> void enumerate(t_callback & p_callback) const {__enumerate_chain<const t_elem>(m_first,p_callback);}
		template<typename t_callback> void enumerate(t_callback & p_callback) {__enumerate_chain<t_elem>(m_first,p_callback);}

		template<typename t_source> bool remove_item(const t_source & p_item) {
			t_elem * elem;
			if (__find(elem,p_item)) {
				__unlink(elem);
				return true;
			} else {
				return false;
			}
		}

		~chain_list_v2_t() {remove_all();}

		template<typename ... arg_t>
		inline void add_item(arg_t && ... arg) {
            __link_last(new t_elem(std::forward<arg_t>(arg) ... ));
		}
		template<typename t_source>
		inline t_self & operator+=(t_source && p_source) {
            add_item(std::forward<t_source>(p_source)); return *this;
		}
		iterator insert_last() {return __link_last(new t_elem);}
		iterator insert_first() {return __link_first(new t_elem);}
		iterator insert_after(const_iterator const & p_iter) {return __link_next(_elem(p_iter),new t_elem);}
		iterator insert_before(const_iterator const & p_iter) {return __link_prev(_elem(p_iter),new t_elem);}
        template<typename ... arg_t> iterator insert_last(arg_t && ... arg) {return __link_last(new t_elem(std::forward<arg_t>(arg) ...));}
        template<typename ... arg_t> iterator insert_first(arg_t && ... arg) {return __link_first(new t_elem(std::forward<arg_t>(arg) ...));}
        template<typename ... arg_t> iterator insert_after(const_iterator const & p_iter,arg_t && ... arg) {return __link_next(_elem(p_iter),new t_elem(std::forward<arg_t>(arg) ... ));}
		template<typename ... arg_t> iterator insert_before(const_iterator const & p_iter,arg_t && ... arg) {return __link_prev(_elem(p_iter),new t_elem(std::forward<arg_t>(arg) ... ));}

		template<typename t_source> const_iterator find_item(const t_source & p_item) const {
			t_elem * elem;
			if (!__find(elem,p_item)) return const_iterator();
			return const_iterator(elem);
		}

		template<typename t_source> iterator find_item(const t_source & p_item) {
			t_elem * elem;
			if (!__find(elem,p_item)) return iterator();
			return iterator(elem);
		}

		template<typename t_source> bool have_item(const t_source & p_item) const {
			t_elem * dummy;
			return __find(dummy,p_item);
		}
		template<typename t_source> void set_single(const t_source & p_item) {
			remove_all(); add_item(p_item);
		}

		//! Slow!
		const_iterator by_index(t_size p_index) const {return __by_index(p_index);}
		//! Slow!
		iterator by_index(t_size p_index) {return __by_index(p_index);}

		t_self & operator<<(t_self & p_other) {
			while(p_other.m_first != NULL) {
				__link_last( p_other.__unlink_temporary(p_other.m_first) );
			}
			return *this;
		}
		t_self & operator>>(t_self & p_other) {
			while(m_last != NULL) {
				p_other.__link_first(__unlink_temporary(m_last));
			}
			return p_other;
		}
		//! Links an object that has been unlinked from another list. Unsafe.
		void _link_last(const_iterator const& iter) {
			PFC_ASSERT(iter.is_valid());
			PFC_ASSERT( _elem(iter)->m_prev == NULL && _elem(iter)->m_next == NULL );
			__link_last(_elem(iter));
		}
		//! Links an object that has been unlinked from another list. Unsafe.
		void _link_first(const_iterator const& iter) {
			PFC_ASSERT(iter.is_valid());
			PFC_ASSERT( _elem(iter)->m_prev == NULL && _elem(iter)->m_next == NULL );
			__link_first(_elem(iter));
		}
		void append_move_from( t_self & p_other ) {
			while (p_other.m_first != NULL) {
				__link_last(p_other.__unlink_temporary(p_other.m_first));
			}
		}
		void move_from( t_self & p_other ) {
			remove_all();
			append_move_from( p_other );
		}
	private:
		static t_elem * _elem(const_iterator const & iter) {
			return static_cast<t_elem*>(iter._node());
		}
		t_elem * __by_index(t_size p_index) const {
			t_elem * walk = m_first;
			while(p_index > 0 && walk != NULL) {
				p_index--;
				walk = walk->m_next;
			}
			return walk;
		}
		template<typename t_elemwalk,typename t_callback>
		static void __enumerate_chain(t_elemwalk * p_elem,t_callback & p_callback) {
			t_elemwalk * walk = p_elem;
			while(walk != NULL) {
				p_callback(walk->m_content);
				walk = walk->m_next;
			}
		}

		template<typename t_source> bool __find(t_elem * & p_elem,const t_source & p_item) const {
			for(t_elem * walk = m_first; walk != NULL; walk = walk->m_next) {
				if (walk->m_content == p_item) {
					p_elem = walk; return true;
				}
			}
			return false;
		}

		void __unlink_helper(t_elem * p_elem) throw() {
			(p_elem->m_prev == NULL ? m_first : p_elem->m_prev->m_next) = p_elem->m_next;
			(p_elem->m_next == NULL ? m_last : p_elem->m_next->m_prev) = p_elem->m_prev;
			p_elem->m_next = p_elem->m_prev = NULL;
		}

		//workaround for cross-list-relinking case - never actually deletes p_elem
		t_elem * __unlink_temporary(t_elem * p_elem) throw() {
			__unlink_helper(p_elem);
			--m_count; p_elem->__release_temporary();
			return p_elem;
		}

		t_elem * __unlink(t_elem * p_elem) throw() {
			__unlink_helper(p_elem);
			--m_count; p_elem->release();
			return p_elem;
		}
		void __on_link(t_elem * p_elem) throw() {
			p_elem->add_ref(); ++m_count;
		}
		t_elem * __link_first(t_elem * p_elem) throw() {
			__on_link(p_elem);
			p_elem->m_next = m_first;
			p_elem->m_prev = NULL;
			(m_first == NULL ? m_last : m_first->m_prev) = p_elem;
			m_first = p_elem;
			return p_elem;
		}
		t_elem * __link_last(t_elem * p_elem) throw() {
			__on_link(p_elem);
			p_elem->m_prev = m_last;
			p_elem->m_next = NULL;
			(m_last == NULL ? m_first : m_last->m_next) = p_elem;
			m_last = p_elem;
			return p_elem;
		}
		t_elem * __link_next(t_elem * p_prev,t_elem * p_elem) throw() {
			__on_link(p_elem);
			p_elem->m_prev = p_prev;
			p_elem->m_next = p_prev->m_next;
			(p_prev->m_next != NULL ? p_prev->m_next->m_prev : m_last) = p_elem;
			p_prev->m_next = p_elem;
			return p_elem;
		}
		t_elem * __link_prev(t_elem * p_next,t_elem * p_elem) throw() {
			__on_link(p_elem);
			p_elem->m_next = p_next;
			p_elem->m_prev = p_next->m_prev;
			(p_next->m_prev != NULL ? p_next->m_prev->m_next : m_first) = p_elem;
			p_next->m_prev = p_elem;
			return p_elem;
		}
		t_elem * m_first = nullptr, * m_last = nullptr;
		t_size m_count = 0;
	};


	template<typename t_item> class traits_t<chain_list_v2_t<t_item> > : public traits_default_movable {};

	class __chain_list_iterator_traits : public traits_default_movable {
	public:
		enum {
			constructor_may_fail = false
		};
	};

	template<typename t_item> class traits_t<const_iterator<t_item> > : public traits_t<refcounted_object_ptr_t<_list_node<t_item> > > {};
	
	template<typename t_item> class traits_t<iterator<t_item> > : public traits_t<const_iterator<t_item> > {};

}//namespace pfc
