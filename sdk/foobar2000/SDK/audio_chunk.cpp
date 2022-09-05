#include "foobar2000.h"
#include "mem_block_container.h"
#include "audio_chunk.h"

void audio_chunk::set_data(const audio_sample * src,t_size samples,unsigned nch,unsigned srate,unsigned channel_config)
{
	t_size size = samples * nch;
	set_data_size(size);
	if (src)
		pfc::memcpy_t(get_data(),src,size);
	else
		pfc::memset_t(get_data(),(audio_sample)0,size);
	set_sample_count(samples);
	set_channels(nch,channel_config);
	set_srate(srate);
}

inline bool check_exclusive(unsigned val, unsigned mask)
{
	return (val&mask)!=0 && (val&mask)!=mask;
}

static void _import8u(uint8_t const * in, audio_sample * out, size_t count) {
	for(size_t walk = 0; walk < count; ++walk) {
		uint32_t i = *(in++);
		i -= 0x80; // to signed
		*(out++) = (audio_sample) (int32_t) i / (float) 0x80;
	}
}

static void _import8s(uint8_t const * in, audio_sample * out, size_t count) {
	for(size_t walk = 0; walk < count; ++walk) {
		int32_t i = (int8_t) *(in++);
		*(out++) = (audio_sample) i / (float) 0x80;
	}
}

static audio_sample _import24s(uint32_t i) {
	i ^= 0x800000; // to unsigned
	i -= 0x800000; // and back to signed / fill MSBs proper
	return (audio_sample) (int32_t) i / (audio_sample) 0x800000;
}

static void _import24(const void * in_, audio_sample * out, size_t count) {
	const uint8_t * in = (const uint8_t*) in_;
#if 1
	while(count > 0 && !pfc::is_ptr_aligned_t<4>(in)) {
		uint32_t i = *(in++);
		i |= (uint32_t) *(in++) << 8;
		i |= (uint32_t) *(in++) << 16;
		*(out++) = _import24s(i);
		--count;
	}
	{
		for(size_t loop = count >> 2; loop; --loop) {
			uint32_t i1 = * (uint32_t*) in; in += 4;
			uint32_t i2 = * (uint32_t*) in; in += 4;
			uint32_t i3 = * (uint32_t*) in; in += 4;
			*out++ = _import24s( i1 & 0xFFFFFF );
			*out++ = _import24s( (i1 >> 24) | ((i2 & 0xFFFF) << 8) );
			*out++ = _import24s( (i2 >> 16) | ((i3 & 0xFF) << 16) );
			*out++ = _import24s( i3 >> 8 );
		}
		count &= 3;
	}
	for( ; count ; --count) {
		uint32_t i = *(in++);
		i |= (uint32_t) *(in++) << 8;
		i |= (uint32_t) *(in++) << 16;
		*(out++) = _import24s(i);
	}
#else
	if (count > 0) {
		int32_t i = *(in++);
		i |= (int32_t) *(in++) << 8;
		i |= (int32_t) (int8_t) *in << 16;
		*out++ = (audio_sample) i / (audio_sample) 0x800000;
		--count;

		// Now we have in ptr at offset_of_next - 1 and we can read as int32 then discard the LSBs
		for(;count;--count) {
			int32_t i = *(  int32_t*) in; in += 3;
			*out++ = (audio_sample) (i >> 8) / (audio_sample) 0x800000;
		}
	}
#endif
}

template<bool byteSwap, bool isSigned> static void _import16any(const void * in, audio_sample * out, size_t count) {
	uint16_t const * inPtr = (uint16_t const*) in;
	const audio_sample factor = 1.0f / (audio_sample) 0x8000;
	for(size_t walk = 0; walk < count; ++walk) {
		uint16_t v = *inPtr++;
		if (byteSwap) v = pfc::byteswap_t(v);
		if (!isSigned) v ^= 0x8000; // to signed
		*out++ = (audio_sample) (int16_t) v * factor;
	}
}

