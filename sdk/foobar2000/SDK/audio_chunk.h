#pragma once

#ifdef _WIN32
#include <MMReg.h>
#endif
//! Thrown when audio_chunk sample rate or channel mapping changes in mid-stream and the code receiving audio_chunks can't deal with that scenario.
PFC_DECLARE_EXCEPTION(exception_unexpected_audio_format_change, exception_io_data, "Unexpected audio format change" );

//! Interface to container of a chunk of audio data. See audio_chunk_impl for an implementation.
class NOVTABLE audio_chunk {
public:

	enum {
		sample_rate_min = 1000, sample_rate_max = 20000000
	};
	static bool g_is_valid_sample_rate(t_uint32 p_val) {return p_val >= sample_rate_min && p_val <= sample_rate_max;}
	
	//! Channel map flag declarations. Note that order of interleaved channel data in the stream is same as order of these flags.
	enum
	{
		channel_front_left			= 1<<0,
		channel_front_right			= 1<<1,
		channel_front_center		= 1<<2,
		channel_lfe					= 1<<3,
		channel_back_left			= 1<<4,
		channel_back_right			= 1<<5,
		channel_front_center_left	= 1<<6,
		channel_front_center_right	= 1<<7,
		channel_back_center			= 1<<8,
		channel_side_left			= 1<<9,
		channel_side_right			= 1<<10,
		channel_top_center			= 1<<11,
		channel_top_front_left		= 1<<12,
		channel_top_front_center	= 1<<13,
		channel_top_front_right		= 1<<14,
		channel_top_back_left		= 1<<15,
		channel_top_back_center		= 1<<16,
		channel_top_back_right		= 1<<17,

		channel_config_mono = channel_front_center,
		channel_config_stereo = channel_front_left | channel_front_right,
		channel_config_4point0 = channel_front_left | channel_front_right | channel_back_left | channel_back_right,
		channel_config_5point0 = channel_front_left | channel_front_right | channel_front_center | channel_back_left | channel_back_right,
		channel_config_5point1 = channel_front_left | channel_front_right | channel_front_center | channel_lfe | channel_back_left | channel_back_right,
		channel_config_5point1_side = channel_front_left | channel_front_right | channel_front_center | channel_lfe | channel_side_left | channel_side_right,
		channel_config_7point1 = channel_config_5point1 | channel_side_left | channel_side_right,

		channels_back_left_right = channel_back_left | channel_back_right,
		channels_side_left_right = channel_side_left | channel_side_right,

		defined_channel_count = 18,
	};

	//! Helper function; guesses default channel map for the specified channel count. Returns 0 on failure.
	static unsigned g_guess_channel_config(unsigned count);
	//! Helper function; determines channel map for the specified channel count according to Xiph specs. Throws exception_io_data on failure.
	static unsigned g_guess_channel_config_xiph(unsigned count);

	//! Helper function; translates audio_chunk channel map to WAVEFORMATEXTENSIBLE channel map.
	static uint32_t g_channel_config_to_wfx(unsigned p_config);
	//! Helper function; translates WAVEFORMATEXTENSIBLE channel map to audio_chunk channel map.
	static unsigned g_channel_config_from_wfx(uint32_t p_wfx);

	//! Extracts flag describing Nth channel from specified map. Usable to figure what specific channel in a stream means.
	static unsigned g_extract_channel_flag(unsigned p_config,unsigned p_index);
	//! Counts channels specified by channel map.
	static unsigned g_count_channels(unsigned p_config);
	//! Calculates index of a channel specified by p_flag in a stream where channel map is described by p_config.
	static unsigned g_channel_index_from_flag(unsigned p_config,unsigned p_flag);

	static const char * g_channel_name(unsigned p_flag);
	static const char * g_channel_name_byidx(unsigned p_index);
	static unsigned g_find_channel_idx(unsigned p_flag);
	static void g_formatChannelMaskDesc(unsigned flags, pfc::string_base & out);
	static pfc::string8 g_formatChannelMaskDesc(unsigned flags);

	

