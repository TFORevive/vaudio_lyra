//#include "pch.h"
#include "ivoicecodec.h"
#include "iframeencoder.h"

#include "include/ghc/filesystem.hpp"
#include "lyra/lyra_config.h"
#include "lyra/lyra_encoder.h"
#include "lyra/lyra_decoder.h"

#include "lyra/model_coeffs/_models.h"

//#include <opus/include/opus.h>
//#include <opus/include/opus_custom.h>
//#pragma comment(lib, "opus.lib")
#pragma comment(lib, "lyra_config.lib")
#pragma comment(lib, "lyra_encoder.lib")
#pragma comment(lib, "lyra_decoder.lib")

#pragma comment(lib, "User32.lib")

const ghc::filesystem::path GetBinPath()
{
	CHAR result[MAX_PATH];
	DWORD length = GetModuleFileNameA(NULL, result, MAX_PATH);
	ghc::filesystem::path path{ result };
	path.remove_filename();
	return path;
}

struct opus_options
{
	int iSampleRate;
	int iRawFrameSize;
	int iPacketSize;
};

#define CHANNELS 1

opus_options g_OpusOpts[] =
{
		{44100, 256, 120},
		{22050, 120, 60},
		{22050, 256, 60},
		//{22050, 320, 16}, // only this mode is used in TFO, values are broken pls fix
		//{8000, 320, 16}, // only this mode is used in TFO, values are broken pls fix
		//{22050, 512, 64}, // only this mode is used in TFO, values are broken pls fix
		//{8000, 160, 20}
};

class VoiceEncoder_Lyra : public IFrameEncoder
{
public:
	VoiceEncoder_Lyra();
	virtual ~VoiceEncoder_Lyra();

	// Interfaces IFrameDecoder

	bool Init(int quality, int samplerate, int& rawFrameSize, int& encodedFrameSize);
	void Release();
	void EncodeFrame(const char* pUncompressed, char* pCompressed);
	void DecodeFrame(const char* pCompressed, char* pDecompressed);
	bool ResetState() { return true; };

private:

	//bool	InitStates();
	//void	TermStates();

	//OpusCustomEncoder* m_EncoderState;	// Celt internal encoder state
	//OpusCustomDecoder* m_DecoderState; // Celt internal decoder state
	//OpusCustomMode* m_Mode;
	std::unique_ptr<chromemedia::codec::LyraEncoder> m_Encoder;
	std::unique_ptr<chromemedia::codec::LyraDecoder> m_Decoder;
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

VoiceEncoder_Lyra::VoiceEncoder_Lyra()
{
	//m_EncoderState = NULL;
	//m_DecoderState = NULL;
	//m_Mode = NULL;
	//m_iVersion = 0;
}

VoiceEncoder_Lyra::~VoiceEncoder_Lyra()
{
	//TermStates();
}

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

	const int sample_rate_hz = 16000;
	const int bitrate = 3200; // 3.2 Kb/s = 3200 b/s = 400 B/s

	const chromemedia::codec::LyraModels models{
		{ reinterpret_cast<const char*>(lyra_config_proto), lyra_config_proto_len },
		{ reinterpret_cast<const char*>(lyragan), lyragan_len },
		{ reinterpret_cast<const char*>(quantizer), quantizer_len },
		{ reinterpret_cast<const char*>(soundstream_encoder), soundstream_encoder_len },
	};

	m_Encoder = chromemedia::codec::LyraEncoder::Create(/*sample_rate_hz=*/sample_rate_hz, // <=kInternalSampleRateHz (using Lyra's native sample rate to avoid using resampler...)
		/*num_channels=*/1,
		/*bitrate=*/bitrate,
		/*enable_dtx=*/false,
		/*models=*/models);

	if (m_Encoder == nullptr)
	{
		MessageBoxA(0, "Could not create lyra encoder.\n\nEnsure the folder \"lyra_model\" exists under the \"Bin\" folder along with its contents.", "Lyra init error", MB_ICONERROR);
		return false;
	}

	m_Decoder = chromemedia::codec::LyraDecoder::Create(sample_rate_hz, 1, models);

