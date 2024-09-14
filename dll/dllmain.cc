// dllmain.cpp : Defines the entry point for the DLL application.
//#include "pch.h"
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include "lyra/lyra_config.h"
#include "lyra/lyra_encoder.h"
#include "lyra/lyra_decoder.h"
#include "lyra/model_coeffs/_models.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// If we wanted to do something smarter, we could allocate N of these and route packets to them so we could support N different speakers.
// Not going crazy with this right yet.
chromemedia::codec::LyraEncoder* m_Encoder = nullptr;
chromemedia::codec::LyraDecoder* m_Decoder = nullptr;

extern "C" __declspec(dllexport) bool Initialize()
{
	// The only samplerate supported is 16000 because it requires no resampling.
	const int samplerate = 16000;
	
	// The only supported bitrate values are 3200, 6000, 9200
	const int bitrate = 3200; // 3.2 Kb/s = 3200 b/s = 400 B/s
	//const int bitrate = 6000; // 6 Kb/s = 6000 b/s = 750 B/s
	//const int bitrate = 9200; // 9.2 Kb/s = 9200 b/s = 1150 B/s

	const chromemedia::codec::LyraModels models = GetEmbeddedLyraModels();

	if (m_Encoder==nullptr)
	{
		m_Encoder = chromemedia::codec::LyraEncoder::Create(/*sample_rate_hz=*/samplerate, // <=kInternalSampleRateHz (using Lyra's native sample rate to avoid using resampler...)
			/*num_channels=*/1,
			/*bitrate=*/bitrate,
			/*enable_dtx=*/false,
			/*models=*/models);
	}
	if (m_Decoder==nullptr)
	{
		m_Decoder = chromemedia::codec::LyraDecoder::Create(samplerate, /*num_channels=*/1, models);
		//const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();
		//rawFrameSize = num_samples_per_packet * BYTES_PER_SAMPLE;
		//encodedFrameSize = chromemedia::codec::BitrateToPacketSize(m_Encoder->bitrate());
		//assert(rawFrameSize == 2 * 16000 / 50);
		//if (bitrate == 9200) assert(encodedFrameSize == 23);
	}
	return m_Encoder!=nullptr && m_Decoder!=nullptr;
}

extern "C" __declspec(dllexport) void Shutdown()
{
	if (m_Encoder!=nullptr)
	{
		delete m_Encoder;
		m_Encoder = nullptr;
	}
	if (m_Decoder!=nullptr)
	{
		delete m_Decoder;
		m_Decoder = nullptr;
	}
}

extern "C" __declspec(dllexport) void Encode(const int16_t *uncompressed, size_t uncompressed_size, uint8_t *compressed, size_t compressed_size)
{
    const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();
    const int raw_frame_size = num_samples_per_packet * BYTES_PER_SAMPLE;

    // Ensure the uncompressed buffer is large enough
    assert(uncompressed_size >= num_samples_per_packet);

    // Create a vector to pass only the required samples to the encoder
    std::vector<int16_t> uncompressed_vector(uncompressed, uncompressed + num_samples_per_packet);
    std::optional<std::vector<uint8_t>> encoded = m_Encoder->Encode(uncompressed_vector);

    if (!encoded.has_value())
    {
        MessageBoxA(0, "Could not encode Lyra frame.", "Lyra error", MB_ICONERROR);
        return;
    }

    // Ensure the compressed buffer is large enough
    assert(encoded->size() == chromemedia::codec::BitrateToPacketSize(m_Encoder->bitrate()));
    assert(compressed_size >= encoded->size());

    // Copy the encoded data into the compressed buffer
    memcpy_s(compressed, compressed_size, encoded->data(), encoded->size());
}

extern "C" __declspec(dllexport) void Decode(const int8_t *compressed, size_t compressed_size, uint16_t *uncompressed, size_t uncompressed_size)
{
    const int num_samples_per_packet = m_Encoder->sample_rate_hz() / m_Encoder->frame_rate();
    const int packet_size = chromemedia::codec::BitrateToPacketSize(m_Encoder->bitrate());

    // Ensure compressed buffer is the correct size
    assert(compressed_size == packet_size);

    // Set the encoded packet in the decoder
    bool valid = m_Decoder->SetEncodedPacket(std::vector<int8_t>(compressed, compressed + compressed_size));
    if (!valid)
    {
        assert(valid == true);
        return;
    }

    // Decode the samples
    std::optional<std::vector<int16_t>> decoded = m_Decoder->DecodeSamples(num_samples_per_packet);
    if (!decoded.has_value())
    {
        assert(decoded.has_value());
        return;
    }

    // Ensure the uncompressed buffer is large enough
    assert(decoded->size() == num_samples_per_packet);
    assert(uncompressed_size >= decoded->size());

    // Copy the decoded samples to the uncompressed buffer
    memcpy_s(uncompressed, uncompressed_size * sizeof(uint16_t), decoded->data(), decoded->size() * sizeof(uint16_t));
}