template<bool byteSwap, bool isSigned> static void _import32any(const void * in, audio_sample * out, size_t count) {
	uint32_t const * inPtr = (uint32_t const*) in;
	const audio_sample factor = 1.0f / (audio_sample) 0x80000000ul;
	for(size_t walk = 0; walk < count; ++walk) {
		uint32_t v = *inPtr++;
		if (byteSwap) v = pfc::byteswap_t(v);
		if (!isSigned) v ^= 0x80000000u; // to signed
		*out++ = (audio_sample) (int32_t) v * factor;
	}
}

template<bool byteSwap, bool isSigned> static void _import24any(const void * in, audio_sample * out, size_t count) {
	uint8_t const * inPtr = (uint8_t const*) in;
	const audio_sample factor = 1.0f / (audio_sample) 0x800000;
	for(size_t walk = 0; walk < count; ++walk) {
		uint32_t v;
		if (byteSwap) v = (uint32_t) inPtr[2] | ( (uint32_t) inPtr[1] << 8 ) | ( (uint32_t) inPtr[0] << 16 );
		else v = (uint32_t) inPtr[0] | ( (uint32_t) inPtr[1] << 8 ) | ( (uint32_t) inPtr[2] << 16 );
		inPtr += 3;
		if (isSigned) v ^= 0x800000; // to unsigned
		v -= 0x800000; // then subtract to get proper MSBs
		*out++ = (audio_sample) (int32_t) v * factor;
	}
}

void audio_chunk::set_data_fixedpoint_ex(const void * source,t_size size,unsigned srate,unsigned nch,unsigned bps,unsigned flags,unsigned p_channel_config)
{
	PFC_ASSERT( check_exclusive(flags,FLAG_SIGNED|FLAG_UNSIGNED) );
	PFC_ASSERT( check_exclusive(flags,FLAG_LITTLE_ENDIAN|FLAG_BIG_ENDIAN) );

	bool byteSwap = !!(flags & FLAG_BIG_ENDIAN);
	if (pfc::byte_order_is_big_endian) byteSwap = !byteSwap;

	t_size count = size / (bps/8);
	set_data_size(count);
	audio_sample * buffer = get_data();
	bool isSigned = !!(flags & FLAG_SIGNED);

	switch(bps)
	{
	case 8:
		// byte order irrelevant
		if (isSigned) _import8s( (const uint8_t*) source , buffer, count);
		else _import8u( (const uint8_t*) source , buffer, count);
		break;
	case 16:
		if (byteSwap) {
			if (isSigned) {
				_import16any<true, true>( source, buffer, count );
			} else {
				_import16any<true, false>( source, buffer, count );
			}
		} else {
			if (isSigned) {
				//_import16any<false, true>( source, buffer, count );
				audio_math::convert_from_int16((const int16_t*)source,count,buffer,1.0);
			} else {
				_import16any<false, false>( source, buffer, count);
			}
		}
		break;
	case 24:
		if (byteSwap) {
			if (isSigned) {
				_import24any<true, true>( source, buffer, count );
			} else {
				_import24any<true, false>( source, buffer, count );
			}
		} else {
			if (isSigned) {
				//_import24any<false, true>( source, buffer, count);
				_import24( source, buffer, count);
			} else {
				_import24any<false, false>( source, buffer, count);
			}
		}
		break;
	case 32:
		if (byteSwap) {
			if (isSigned) {
				_import32any<true, true>( source, buffer, count );
			} else {
				_import32any<true, false>( source, buffer, count );
			}
		} else {
			if (isSigned) {
				audio_math::convert_from_int32((const int32_t*)source,count,buffer,1.0);
			} else {
				_import32any<false, false>( source, buffer, count);
			}
		}
		break;
	default:
		//unknown size, cant convert
		pfc::memset_t(buffer,(audio_sample)0,count);
		break;
	}
	set_sample_count(count/nch);
	set_srate(srate);
	set_channels(nch,p_channel_config);
}

