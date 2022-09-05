#pragma once

//! Callback service receiving notifications about metadb contents changes.
class NOVTABLE metadb_io_callback : public service_base {
public:
	//! Called when metadb contents change. (Or, one of display hook component requests display update).
	//! @param p_items_sorted List of items that have been updated. The list is always sorted by pointer value, to allow fast bsearch to test whether specific item has changed.
	//! @param p_fromhook Set to true when actual file contents haven't changed but one of metadb_display_field_provider implementations requested an update so output of metadb_handle::format_title() etc has changed.
	virtual void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb_io_callback);
};

//! Dynamically-registered version of metadb_io_callback. See metadb_io_callback for documentation, register instances using metadb_io_v3::register_callback(). It's recommended that you use the metadb_io_callback_dynamic_impl_base helper class to manage registration/unregistration.
class NOVTABLE metadb_io_callback_dynamic {
public:
	virtual void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) = 0;

	void register_callback(); void unregister_callback();

};

//! metadb_io_callback_dynamic implementation helper.
class metadb_io_callback_dynamic_impl_base : public metadb_io_callback_dynamic {
public:
	void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) override {}

	metadb_io_callback_dynamic_impl_base() { register_callback(); }
	~metadb_io_callback_dynamic_impl_base() { unregister_callback(); }

	PFC_CLASS_NOT_COPYABLE_EX(metadb_io_callback_dynamic_impl_base)
};

//! \since 1.1
//! Callback service receiving notifications about user-triggered tag edits. \n
//! You want to use metadb_io_callback instead most of the time, unless you specifically want to track tag edits for purposes other than updating user interface.
class NOVTABLE metadb_io_edit_callback : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb_io_edit_callback)
public:
	//! Called after the user has edited tags on a set of files.
	typedef const pfc::list_base_const_t<const file_info*>& t_infosref;
	virtual void on_edited(metadb_handle_list_cref items, t_infosref before, t_infosref after) = 0;
};

//! \since 2.0
//! Parameter for on_changed_sorted_v2()
class NOVTABLE metadb_io_callback_v2_data {
public:
	virtual metadb_v2_rec_t get(size_t idxInList) = 0;
	metadb_v2_rec_t operator[](size_t i) { return get(i); }
};

//! \since 2.0
class NOVTABLE metadb_io_callback_v2 : public metadb_io_callback {
	FB2K_MAKE_SERVICE_INTERFACE(metadb_io_callback_v2, metadb_io_callback);
public:
	virtual void on_changed_sorted_v2(metadb_handle_list_cref itemsSorted, metadb_io_callback_v2_data & data, bool bFromHook) = 0;
	//! Reserved for future use.
	virtual double get_callback_merit() { return 0; }
};


class NOVTABLE metadb_io_callback_v2_dynamic {
public:
	virtual void on_changed_sorted_v2(metadb_handle_list_cref itemsSorted, metadb_io_callback_v2_data & data, bool bFromHook) = 0;
	//! Reserved for future use.
	virtual double get_callback_merit() { return 0; }

	bool try_register_callback(); void try_unregister_callback();
	void register_callback(); void unregister_callback();
};

class metadb_io_callback_v2_dynamic_impl_base : public metadb_io_callback_v2_dynamic {
public:
	void on_changed_sorted_v2(metadb_handle_list_cref itemsSorted, metadb_io_callback_v2_data & data, bool bFromHook) override {}

	metadb_io_callback_v2_dynamic_impl_base() { register_callback(); }
	~metadb_io_callback_v2_dynamic_impl_base() { unregister_callback(); }

	PFC_CLASS_NOT_COPYABLE_EX(metadb_io_callback_v2_dynamic_impl_base)
};
