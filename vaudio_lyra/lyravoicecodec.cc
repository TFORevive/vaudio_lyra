#include "ivoicecodec.h"
#include "iframeencoder.h"

#include "include/ghc/filesystem.hpp"
#include "lyra/lyra_config.h"
#include "lyra/lyra_encoder.h"
#include "lyra/lyra_decoder.h"

#include "lyra/model_coeffs/_models.h"

//#pragma comment(lib, "lyra_config.lib")
//#pragma comment(lib, "lyra_encoder.lib")
//#pragma comment(lib, "lyra_decoder.lib")

#pragma comment(lib, "User32.lib")

class VoiceEncoder_Lyra : public IFrameEncoder
{
public:
	VoiceEncoder_Lyra() {};
	virtual ~VoiceEncoder_Lyra() {};

	// Interfaces IFrameDecoder

	bool Init(int quality, int samplerate, int& rawFrameSize, int& encodedFrameSize);
	void Release() { delete this; };
	void EncodeFrame(const absl::Span<const int16_t> uncompressed, const absl::Span<uint8_t> compressed);
	void DecodeFrame(const absl::Span<const uint8_t> compressed, const absl::Span<int16_t> uncompressed);
	bool ResetState() { return true; };

private:
	std::unique_ptr<chromemedia::codec::LyraEncoder> m_Encoder;
	std::unique_ptr<chromemedia::codec::LyraDecoder> m_Decoder;
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

bool VoiceEncoder_Lyra::Init(int quality, int samplerate, int& rawFrameSize, int& encodedFrameSize)
{
	assert(quality == 3);
	assert(samplerate == 16000);
	if (!chromemedia::codec::IsSampleRateSupported(samplerate))
	{
		char buf[512];
		sprintf_s(buf, "Lyra does not support the requested sample rate: %lu.", samplerate);
		MessageBoxA(0, buf, "Lyra init error", MB_ICONERROR);
		return false;
	}

	// The only supported bitrate values are 3200, 6000, 9200
	//const int bitrate = 3200; // 3.2 Kb/s = 3200 b/s = 400 B/s
	//const int bitrate = 6000; // 6 Kb/s = 6000 b/s = 750 B/s
	const int bitrate = 9200; // 9.2 Kb/s = 9200 b/s = 1150 B/s

	const chromemedia::codec::LyraModels models = GetEmbeddedLyraModels();

	m_Encoder = chromemedia::codec::LyraEncoder::Create(/*sample_rate_hz=*/samplerate, // <=kInternalSampleRateHz (using Lyra's native sample rate to avoid using resampler...)
		/*num_channels=*/1,
		/*bitrate=*/bitrate,
		/*enable_dtx=*/false,
		/*models=*/models);

	if (m_Encoder == nullptr)
	{
		MessageBoxA(0, "Could not create Lyra encoder.", "Lyra init error", MB_ICONERROR);
		return false;
	}

	m_Decoder = chromemedia::codec::LyraDecoder::Create(samplerate, /*num_channels=*/1, models);

	if (m_Decoder == nullptr)
	{
		MessageBoxA(0, "Could not create Lyra decoder.", "Lyra init error", MB_ICONERROR);
		return false;
	}

	const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();

	rawFrameSize = num_samples_per_packet * BYTES_PER_SAMPLE;
	encodedFrameSize = chromemedia::codec::BitrateToPacketSize(m_Encoder->bitrate());
	assert(rawFrameSize == 2 * 16000 / 50);
	if (bitrate == 9200) assert(encodedFrameSize == 23);

	//MessageBoxA(0, ("OK, quality:" + std::to_string(quality) + ", samplerate:" + std::to_string(samplerate)).c_str(), "Lyra init", 0);
	return true;
}

void VoiceEncoder_Lyra::EncodeFrame(const absl::Span<const int16_t> uncompressed, const absl::Span<uint8_t> compressed)
{
	const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();
	const int raw_frame_size = num_samples_per_packet * BYTES_PER_SAMPLE;

	// uncompressed buffer is set to arbitrary big size, it does not equal num_samples_per_packet,
	// but it only stores `num_samples_per_packet` amount of samples, and that's how must we must read
	assert(uncompressed.size() >= num_samples_per_packet);
	auto encoded = m_Encoder->Encode(uncompressed.first(num_samples_per_packet));

	if (!encoded.has_value())
	{
		MessageBoxA(0, "Could not encode Lyra frame.", "Lyra error", MB_ICONERROR);
		return;
	}

	assert(encoded->size() == chromemedia::codec::BitrateToPacketSize(m_Encoder->bitrate()));

	memcpy_s(compressed.data(), compressed.size(), encoded->data(), encoded->size());
}

void VoiceEncoder_Lyra::DecodeFrame(const absl::Span<const uint8_t> compressed, const absl::Span<int16_t> uncompressed)
{
	const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();
	const int packet_size = chromemedia::codec::BitrateToPacketSize(m_Encoder->bitrate());
	assert(compressed.size() == packet_size);

	bool valid = m_Decoder->SetEncodedPacket(compressed);
	if (!valid)
	{
		//MessageBoxA(0, "Invalid Lyra frame.", "Lyra error", MB_ICONERROR);
		assert(valid == true);
		return;
	}

	auto decoded = m_Decoder->DecodeSamples(num_samples_per_packet);

	if (!decoded.has_value())
	{
		//MessageBoxA(0, "Could not decode Lyra frame.", "Lyra error", MB_ICONERROR);
		assert(decoded.has_value());
		return;
	}

	assert(decoded->size() == num_samples_per_packet);

	memcpy_s(uncompressed.data(), uncompressed.size() * BYTES_PER_SAMPLE, decoded->data(), decoded->size() * BYTES_PER_SAMPLE);
}

class VoiceCodec_Uncompressed : public IVoiceCodec
{
public:
	VoiceCodec_Uncompressed() {}
	virtual ~VoiceCodec_Uncompressed() {}
	virtual bool Init(int quality, unsigned int nSamplesPerSec) { return true; }
	virtual void Release() { delete this; }
	virtual bool ResetState() { return true; }
	virtual int Compress(const char* pUncompressed, int nSamples, char* pCompressed, int maxCompressedBytes/*, bool bFinal*/)
	{
		int nCompressedBytes = nSamples * BYTES_PER_SAMPLE;
		memcpy_s(pCompressed, maxCompressedBytes, pUncompressed, nCompressedBytes);
		return nCompressedBytes;
	}
	virtual int Decompress(const char* pCompressed, int compressedBytes, char* pUncompressed, int maxUncompressedBytes)
	{
		int nDecompressedBytes = compressedBytes;
		memcpy_s(pUncompressed, maxUncompressedBytes, pCompressed, compressedBytes);
		return nDecompressedBytes / BYTES_PER_SAMPLE;
	}
};

#define EXPORT __declspec(dllexport)
#define CALLTYPE STDAPICALLTYPE

extern "C" EXPORT void* CALLTYPE CreateInterface(const char* pName, int* pReturnCode)
{
	assert(strcmp(pName, "vaudio_lyra") == 0);

	//return new VoiceCodec_Uncompressed;

	IFrameEncoder* pEncoder = new VoiceEncoder_Lyra;
	return CreateVoiceCodec_Frame(pEncoder);
}