void audio_chunk::set_data_fixedpoint_ms(const void * ptr, size_t bytes, unsigned sampleRate, unsigned channels, unsigned bps, unsigned channelConfig) {
	//set_data_fixedpoint_ex(ptr,bytes,sampleRate,channels,bps,(bps==8 ? FLAG_UNSIGNED : FLAG_SIGNED) | flags_autoendian(), channelConfig);
	PFC_ASSERT( bps != 0 );
	size_t count = bytes / (bps/8);
	this->set_data_size( count );
	audio_sample * buffer = this->get_data();
	switch(bps) {
	case 8:
		_import8u((const uint8_t*)ptr, buffer, count);
		break;
	case 16:
		audio_math::convert_from_int16((const int16_t*) ptr, count, buffer, 1.0);
		break;
	case 24:
		_import24( ptr, buffer, count);
		break;
	case 32:
		audio_math::convert_from_int32((const int32_t*) ptr, count, buffer, 1.0);
		break;
	default:
		PFC_ASSERT(!"Unknown bit depth!");
		memset(buffer, 0, sizeof(audio_sample) * count);
		break;
	}
	set_sample_count(count/channels);
	set_srate(sampleRate);
	set_channels(channels,channelConfig);
}

void audio_chunk::set_data_fixedpoint_signed(const void * ptr,t_size bytes,unsigned sampleRate,unsigned channels,unsigned bps,unsigned channelConfig) {
	PFC_ASSERT( bps != 0 );
	size_t count = bytes / (bps/8);
	this->set_data_size( count );
	audio_sample * buffer = this->get_data();
	switch(bps) {
	case 8:
		_import8s((const uint8_t*)ptr, buffer, count);
		break;
	case 16:
		audio_math::convert_from_int16((const int16_t*) ptr, count, buffer, 1.0);
		break;
	case 24:
		_import24( ptr, buffer, count);
		break;
	case 32:
		audio_math::convert_from_int32((const int32_t*) ptr, count, buffer, 1.0);
		break;
	default:
		PFC_ASSERT(!"Unknown bit depth!");
		memset(buffer, 0, sizeof(audio_sample) * count);
		break;
	}
	set_sample_count(count/channels);
	set_srate(sampleRate);
	set_channels(channels,channelConfig);
}

void audio_chunk::set_data_int16(const int16_t * src,t_size samples,unsigned nch,unsigned srate,unsigned channel_config) {
	const size_t count = samples * nch;
	this->set_data_size( count );
	audio_sample * buffer = this->get_data();
	audio_math::convert_from_int16(src, count, buffer, 1.0);
	set_sample_count(samples);
	set_srate(srate);
	set_channels(nch,channel_config);
}

template<class t_float>
static void process_float_multi(audio_sample * p_out,const t_float * p_in,const t_size p_count)
{
	audio_math::convert(p_in, p_out, p_count);
}

template<class t_float>
static void process_float_multi_swap(audio_sample * p_out,const t_float * p_in,const t_size p_count)
{
	for(size_t n=0;n<p_count;n++) {
		p_out[n] = (audio_sample) pfc::byteswap_t(p_in[n]);
	}
}

void audio_chunk::set_data_32(const float* src, t_size samples, unsigned nch, unsigned srate) { 
#if audio_sample_size == 32
	set_data(src, samples, nch, srate);
#else
	t_size size = samples * nch;
	set_data_size(size);
	if (src)
		audio_math::convert(src, get_data(), size);
	else
		pfc::memset_t(get_data(), (audio_sample)0, size);
	set_sample_count(samples);
	set_channels(nch);
	set_srate(srate);
#endif
}