	//! Retrieves audio data buffer pointer (non-const version). Returned pointer is for temporary use only; it is valid until next set_data_size call, or until the object is destroyed. \n
	//! Size of returned buffer is equal to get_data_size() return value (in audio_samples). Amount of actual data may be smaller, depending on sample count and channel count. Conditions where sample count * channel count are greater than data size should not be possible.
	virtual audio_sample * get_data() = 0;
	//! Retrieves audio data buffer pointer (const version). Returned pointer is for temporary use only; it is valid until next set_data_size call, or until the object is destroyed. \n
	//! Size of returned buffer is equal to get_data_size() return value (in audio_samples). Amount of actual data may be smaller, depending on sample count and channel count. Conditions where sample count * channel count are greater than data size should not be possible.
	virtual const audio_sample * get_data() const = 0;
	//! Retrieves size of allocated buffer space, in audio_samples.
	virtual t_size get_data_size() const = 0;
	//! Resizes audio data buffer to specified size. Throws std::bad_alloc on failure.
	virtual void set_data_size(t_size p_new_size) = 0;
	//! Sanity helper, same as set_data_size.
	void allocate(size_t size) { set_data_size( size ); }
	
	//! Retrieves sample rate of contained audio data.
	virtual unsigned get_srate() const = 0;
	//! Sets sample rate of contained audio data.
	virtual void set_srate(unsigned val) = 0;
	//! Retrieves channel count of contained audio data.
	virtual unsigned get_channels() const = 0;
	//! Helper - for consistency - same as get_channels().
	inline unsigned get_channel_count() const {return get_channels();}
	//! Retrieves channel map of contained audio data. Conditions where number of channels specified by channel map don't match get_channels() return value should not be possible.
	virtual unsigned get_channel_config() const = 0;
	//! Sets channel count / channel map.
	virtual void set_channels(unsigned p_count,unsigned p_config) = 0;

	//! Retrieves number of valid samples in the buffer. \n
	//! Note that a "sample" means a unit of interleaved PCM data representing states of each channel at given point of time, not a single PCM value. \n
	//! For an example, duration of contained audio data is equal to sample count / sample rate, while actual size of contained data is equal to sample count * channel count.
	virtual t_size get_sample_count() const = 0;
	
	//! Sets number of valid samples in the buffer. WARNING: sample count * channel count should never be above allocated buffer size.
	virtual void set_sample_count(t_size val) = 0;

	//! Helper, same as get_srate().
	inline unsigned get_sample_rate() const {return get_srate();}
	//! Helper, same as set_srate().
	inline void set_sample_rate(unsigned val) {set_srate(val);}

	//! Helper; sets channel count to specified value and uses default channel map for this channel count.
	void set_channels(unsigned val) {set_channels(val,g_guess_channel_config(val));}

	
	//! Helper; resizes audio data buffer when its current size is smaller than requested.
	inline void grow_data_size(t_size p_requested) {if (p_requested > get_data_size()) set_data_size(p_requested);}


	//! Retrieves duration of contained audio data, in seconds.
	inline double get_duration() const
	{
		double rv = 0;
		t_size srate = get_srate (), samples = get_sample_count();
		if (srate>0 && samples>0) rv = (double)samples/(double)srate;
		return rv;
	}
	
	//! Returns whether the chunk is empty (contains no audio data).
	inline bool is_empty() const {return get_channels()==0 || get_srate()==0 || get_sample_count()==0;}
	
	//! Returns whether the chunk contents are valid (for bug check purposes).
	bool is_valid() const;

	void debugChunkSpec() const;
	pfc::string8 formatChunkSpec() const;
#if PFC_DEBUG
	void assert_valid(const char * ctx) const;
#else
	void assert_valid(const char * ctx) const {}
#endif
	
    
    //! Returns whether the chunk contains valid sample rate & channel info (but allows an empty chunk).
    bool is_spec_valid() const;

	//! Returns actual amount of audio data contained in the buffer (sample count * channel count). Must not be greater than data size (see get_data_size()).
	size_t get_used_size() const {return get_sample_count() * get_channels();}
	//! Same as get_used_size(); old confusingly named version.
	size_t get_data_length() const {return get_sample_count() * get_channels();}
#ifdef _MSC_VER
#pragma deprecated( get_data_length )
#endif