	if (m_Decoder == nullptr)
	{
		MessageBoxA(0, "Could not create lyra decoder.\n\nEnsure the folder \"lyra_model\" exists under the \"Bin\" folder along with its contents.", "Lyra init error", MB_ICONERROR);
		return false;
	}

	const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();

	rawFrameSize = num_samples_per_packet * BYTES_PER_SAMPLE;
	//encodedFrameSize = m_Encoder->bitrate() / 8 / m_Encoder->frame_rate();
	encodedFrameSize = chromemedia::codec::BitrateToPacketSize(m_Encoder->bitrate());

	return true;
}

void VoiceEncoder_Lyra::Release()
{
	delete this;
}

//void VoiceEncoder_Lyra::EncodeFrame(const char* pUncompressedBytes, int maxUncompressedBytes, char* pCompressed, int maxCompressedBytes)
void VoiceEncoder_Lyra::EncodeFrame(const char* pUncompressed, char* pCompressed)
{
	unsigned char output[1024];

	const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();
	const int raw_frame_size = num_samples_per_packet * BYTES_PER_SAMPLE;

	auto encoded = m_Encoder->Encode(absl::MakeConstSpan(
		reinterpret_cast<const int16_t*>(pUncompressed),
		//std::min(num_samples_per_packet, maxUncompressedBytes)
		num_samples_per_packet
	));

	if (!encoded.has_value())
	{
		MessageBoxA(0, "Could not encode Lyra frame.", "Lyra error", MB_ICONERROR);
		return;
	}

	//opus_custom_encode(m_EncoderState, (opus_int16*)pUncompressedBytes, g_OpusOpts[m_iVersion].iRawFrameSize, output, g_OpusOpts[m_iVersion].iPacketSize);

	//auto* pCompressedMax = pCompressed + maxCompressedBytes;
	for (auto& c : *encoded)
	{
		*pCompressed = (char)c;
		pCompressed++;
		//if (pCompressed == pCompressedMax)
		//	break;
	}
}

void VoiceEncoder_Lyra::DecodeFrame(const char* pCompressed, char* pDecompressed)
{
	const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();
	const int packet_size = chromemedia::codec::BitrateToPacketSize(m_Encoder->bitrate());

	m_Decoder->SetEncodedPacket(absl::MakeConstSpan(
		reinterpret_cast<const uint8_t*>(pCompressed),
		packet_size
	));

	auto decoded = m_Decoder->DecodeSamples(num_samples_per_packet);

	if (!decoded.has_value())
	{
		MessageBoxA(0, "Could not decode Lyra frame.", "Lyra error", MB_ICONERROR);
		return;
	}

	for (auto& c : *decoded)
	{
		*pDecompressed = (char)c;
		pDecompressed++;
	}
}

/*bool VoiceEncoder_Lyra::ResetState()
{
	opus_custom_encoder_ctl(m_EncoderState, OPUS_RESET_STATE);
	opus_custom_decoder_ctl(m_DecoderState, OPUS_RESET_STATE);

	return true;
}

bool VoiceEncoder_Lyra::InitStates()
{
	if (!m_EncoderState || !m_DecoderState)
		return false;

	opus_custom_encoder_ctl(m_EncoderState, OPUS_RESET_STATE);
	opus_custom_decoder_ctl(m_DecoderState, OPUS_RESET_STATE);

	return true;
}

void VoiceEncoder_Lyra::TermStates()
{
	if (m_EncoderState)
	{
		opus_custom_encoder_destroy(m_EncoderState);
		m_EncoderState = NULL;
	}

	if (m_DecoderState)
	{
		opus_custom_decoder_destroy(m_DecoderState);
		m_DecoderState = NULL;
	}

	opus_custom_mode_destroy(m_Mode);
}*/

#define EXPORT __declspec(dllexport)
#define CALLTYPE STDAPICALLTYPE

extern "C" EXPORT void* CALLTYPE CreateInterface(const char* pName, int* pReturnCode)
{
	IFrameEncoder* pEncoder = new VoiceEncoder_Lyra;
	return CreateVoiceCodec_Frame(pEncoder);
}