void audio_chunk::set_data_floatingpoint_ex(const void * ptr,t_size size,unsigned srate,unsigned nch,unsigned bps,unsigned flags,unsigned p_channel_config)
{
	PFC_ASSERT(bps==32 || bps==64 || bps == 16 || bps == 24);
	PFC_ASSERT( check_exclusive(flags,FLAG_LITTLE_ENDIAN|FLAG_BIG_ENDIAN) );
	PFC_ASSERT( ! (flags & (FLAG_SIGNED|FLAG_UNSIGNED) ) );

	bool use_swap = pfc::byte_order_is_big_endian ? !!(flags & FLAG_LITTLE_ENDIAN) : !!(flags & FLAG_BIG_ENDIAN);

	const t_size count = size / (bps/8);
	set_data_size(count);
	audio_sample * out = get_data();

	if (bps == 32)
	{
		if (use_swap)
			process_float_multi_swap(out,reinterpret_cast<const float*>(ptr),count);
		else
			process_float_multi(out,reinterpret_cast<const float*>(ptr),count);
	}
	else if (bps == 64)
	{
		if (use_swap)
			process_float_multi_swap(out,reinterpret_cast<const double*>(ptr),count);
		else
			process_float_multi(out,reinterpret_cast<const double*>(ptr),count);
	} else if (bps == 16) {
		const uint16_t * in = reinterpret_cast<const uint16_t*>(ptr);
		if (use_swap) {
			for(size_t walk = 0; walk < count; ++walk) out[walk] = audio_math::decodeFloat16(pfc::byteswap_t(in[walk]));
		} else {
			for(size_t walk = 0; walk < count; ++walk) out[walk] = audio_math::decodeFloat16(in[walk]);
		}
	} else if (bps == 24) {
		const uint8_t * in = reinterpret_cast<const uint8_t*>(ptr);
		if (use_swap) {
			for(size_t walk = 0; walk < count; ++walk) out[walk] = audio_math::decodeFloat24ptrbs(&in[walk*3]);
		} else {
			for(size_t walk = 0; walk < count; ++walk) out[walk] = audio_math::decodeFloat24ptr(&in[walk*3]);
		}
	} else pfc::throw_exception_with_message< exception_io_data >("invalid bit depth");

	set_sample_count(count/nch);
	set_srate(srate);
	set_channels(nch,p_channel_config);
}

pfc::string8 audio_chunk::formatChunkSpec() const {
	pfc::string8 msg;
	msg << get_sample_rate() << " Hz, " << get_channels() << ":0x" << pfc::format_hex(get_channel_config(), 2) << " channels, " << get_sample_count() << " samples";
	return msg;
}

void audio_chunk::debugChunkSpec() const {
	FB2K_DebugLog() << "Chunk: " << this->formatChunkSpec();
}

#if PFC_DEBUG
void audio_chunk::assert_valid(const char * ctx) const {
	if (!is_valid()) {
		FB2K_DebugLog() << "audio_chunk::assert_valid failure in " << ctx;
		debugChunkSpec();
		uBugCheck();
	}
}
#endif
bool audio_chunk::is_valid() const
{
	unsigned nch = get_channels();
	if (nch == 0 || nch > 32) return false;
	if (!g_is_valid_sample_rate(get_srate())) return false;
	t_size samples = get_sample_count();
	if (samples==0 || samples >= 0x80000000ul / (sizeof(audio_sample) * nch) ) return false;
	t_size size = get_data_size();
	if (samples * nch > size) return false;
	if (!get_data()) return false;
	return true;
}

bool audio_chunk::is_spec_valid() const {
	return this->get_spec().is_valid();
}

void audio_chunk::pad_with_silence_ex(t_size samples,unsigned hint_nch,unsigned hint_srate) {
	if (is_empty())
	{
		if (hint_srate && hint_nch) {
			return set_data(0,samples,hint_nch,hint_srate);
		} else throw exception_io_data();
	}
	else
	{
		if (hint_srate && hint_srate != get_srate()) samples = MulDiv_Size(samples,get_srate(),hint_srate);
		if (samples > get_sample_count())
		{
			t_size old_size = get_sample_count() * get_channels();
			t_size new_size = samples * get_channels();
			set_data_size(new_size);
			pfc::memset_t(get_data() + old_size,(audio_sample)0,new_size - old_size);
			set_sample_count(samples);
		}
	}
}

void audio_chunk::pad_with_silence(t_size samples) {
	if (samples > get_sample_count())
	{
		t_size old_size = get_sample_count() * get_channels();
		t_size new_size = pfc::multiply_guarded(samples,(size_t)get_channels());
		set_data_size(new_size);
		pfc::memset_t(get_data() + old_size,(audio_sample)0,new_size - old_size);
		set_sample_count(samples);
	}
}