	//! Resets all audio_chunk data.
	inline void reset() {
		set_sample_count(0);
		set_srate(0);
		set_channels(0);
		set_data_size(0);
	}
	
	//! Helper, sets chunk data to contents of specified buffer, with specified number of channels / sample rate / channel map.
	void set_data(const audio_sample * src,t_size samples,unsigned nch,unsigned srate,unsigned channel_config);
	
	//! Helper, sets chunk data to contents of specified buffer, with specified number of channels / sample rate, using default channel map for specified channel count.
	inline void set_data(const audio_sample * src,t_size samples,unsigned nch,unsigned srate) {set_data(src,samples,nch,srate,g_guess_channel_config(nch));}

	void set_data_int16(const int16_t * src,t_size samples,unsigned nch,unsigned srate,unsigned channel_config);
	
	//! Helper, sets chunk data to contents of specified buffer, using default win32/wav conventions for signed/unsigned switch.
	inline void set_data_fixedpoint(const void * ptr,t_size bytes,unsigned srate,unsigned nch,unsigned bps,unsigned channel_config) {
		this->set_data_fixedpoint_ms(ptr, bytes, srate, nch, bps, channel_config);
	}

	void set_data_fixedpoint_signed(const void * ptr,t_size bytes,unsigned srate,unsigned nch,unsigned bps,unsigned channel_config);

	enum
	{
		FLAG_LITTLE_ENDIAN = 1,
		FLAG_BIG_ENDIAN = 2,
		FLAG_SIGNED = 4,
		FLAG_UNSIGNED = 8,
	};

	inline static unsigned flags_autoendian() {
		return pfc::byte_order_is_big_endian ? FLAG_BIG_ENDIAN : FLAG_LITTLE_ENDIAN;
	}

	void set_data_fixedpoint_ex(const void * ptr,t_size bytes,unsigned p_sample_rate,unsigned p_channels,unsigned p_bits_per_sample,unsigned p_flags,unsigned p_channel_config);//p_flags - see FLAG_* above

	void set_data_fixedpoint_ms(const void * ptr, size_t bytes, unsigned sampleRate, unsigned channels, unsigned bps, unsigned channelConfig);

	void set_data_floatingpoint_ex(const void * ptr,t_size bytes,unsigned p_sample_rate,unsigned p_channels,unsigned p_bits_per_sample,unsigned p_flags,unsigned p_channel_config);//signed/unsigned flags dont apply

	inline void set_data_32(const float * src,t_size samples,unsigned nch,unsigned srate) {return set_data(src,samples,nch,srate);}

	//! Appends silent samples at the end of the chunk. \n
	//! The chunk may be empty prior to this call, its sample rate & channel count will be set to the specified values then. \n
	//! The chunk may have different sample rate than requested; silent sample count will be recalculated to the used sample rate retaining actual duration.
	//! @param samples Number of silent samples to append.
	//! @param hint_nch If no channel count is set on this chunk, it will be set to this value.
	//! @param hint_srate The sample rate of silent samples being inserted. If no sampler ate is set on this chunk, it will be set to this value.\n
	//! Otherwise if chunk's sample rate doesn't match hint_srate, sample count will be recalculated to chunk's actual sample rate.
	void pad_with_silence_ex(t_size samples,unsigned hint_nch,unsigned hint_srate);
	//! Appends silent samples at the end of the chunk. \n
	//! The chunk must have valid sample rate & channel count prior to this call.
	//! @param Number of silent samples to append.s
	void pad_with_silence(t_size samples);
	//! Inserts silence at the beginning of the audio chunk.
	//! @param Number of silent samples to insert.
	void insert_silence_fromstart(t_size samples);
	//! Helper; removes N first samples from the chunk. \n
	//! If the chunk contains fewer samples than requested, it becomes empty.
	//! @returns Number of samples actually removed.
	t_size skip_first_samples(t_size samples);
	//! Produces a chunk of silence, with the specified duration. \n
	//! Any existing audio sdata will be discarded. \n
	//! Expects sample rate and channel count to be set first. \n
	//! Also allocates memory for the requested amount of data see: set_data_size().
	//! @param samples Desired number of samples.
	void set_silence(t_size samples);
	//! Produces a chunk of silence, with the specified duration. \n
	//! Any existing audio sdata will be discarded. \n
	//! Expects sample rate and channel count to be set first. \n
	//! Also allocates memory for the requested amount of data see: set_data_size().
	//! @param samples Desired duration in seconds.
	void set_silence_seconds( double seconds );

