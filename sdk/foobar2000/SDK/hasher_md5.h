#pragma once

struct hasher_md5_state {
	char m_data[128];
};

struct hasher_md5_result {
	char m_data[16];

	t_uint64 xorHalve() const;
	GUID asGUID() const;
	pfc::string8 asString() const;

	static hasher_md5_result null() {hasher_md5_result h = {}; return h;}
};

FB2K_STREAM_READER_OVERLOAD(hasher_md5_result) {
	stream.read_raw(&value, sizeof(value)); return stream;
}
FB2K_STREAM_WRITER_OVERLOAD(hasher_md5_result) {
	stream.write_raw(&value, sizeof(value)); return stream;
}

inline bool operator==(const hasher_md5_result & p_item1,const hasher_md5_result & p_item2) {return memcmp(&p_item1,&p_item2,sizeof(hasher_md5_result)) == 0;}
inline bool operator!=(const hasher_md5_result & p_item1,const hasher_md5_result & p_item2) {return memcmp(&p_item1,&p_item2,sizeof(hasher_md5_result)) != 0;}

namespace pfc {
	template<> class traits_t<hasher_md5_state> : public traits_rawobject {};
	template<> class traits_t<hasher_md5_result> : public traits_rawobject {};
	
	template<> inline int compare_t(const hasher_md5_result & p_item1, const hasher_md5_result & p_item2) {
		return memcmp(&p_item1, &p_item2, sizeof(hasher_md5_result));
	}
	
}

class NOVTABLE hasher_md5 : public service_base
{
public:

	virtual void initialize(hasher_md5_state & p_state) = 0;
	virtual void process(hasher_md5_state & p_state,const void * p_buffer,t_size p_bytes) = 0;
	virtual hasher_md5_result get_result(const hasher_md5_state & p_state) = 0;

	
	static GUID guid_from_result(const hasher_md5_result & param);

	hasher_md5_result process_single(const void * p_buffer,t_size p_bytes);
	hasher_md5_result process_single_string(const char * str) {return process_single(str, strlen(str));}
	GUID process_single_guid(const void * p_buffer,t_size p_bytes);
	GUID get_result_guid(const hasher_md5_state & p_state) {return guid_from_result(get_result(p_state));}

	
	//! Helper
	void process_string(hasher_md5_state & p_state,const char * p_string,t_size p_length = ~0) {return process(p_state,p_string,pfc::strlen_max(p_string,p_length));}

	FB2K_MAKE_SERVICE_COREAPI(hasher_md5);
};


class stream_writer_hasher_md5 : public stream_writer {
public:
	stream_writer_hasher_md5() {
		m_hasher->initialize(m_state);
	}
	void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
		p_abort.check();
		m_hasher->process(m_state,p_buffer,p_bytes);
	}
	hasher_md5_result result() const {
		return m_hasher->get_result(m_state);
	}
	GUID resultGuid() const {
		return hasher_md5::guid_from_result(result());
	}
private:
	hasher_md5_state m_state;
	const hasher_md5::ptr m_hasher = hasher_md5::get();
};

template<bool isBigEndian = false>
class stream_formatter_hasher_md5 : public stream_writer_formatter<isBigEndian> {
public:
	stream_formatter_hasher_md5() : stream_writer_formatter<isBigEndian>(_m_stream,fb2k::noAbort) {}

	hasher_md5_result result() const {
		return _m_stream.result();
	}
	GUID resultGuid() const {
		return hasher_md5::guid_from_result(result());
	}
private:
	stream_writer_hasher_md5 _m_stream;
};