void audio_chunk::set_silence(t_size samples) {
	t_size items = samples * get_channels();
	set_data_size(items);
	pfc::memset_null_t(get_data(), items);
	set_sample_count(samples);
}

void audio_chunk::set_silence_seconds( double seconds ) {
	set_silence( (size_t) audio_math::time_to_samples( seconds, this->get_sample_rate() ) ); 
}

void audio_chunk::insert_silence_fromstart(t_size samples) {
	t_size old_size = get_sample_count() * get_channels();
	t_size delta = samples * get_channels();
	t_size new_size = old_size + delta;
	set_data_size(new_size);
	audio_sample * ptr = get_data();
	pfc::memmove_t(ptr+delta,ptr,old_size);
	pfc::memset_t(ptr,(audio_sample)0,delta);
	set_sample_count(get_sample_count() + samples);
}

bool audio_chunk::process_skip(double & skipDuration) {
	t_uint64 skipSamples = audio_math::time_to_samples(skipDuration, get_sample_rate());
	if (skipSamples == 0) {skipDuration = 0; return true;}
	const t_size mySamples = get_sample_count();
	if (skipSamples < mySamples) {
		skip_first_samples((t_size)skipSamples); 
		skipDuration = 0;
		return true;
	}
	if (skipSamples == mySamples) {
		skipDuration = 0;
		return false;
	}
	skipDuration -= audio_math::samples_to_time(mySamples, get_sample_rate());
	return false;
}

t_size audio_chunk::skip_first_samples(t_size samples_delta)
{
	t_size samples_old = get_sample_count();
	if (samples_delta >= samples_old)
	{
		set_sample_count(0);
		set_data_size(0);
		return samples_old;
	}
	else
	{
		t_size samples_new = samples_old - samples_delta;
		unsigned nch = get_channels();
		audio_sample * ptr = get_data();
		pfc::memmove_t(ptr,ptr+nch*samples_delta,nch*samples_new);
		set_sample_count(samples_new);
		set_data_size(nch*samples_new);
		return samples_delta;
	}
}

audio_sample audio_chunk::get_peak(audio_sample p_peak) const {
	return pfc::max_t(p_peak, get_peak());
}

audio_sample audio_chunk::get_peak() const {
	return audio_math::calculate_peak(get_data(),get_sample_count() * get_channels());
}

void audio_chunk::scale(audio_sample p_value)
{
	audio_sample * ptr = get_data();
	audio_math::scale(ptr,get_sample_count() * get_channels(),ptr,p_value);
}


namespace {

struct sampleToIntDesc {
	unsigned bps, bpsValid;
	bool useUpperBits;
	audio_sample scale;
};
template<typename int_t> class sampleToInt {
public:
	sampleToInt(sampleToIntDesc const & d) {
		clipLo = - ( (int_t) 1 << (d.bpsValid-1));
		clipHi = ( (int_t) 1 << (d.bpsValid-1)) - 1;
		scale = (float) ( (int64_t) 1 << (d.bpsValid - 1) ) * d.scale;
		if (d.useUpperBits) {
			shift = d.bps - d.bpsValid;
		} else {
			shift = 0;
		}
	}
	inline int_t operator() (audio_sample s) const {
		int_t v;
		if (sizeof(int_t) > 4) v = (int_t) audio_math::rint64( s * scale );
		else v = (int_t)audio_math::rint32( s * scale );
		return pfc::clip_t<int_t>( v, clipLo, clipHi) << shift;
	}
private:
	int_t clipLo, clipHi;
	int8_t shift;
	audio_sample scale;
};
}
static void render_24bit(const audio_sample * in, t_size inLen, void * out, sampleToIntDesc const & d) {
	t_uint8 * outWalk = reinterpret_cast<t_uint8*>(out);
	sampleToInt<int32_t> gen(d);
	for(t_size walk = 0; walk < inLen; ++walk) {
		int32_t v = gen(in[walk]);
		*(outWalk ++) = (t_uint8) (v & 0xFF);
		*(outWalk ++) = (t_uint8) ((v >> 8) & 0xFF);
		*(outWalk ++) = (t_uint8) ((v >> 16) & 0xFF);
	}
}
static void render_8bit(const audio_sample * in, t_size inLen, void * out, sampleToIntDesc const & d) {
	sampleToInt<int32_t> gen(d);
	t_int8 * outWalk = reinterpret_cast<t_int8*>(out);
	for(t_size walk = 0; walk < inLen; ++walk) {
		*outWalk++ = (t_int8)gen(in[walk]);
	}
}
static void render_16bit(const audio_sample * in, t_size inLen, void * out, sampleToIntDesc const & d) {
	sampleToInt<int32_t> gen(d);
	int16_t * outWalk = reinterpret_cast<int16_t*>(out);
	for(t_size walk = 0; walk < inLen; ++walk) {
		*outWalk++ = (int16_t)gen(in[walk]);
	}
}

