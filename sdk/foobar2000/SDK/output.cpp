#include "foobar2000.h"
#include "output.h"
#include "audio_chunk_impl.h"
#include "dsp.h"
#include "resampler.h"

pfc::string8 output_entry::get_device_name( const GUID & deviceID ) {
	pfc::string8 temp;
	if (!get_device_name(deviceID, temp)) temp = "[unknown device]";
	return temp;
}

namespace {
	class output_device_enum_callback_getname : public output_device_enum_callback {
	public:
		output_device_enum_callback_getname( const GUID & wantID, pfc::string_base & strOut ) : m_wantID(wantID), m_got(), m_strOut(strOut) {}
		void on_device(const GUID & p_guid,const char * p_name,unsigned p_name_length) {
			if (!m_got && p_guid == m_wantID) {
				m_strOut.set_string(p_name, p_name_length);
				m_got = true;
			}
		}
		bool m_got;
		pfc::string_base & m_strOut;
		const GUID m_wantID;
	};

}

bool output_entry::get_device_name( const GUID & deviceID, pfc::string_base & out ) {
	output_device_enum_callback_getname cb(deviceID, out);
	this->enum_devices(cb);
	return cb.m_got;
}

bool output_entry::g_find( const GUID & outputID, output_entry::ptr & outObj ) {
	service_enum_t<output_entry> e; output_entry::ptr obj;
	while(e.next(obj)) {
		if (obj->get_guid() == outputID) {
			outObj = obj; return true;
		}
	}
	return false;
}

output_entry::ptr output_entry::g_find( const GUID & outputID ) {
	output_entry::ptr ret;
	if (!g_find( outputID, ret ) ) throw exception_output_module_not_found();
	return ret;
}


bool output::is_progressing_() {
    output_v4::ptr v4;
    if ( v4 &= this ) return v4->is_progressing();
    return true;
}

size_t output::update_v2_() {
    output_v4::ptr v4;
    if ( v4 &= this ) return v4->update_v2();
    bool bReady = false;
    this->update(bReady);
    return bReady ? SIZE_MAX : 0;
}

pfc::eventHandle_t output::get_trigger_event_() {
    output_v4::ptr v4;
    if ( v4 &= this ) return v4->get_trigger_event();
    return pfc::eventInvalid;
}

void output_impl::flush() {
	m_incoming_ptr = 0;
	m_incoming.set_size(0);
	on_flush();
}
void output_impl::flush_changing_track() {
	m_incoming_ptr = 0;
	m_incoming.set_size(0);
	on_flush_changing_track();
}

void output_impl::update(bool & p_ready) {
    p_ready = update_v2() > 0;
}
size_t output_impl::update_v2() {
	on_update();
	if (m_incoming_spec != m_active_spec && m_incoming_ptr < m_incoming.get_size()) {
		if (get_latency_samples() == 0) {
			open(m_incoming_spec);
			m_active_spec = m_incoming_spec;
		} else {
			force_play();
		}
	}
    size_t retCanWriteSamples = 0;
	if (m_incoming_spec == m_active_spec && m_incoming_ptr < m_incoming.get_size()) {
		t_size cw = can_write_samples() * m_incoming_spec.m_channels;
		t_size delta = pfc::min_t(m_incoming.get_size() - m_incoming_ptr,cw);
		if (delta > 0) {
			write(audio_chunk_temp_impl(m_incoming.get_ptr()+m_incoming_ptr,delta / m_incoming_spec.m_channels,m_incoming_spec.m_sample_rate,m_incoming_spec.m_channels,m_incoming_spec.m_channel_config));
			m_incoming_ptr += delta;
		}
        retCanWriteSamples = (cw - delta) / m_incoming_spec.m_channels;
    } else if ( m_incoming_ptr == m_incoming.get_size() ) {
        retCanWriteSamples = SIZE_MAX;
    }
    return retCanWriteSamples;
}

double output_impl::get_latency() {
	double ret = 0;
	if (m_incoming_spec.is_valid()) {
		ret += audio_math::samples_to_time( (m_incoming.get_size() - m_incoming_ptr) / m_incoming_spec.m_channels, m_incoming_spec.m_sample_rate );
	}
	if (m_active_spec.is_valid()) {
		ret += audio_math::samples_to_time( get_latency_samples() , m_active_spec.m_sample_rate );
	}
	return ret;
}
void output_impl::process_samples(const audio_chunk & p_chunk) {
	pfc::dynamic_assert(m_incoming_ptr == m_incoming.get_size());
	t_samplespec spec;
	spec.fromchunk(p_chunk);
	if (!spec.is_valid()) pfc::throw_exception_with_message< exception_io_data >("Invalid audio stream specifications");
	m_incoming_spec = spec;
	t_size length = p_chunk.get_used_size();
	m_incoming.set_data_fromptr(p_chunk.get_data(),length);
	m_incoming_ptr = 0;
}