	//! Helper; skips first samples of the chunk updating a remaining to-skip counter.
	//! @param skipDuration Reference to the duration of audio remining to be skipped, in seconds. Updated by each call.
	//! @returns False if the chunk became empty, true otherwise.
	bool process_skip(double & skipDuration);

	//! Simple function to get original PCM stream back. Assumes host's endianness, integers are signed - including the 8bit mode; 32bit mode assumed to be float.
	//! @returns false when the conversion could not be performed because of unsupported bit depth etc.
	bool to_raw_data(class mem_block_container & out, t_uint32 bps, bool useUpperBits = true, float scale = 1.0) const;

	//! Convert audio_chunk contents to fixed-point PCM format.
	//! @param useUpperBits relevant if bps != bpsValid, signals whether upper or lower bits of each sample should be used.
	bool toFixedPoint(class mem_block_container & out, uint32_t bps, uint32_t bpsValid, bool useUpperBits = true, float scale = 1.0) const;

	//! Convert a buffer of audio_samples to fixed-point PCM format.
	//! @param useUpperBits relevant if bps != bpsValid, signals whether upper or lower bits of each sample should be used.
	static bool g_toFixedPoint(const audio_sample * in, void * out, size_t count, uint32_t bps, uint32_t bpsValid, bool useUpperBits = true, float scale = 1.0);


	//! Helper, calculates peak value of data in the chunk. The optional parameter specifies initial peak value, to simplify calling code.
	audio_sample get_peak(audio_sample p_peak) const;
	audio_sample get_peak() const;

	//! Helper function; scales entire chunk content by specified value.
	void scale(audio_sample p_value);

	//! Helper; copies content of another audio chunk to this chunk.
	void copy(const audio_chunk & p_source) {
		set_data(p_source.get_data(),p_source.get_sample_count(),p_source.get_channels(),p_source.get_srate(),p_source.get_channel_config());
	}

	const audio_chunk & operator=(const audio_chunk & p_source) {
		copy(p_source);
		return *this;
	}

	struct spec_t {
		uint32_t sampleRate;
		uint32_t chanCount, chanMask;
		
		static bool equals( const spec_t & v1, const spec_t & v2 );
		bool operator==(const spec_t & other) const { return equals(*this, other);}
		bool operator!=(const spec_t & other) const { return !equals(*this, other);}
		bool is_valid() const;
		void clear() { sampleRate = 0; chanCount = 0; chanMask = 0; }

#ifdef _WIN32
		//! Creates WAVE_FORMAT_IEEE_FLOAT WAVEFORMATEX structure
		WAVEFORMATEX toWFX() const;
		//! Creates WAVE_FORMAT_IEEE_FLOAT WAVEFORMATEXTENSIBLE structure
		WAVEFORMATEXTENSIBLE toWFXEX() const;
		//! Creates WAVE_FORMAT_PCM WAVEFORMATEX structure
		WAVEFORMATEX toWFXWithBPS(uint32_t bps) const;
		//! Creates WAVE_FORMAT_PCM WAVEFORMATEXTENSIBLE structure
		WAVEFORMATEXTENSIBLE toWFXEXWithBPS(uint32_t bps) const;
#endif

		pfc::string8 toString() const;
	};
	static spec_t makeSpec(uint32_t rate, uint32_t channels);
	static spec_t makeSpec(uint32_t rate, uint32_t channels, uint32_t chanMask);
	static spec_t emptySpec() { return makeSpec(0, 0, 0); }

	spec_t get_spec() const;
	void set_spec(const spec_t &);
protected:
	audio_chunk() {}
	~audio_chunk() {}	
};