template<typename internal_t>
static void render_32bit_(const audio_sample * in, t_size inLen, void * out, sampleToIntDesc const & d) {
	sampleToInt<internal_t> gen(d); // must use int64 for clipping
	int32_t * outWalk = reinterpret_cast<int32_t*>(out);
	for(t_size walk = 0; walk < inLen; ++walk) {
		*outWalk++ = (int32_t)gen(in[walk]);
	}
}

bool audio_chunk::g_toFixedPoint(const audio_sample * in, void * out, size_t count, uint32_t bps, uint32_t bpsValid, bool useUpperBits, audio_sample scale) {
	const sampleToIntDesc d = {bps, bpsValid, useUpperBits, scale};
	if (bps == 0) {
		PFC_ASSERT(!"How did we get here?");
		return false;
	} else if (bps <= 8) {
		render_8bit(in, count, out, d);
	} else if (bps <= 16) {
		render_16bit(in, count, out, d);
	} else if (bps <= 24) {
		render_24bit(in, count, out, d);
	} else if (bps <= 32) {
		if (bpsValid <= 28) { // for speed
			render_32bit_<int32_t>(in, count, out, d);
		} else {
			render_32bit_<int64_t>(in, count, out, d);
		}
	} else {
		PFC_ASSERT(!"How did we get here?");
		return false;
	}

	return true;
}

bool audio_chunk::toFixedPoint(class mem_block_container & out, uint32_t bps, uint32_t bpsValid, bool useUpperBits, audio_sample scale) const {
	bps = (bps + 7) & ~7;
	if (bps < bpsValid) return false;
	const size_t count = get_sample_count() * get_channel_count();
	out.set_size( count * (bps/8) );
	return g_toFixedPoint(get_data(), out.get_ptr(), count, bps, bpsValid, useUpperBits, scale);
}

bool audio_chunk::to_raw_data(mem_block_container & out, t_uint32 bps, bool useUpperBits, audio_sample scale) const {
	uint32_t bpsValid = bps;
	bps = (bps + 7) & ~7;
	const size_t count = get_sample_count() * get_channel_count();
	out.set_size( count * (bps/8) );
	void * outPtr = out.get_ptr();
	audio_sample const * inPtr = get_data();
	if (bps == 32) {
		float * f = (float*) outPtr;
		audio_math::convert(inPtr, f, count, scale);
		return true;
	} else {
		return g_toFixedPoint(inPtr, outPtr, count, bps, bpsValid, useUpperBits, scale);
	}
}

audio_chunk::spec_t audio_chunk::makeSpec(uint32_t rate, uint32_t channels) {
	return makeSpec( rate, channels, g_guess_channel_config(channels) );
}

audio_chunk::spec_t audio_chunk::makeSpec(uint32_t rate, uint32_t channels, uint32_t mask) {
	spec_t spec = {};
	spec.sampleRate = rate; spec.chanCount = channels; spec.chanMask = mask;
	return spec;
}

bool audio_chunk::spec_t::equals( const spec_t & v1, const spec_t & v2 ) {
	return v1.sampleRate == v2.sampleRate && v1.chanCount == v2.chanCount && v1.chanMask == v2.chanMask;
}