void output_v3::get_injected_dsps( dsp_chain_config & dsps ) {
	dsps.remove_all();
#if 0 // DEPRECATED
	unsigned rate = this->get_forced_sample_rate();
	if (rate != 0) {
#if PFC_DEBUG
		FB2K_console_formatter() << "output_v3::get_injected_dsps() : requesting resampling to " << rate << " Hz";
#endif
		dsp_preset_impl temp;
		if (resampler_entry::g_create_preset( temp, 0, rate, 0 )) {
			dsps.insert_item( temp, dsps.get_count() );
		} else {
#if PFC_DEBUG
			FB2K_console_formatter() << "output_v3::get_injected_dsps() : resampler could not be created";
#endif
		}
	}
#endif
}

size_t output_v4::update_v2() {
    bool bReady = false;
    update(bReady);
    return bReady ? SIZE_MAX : 0;
}

uint32_t output_entry::get_config_flags_compat() {
	uint32_t ret = get_config_flags();
	if ((ret & (flag_low_latency | flag_high_latency)) == 0) {
		// output predating flag_high_latency + flag_low_latency
		// if it's old foo_out_upnp, report high latency, otherwise low latency.
		static const GUID guid_foo_out_upnp = { 0x9900b4f6, 0x8431, 0x4b0a, { 0x95, 0x56, 0xa7, 0xfc, 0xb9, 0x5b, 0x74, 0x3 } };
		if (this->get_guid() == guid_foo_out_upnp) ret |= flag_high_latency;
		else ret |= flag_low_latency;
	}
	return ret;
}

bool output_entry::is_high_latency() {
	return (this->get_config_flags_compat() & flag_high_latency) != 0;
}

bool output_entry::is_low_latency() {
	return (this->get_config_flags_compat() & flag_low_latency) != 0;
}

// {EEEB07DE-C2C8-44c2-985C-C85856D96DA1}
const GUID output_id_null = 
{ 0xeeeb07de, 0xc2c8, 0x44c2, { 0x98, 0x5c, 0xc8, 0x58, 0x56, 0xd9, 0x6d, 0xa1 } };

// {D41D2423-FBB0-4635-B233-7054F79814AB}
const GUID output_id_default = 
{ 0xd41d2423, 0xfbb0, 0x4635, { 0xb2, 0x33, 0x70, 0x54, 0xf7, 0x98, 0x14, 0xab } };

outputCoreConfig_t outputCoreConfig_t::defaults() {
	outputCoreConfig_t cfg = {};
	cfg.m_bitDepth = 16;
	cfg.m_buffer_length = 1.0;
	cfg.m_output = output_id_default;
	// remaining fields nulled by {}
	return cfg;
}
namespace {
	class output_device_list_callback_impl : public output_device_list_callback {
	public:
		void onDevice( const char * fullName, const GUID & output, const GUID & device ) {
			f(fullName, output, device);
		}
		std::function< void ( const char*, const GUID&, const GUID&) > f;
	};

	class output_config_change_callback_impl : public output_config_change_callback {
	public:
		void outputConfigChanged() {
			f();
		}
		std::function<void () > f;
	};
}
void output_manager_v2::listDevices( std::function< void ( const char*, const GUID&, const GUID&) > f ) {
	output_device_list_callback_impl cb; cb.f = f;
	this->listDevices( cb );
}

service_ptr output_manager_v2::addCallback( std::function<void() > f ) {
	output_config_change_callback_impl * obj = new output_config_change_callback_impl();
	obj->f = f;
 	this->addCallback( obj ); 
	service_ptr_t<output_manager_v2> selfRef ( this );
	return fb2k::callOnRelease( [obj, selfRef] {
		selfRef->removeCallback( obj ); delete obj;
	} );
}

void output_manager_v2::addCallbackPermanent( std::function<void()> f ) {
	output_config_change_callback_impl * obj = new output_config_change_callback_impl();
	obj->f = f;
	addCallback( obj );
}
