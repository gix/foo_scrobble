#pragma once
#include "list.h"

namespace pfc {

	template<class T, class B = list_t<T*> >
	class ptr_list_t : public B {
	public:
		typedef ptr_list_t<T, B> self_t;

		void free_by_idx(t_size n) {free_mask(bit_array_one(n));}
		void free_all() {this->remove_all_ex(free);}
		void free_mask(const bit_array & p_mask) {this->remove_mask_ex(p_mask,free);}

		void delete_item(T* ptr) {delete_by_idx(find_item(ptr));}

		void delete_by_idx(t_size p_index) {
			delete_mask(bit_array_one(p_index));
		}

		void delete_all() {
			this->remove_all_ex(pfc::delete_t<T>);
		}

		void delete_mask(const bit_array & p_mask) {
			this->remove_mask_ex(p_mask,pfc::delete_t<T>);
		}

		T * operator[](t_size n) const {return this->get_item(n);}
	};

	template<typename T,t_size N>
	class ptr_list_hybrid_t : public ptr_list_t<T,list_hybrid_t<T*,N> > {
	public:
	};

	typedef ptr_list_t<void> ptr_list;

	template<typename item, typename base> class traits_t<ptr_list_t<item, base> > : public traits_t<base> {};
}