pfc::string8 audio_chunk::spec_t::toString(const char * delim) const {
	pfc::string_formatter temp;
	if ( sampleRate > 0 ) temp << sampleRate << "Hz";
	if (chanCount > 0) {
		if ( temp.length() > 0 ) temp << delim;
		temp << chanCount << "ch";
	}

	if ( chanMask != audio_chunk::channel_config_mono && chanMask != audio_chunk::channel_config_stereo ) {
		pfc::string8 strMask;
		audio_chunk::g_formatChannelMaskDesc( chanMask, strMask );
		if ( temp.length() > 0) temp << delim;
		temp << strMask;
	}		
	return temp;
}

audio_chunk::spec_t audio_chunk::get_spec() const {
	spec_t spec = {};
	spec.sampleRate = this->get_sample_rate();
	spec.chanCount = this->get_channel_count();
	spec.chanMask = this->get_channel_config();
	return spec;
}
void audio_chunk::set_spec(const spec_t & spec) {
	set_sample_rate(spec.sampleRate);
	set_channels( spec.chanCount, spec.chanMask );
}

bool audio_chunk::spec_t::is_valid() const {
    if (this->chanCount==0 || this->chanCount>256) return false;
    if (!audio_chunk::g_is_valid_sample_rate(this->sampleRate)) return false;
    return true;
}

#ifdef _WIN32

WAVEFORMATEX audio_chunk::spec_t::toWFX() const {
	const uint32_t sampleWidth = sizeof(audio_sample);

	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	wfx.nChannels = chanCount;
	wfx.nSamplesPerSec = sampleRate;
	wfx.nAvgBytesPerSec = sampleRate * chanCount * sampleWidth;
	wfx.nBlockAlign = chanCount * sampleWidth;
	wfx.wBitsPerSample = sampleWidth * 8;
	return wfx;
}

WAVEFORMATEXTENSIBLE audio_chunk::spec_t::toWFXEX() const {
	const uint32_t sampleWidth = sizeof(audio_sample);
	const bool isFloat = true;

	WAVEFORMATEXTENSIBLE wfxe;
	wfxe.Format = toWFX();
	wfxe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wfxe.Format.cbSize = sizeof(wfxe) - sizeof(wfxe.Format);
	wfxe.Samples.wValidBitsPerSample = sampleWidth * 8;
	wfxe.dwChannelMask = audio_chunk::g_channel_config_to_wfx(this->chanMask);
	wfxe.SubFormat = isFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
	
	return wfxe;
}

WAVEFORMATEX audio_chunk::spec_t::toWFXWithBPS(uint32_t bps) const {
	const uint32_t sampleWidth = (bps+7)/8;

	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = chanCount;
	wfx.nSamplesPerSec = sampleRate;
	wfx.nAvgBytesPerSec = sampleRate * chanCount * sampleWidth;
	wfx.nBlockAlign = chanCount * sampleWidth;
	wfx.wBitsPerSample = sampleWidth * 8;
	return wfx;
}

WAVEFORMATEXTENSIBLE audio_chunk::spec_t::toWFXEXWithBPS(uint32_t bps) const {
	const uint32_t sampleWidth = (bps + 7) / 8;
	const bool isFloat = false;

	WAVEFORMATEXTENSIBLE wfxe;
	wfxe.Format = toWFXWithBPS(bps);
	wfxe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wfxe.Format.cbSize = sizeof(wfxe) - sizeof(wfxe.Format);
	wfxe.Samples.wValidBitsPerSample = sampleWidth * 8;
	wfxe.dwChannelMask = audio_chunk::g_channel_config_to_wfx(this->chanMask);
	wfxe.SubFormat = isFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;

	return wfxe;
}
#endif // _WIN32

void audio_chunk::append(const audio_chunk& other) {
	if (other.get_spec() != this->get_spec()) {
		throw pfc::exception_invalid_params();
	}

	this->grow_data_size(get_used_size() + other.get_used_size());
	audio_sample* p = this->get_data() + get_used_size();
	memcpy(p, other.get_data(), other.get_used_size() * sizeof(audio_sample));
	set_sample_count(get_sample_count() + other.get_sample_count());
}
