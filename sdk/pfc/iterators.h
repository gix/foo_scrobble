#pragma once
#include "ref_counter.h"

namespace pfc {
	//! Base class for list nodes. Implemented by list implementers.
	template<typename t_item> class _list_node : public refcounted_object_root {
	public:
		typedef _list_node<t_item> t_self;

        template<typename ... arg_t> _list_node(arg_t && ... arg) : m_content( std::forward<arg_t>(arg) ...) {}

		t_item m_content;

		virtual t_self * prev() throw() {return NULL;}
		virtual t_self * next() throw() {return NULL;}

		t_self * walk(bool forward) throw() {return forward ? next() : prev();}
	};

	template<typename t_item> class const_iterator {
	public:
		typedef _list_node<t_item> t_node;
		typedef refcounted_object_ptr_t<t_node> t_nodeptr;
		typedef const_iterator<t_item> t_self;
		
		bool is_empty() const throw() {return m_content.is_empty();}
		bool is_valid() const throw() {return m_content.is_valid();}
		void invalidate() throw() {m_content = NULL;}

		void walk(bool forward) throw() {m_content = m_content->walk(forward);}
		void prev() throw() {m_content = m_content->prev();}
		void next() throw() {m_content = m_content->next();}

		//! For internal use / list implementations only! Do not call!
		t_node* _node() const throw() {return m_content.get_ptr();}

		const_iterator() {}
		const_iterator(t_node* source) : m_content(source) {}
		const_iterator(t_nodeptr const & source) : m_content(source) {}
		const_iterator(t_self const & other) : m_content(other.m_content) {}
		const_iterator(t_self && other) : m_content(std::move(other.m_content)) {}

		t_self const & operator=(t_self const & other) {m_content = other.m_content; return *this;}
		t_self const & operator=(t_self && other) {m_content = std::move(other.m_content); return *this;}

		const t_item& operator*() const throw() {return m_content->m_content;}
		const t_item* operator->() const throw() {return &m_content->m_content;}

		const t_self & operator++() throw() {this->next(); return *this;}
		const t_self & operator--() throw() {this->prev(); return *this;}
		t_self operator++(int) throw() {t_self old = *this; this->next(); return old;}
		t_self operator--(int) throw() {t_self old = *this; this->prev(); return old;}

		bool operator==(const t_self & other) const throw() {return this->m_content == other.m_content;}
		bool operator!=(const t_self & other) const throw() {return this->m_content != other.m_content;}
	protected:
		t_nodeptr m_content;
	};
	template<typename t_item> class iterator : public const_iterator<t_item> {
	public:
		typedef const_iterator<t_item> t_selfConst;
		typedef iterator<t_item> t_self;
		typedef _list_node<t_item> t_node;
		typedef refcounted_object_ptr_t<t_node> t_nodeptr;

		iterator() {}
		iterator(t_node* source) : t_selfConst(source) {}
		iterator(t_nodeptr const & source) : t_selfConst(source) {}
		iterator(t_self const & other) : t_selfConst(other) {}
		iterator(t_self && other) : t_selfConst(std::move(other)) {}

		t_self const & operator=(t_self const & other) {this->m_content = other.m_content; return *this;}
		t_self const & operator=(t_self && other) {this->m_content = std::move(other.m_content); return *this;}

		t_item& operator*() const throw() {return this->m_content->m_content;}
		t_item* operator->() const throw() {return &this->m_content->m_content;}

		const t_self & operator++() throw() {this->next(); return *this;}
		const t_self & operator--() throw() {this->prev(); return *this;}
		t_self operator++(int) throw() {t_self old = *this; this->next(); return old;}
		t_self operator--(int) throw() {t_self old = *this; this->prev(); return old;}

		bool operator==(const t_self & other) const throw() {return this->m_content == other.m_content;}
		bool operator!=(const t_self & other) const throw() {return this->m_content != other.m_content;}
	};

	template<typename item_t> class forward_iterator {
		iterator<item_t> m_iter;
	public:
		typedef forward_iterator<item_t> self_t;
		void operator++() { ++m_iter; }

		item_t& operator*() const throw() { return *m_iter; }
		item_t* operator->() const throw() { return &*m_iter; }

		bool operator==(const self_t& other) const { return m_iter == other.m_iter; }
		bool operator!=(const self_t& other) const { return m_iter != other.m_iter; }

		forward_iterator() {}
		forward_iterator(iterator<item_t>&& i) : m_iter(std::move(i)) {}
	};


	template<typename item_t> class forward_const_iterator {
		const_iterator<item_t> m_iter;
	public:
		typedef forward_const_iterator<item_t> self_t;
		void operator++() { ++m_iter; }

		const item_t& operator*() const throw() { return *m_iter; }
		const item_t* operator->() const throw() { return &*m_iter; }

		bool operator==(const self_t& other) const { return m_iter == other.m_iter; }
		bool operator!=(const self_t& other) const { return m_iter != other.m_iter; }

		forward_const_iterator() {}
		forward_const_iterator(const_iterator<item_t>&& i) : m_iter(std::move(i)) {}
	};

	template<typename t_comparator = comparator_default>
	class comparator_list {
	public:
		template<typename t_list1, typename t_list2>
		static int compare(const t_list1 & p_list1, const t_list2 & p_list2) {
			typename t_list1::const_iterator iter1 = p_list1.first();
			typename t_list2::const_iterator iter2 = p_list2.first();
			for(;;) {
				if (iter1.is_empty() && iter2.is_empty()) return 0;
				else if (iter1.is_empty()) return -1;
				else if (iter2.is_empty()) return 1;
				else {
					int state = t_comparator::compare(*iter1,*iter2);
					if (state != 0) return state;
				}
				++iter1; ++iter2;
			}
		}
	};

	template<typename t_list1, typename t_list2>
	static bool listEquals(const t_list1 & p_list1, const t_list2 & p_list2) {
		typename t_list1::const_iterator iter1 = p_list1.first();
		typename t_list2::const_iterator iter2 = p_list2.first();
		for(;;) {
			if (iter1.is_empty() && iter2.is_empty()) return true;
			else if (iter1.is_empty() || iter2.is_empty()) return false;
			else if (*iter1 != *iter2) return false;
			++iter1; ++iter2;
		}
	}

	template<typename comparator_t = comparator_default>
	class comparator_stdlist { 
	public:
		template<typename t_list1, typename t_list2>
		static int compare(const t_list1 & p_list1, const t_list2 & p_list2) {
			auto iter1 = p_list1.begin();
			auto iter2 = p_list2.begin();
			for(;;) {
				const bool end1 = iter1 == p_list1.end();
				const bool end2 = iter2 == p_list2.end();
				if ( end1 && end2 ) return 0;
				else if ( end1 ) return -1;
				else if ( end2 ) return 1;
				else {
					int state = comparator_t::compare(*iter1,*iter2);
					if (state != 0) return state;
				}
				++iter1; ++iter2;
			}
		}
	};
}

namespace std {

	template<typename item_t>
	struct iterator_traits< pfc::forward_iterator< item_t > > {
		typedef ptrdiff_t difference_type;
		typedef item_t value_type;
		typedef value_type* pointer;
		typedef value_type& reference;
		typedef std::forward_iterator_tag iterator_category;
	};
	template<typename item_t>
	struct iterator_traits< pfc::forward_const_iterator< item_t > > {
		typedef ptrdiff_t difference_type;
		typedef item_t value_type;
		typedef const value_type* pointer;
		typedef const value_type& reference;
		typedef std::forward_iterator_tag iterator_category;
	};
}