//! Implementation of audio_chunk. Takes pfc allocator template as template parameter.
template<typename container_t = pfc::mem_block_aligned_t<audio_sample, 16> >
class audio_chunk_impl_t : public audio_chunk {
	typedef audio_chunk_impl_t<container_t> t_self;
	container_t m_data;
	unsigned m_srate = 0, m_nch = 0, m_setup = 0;
	t_size m_samples = 0;
	
public:
	audio_chunk_impl_t() {}
	audio_chunk_impl_t(const audio_sample * src,unsigned samples,unsigned nch,unsigned srate) {set_data(src,samples,nch,srate);}
	audio_chunk_impl_t(const audio_chunk & p_source) {copy(p_source);}
	
	
	virtual audio_sample * get_data() {return m_data.get_ptr();}
	virtual const audio_sample * get_data() const {return m_data.get_ptr();}
	virtual t_size get_data_size() const {return m_data.get_size();}
	virtual void set_data_size(t_size new_size) {m_data.set_size(new_size);}
	
	virtual unsigned get_srate() const {return m_srate;}
	virtual void set_srate(unsigned val) {m_srate=val;}
	virtual unsigned get_channels() const {return m_nch;}
	virtual unsigned get_channel_config() const {return m_setup;}
	virtual void set_channels(unsigned val,unsigned setup) {m_nch = val;m_setup = setup;}
	void set_channels(unsigned val) {set_channels(val,g_guess_channel_config(val));}

	virtual t_size get_sample_count() const {return m_samples;}
	virtual void set_sample_count(t_size val) {m_samples = val;}

	const t_self & operator=(const audio_chunk & p_source) {copy(p_source);return *this;}
};

typedef audio_chunk_impl_t<> audio_chunk_impl;
typedef audio_chunk_impl_t<pfc::mem_block_aligned_incremental_t<audio_sample, 16> > audio_chunk_fast_impl;

//! Implements const methods of audio_chunk only, referring to an external buffer. For temporary use only (does not maintain own storage), e.g.: somefunc( audio_chunk_temp_impl(mybuffer,....) );
class audio_chunk_memref_impl : public audio_chunk {
public:
	audio_chunk_memref_impl(const audio_sample * p_data,t_size p_samples,t_uint32 p_sample_rate,t_uint32 p_channels,t_uint32 p_channel_config) :
	m_samples(p_samples), m_sample_rate(p_sample_rate), m_channels(p_channels), m_channel_config(p_channel_config), m_data(p_data)
	{
#if PFC_DEBUG
		assert_valid(__FUNCTION__);
#endif
	}

	audio_sample * get_data() {throw pfc::exception_not_implemented();}
	const audio_sample * get_data() const {return m_data;}
	t_size get_data_size() const {return m_samples * m_channels;}
	void set_data_size(t_size) {throw pfc::exception_not_implemented();}
	
	unsigned get_srate() const {return m_sample_rate;}
	void set_srate(unsigned) {throw pfc::exception_not_implemented();}
	unsigned get_channels() const {return m_channels;}
	unsigned get_channel_config() const {return m_channel_config;}
	void set_channels(unsigned,unsigned) {throw pfc::exception_not_implemented();}

	t_size get_sample_count() const {return m_samples;}
	
	void set_sample_count(t_size) {throw pfc::exception_not_implemented();}

private:
	t_size m_samples;
	t_uint32 m_sample_rate,m_channels,m_channel_config;
	const audio_sample * m_data;
};


// Compatibility typedefs.
typedef audio_chunk_fast_impl audio_chunk_impl_temporary;
typedef audio_chunk_impl audio_chunk_i;
typedef audio_chunk_memref_impl audio_chunk_temp_impl;

class audio_chunk_partial_ref : public audio_chunk_temp_impl {
public:
	audio_chunk_partial_ref(const audio_chunk & chunk, t_size base, t_size count) : audio_chunk_temp_impl(chunk.get_data() + base * chunk.get_channels(), count, chunk.get_sample_rate(), chunk.get_channels(), chunk.get_channel_config()) {}
};
