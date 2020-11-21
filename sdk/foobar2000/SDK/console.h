//! Namespace with functions for sending text to console. All functions are fully multi-thread safe, though they must not be called during dll initialization or deinitialization (e.g. static object constructors or destructors) when service system is not available.
namespace console
{
	void info(const char * p_message);
	void error(const char * p_message);
	void warning(const char * p_message);
	void info_location(const playable_location & src);
	void info_location(const metadb_handle_ptr & src);
	void print_location(const playable_location & src);
	void print_location(const metadb_handle_ptr & src);

	void print(const char*);
    void printf(const char*,...);
	void printfv(const char*,va_list p_arglist);

	class lineWriter {
	public:
		const lineWriter & operator<<( const char * msg ) const {print(msg);return *this;}
	};

	//! Usage: console::formatter() << "blah " << somenumber << " asdf" << somestring;
	class formatter : public pfc::string_formatter {
	public:
		~formatter() {if (!is_empty()) console::print(get_ptr());}
	};
#define FB2K_console_formatter() ::console::formatter()._formatter()
#define FB2K_console_formatter1() ::console::lineWriter()
#define FB2K_console_print(X) ::console::print(X)
    
	void complain(const char * what, const char * msg);
	void complain(const char * what, std::exception const & e);

	class timer_scope {
	public:
		timer_scope(const char * name) : m_name(name) {m_timer.start();}
		~timer_scope() {
			try {
				FB2K_console_formatter() << m_name << ": " << pfc::format_time_ex(m_timer.query(), 6);
			} catch(...) {}
		}
	private:
		pfc::hires_timer m_timer;
		const char * const m_name;
	};
};

//! Interface receiving console output. Do not call directly; use console namespace functions instead.
class NOVTABLE console_receiver : public service_base {
public:
	virtual void print(const char * p_message,t_size p_message_length) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(console_receiver);
};
