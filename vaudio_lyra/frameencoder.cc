#include "ivoicecodec.h"
#include "iframeencoder.h"

#include <cassert>
#include <string.h>

#ifndef NDEBUG
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "Windows.h"
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

// VoiceCodec_Frame can be used to wrap a frame encoder for the engine. As it gets sound data, it will queue it
// until it has enough for a frame, then it will compress it. Same thing for decompression.
class VoiceCodec_Frame : public IVoiceCodec
{
public:
	enum { MAX_FRAMEBUFFER_SAMPLES = 1024 };

	VoiceCodec_Frame(IFrameEncoder* pEncoder)
	{
		m_nEncodeBufferSamples = 0;
		m_nRawBytes = m_nRawSamples = m_nEncodedBytes = 0;
		m_pFrameEncoder = pEncoder;
	}

	virtual ~VoiceCodec_Frame()
	{
		if (m_pFrameEncoder)
			m_pFrameEncoder->Release();
	}

	virtual bool Init(int quality, unsigned int nSamplesPerSec)
	{
		if (m_pFrameEncoder && m_pFrameEncoder->Init(quality, nSamplesPerSec, m_nRawBytes, m_nEncodedBytes))
		{
			m_nRawSamples = m_nRawBytes / BYTES_PER_SAMPLE;
			assert(m_nRawBytes <= MAX_FRAMEBUFFER_SAMPLES && m_nEncodedBytes <= MAX_FRAMEBUFFER_SAMPLES);
			return true;
		}
		else
		{
			if (m_pFrameEncoder)
				m_pFrameEncoder->Release();

			m_pFrameEncoder = NULL;
			return false;
		}
	}

	virtual void Release()
	{
		delete this;
	}

	virtual int Compress(const char* pUncompressedBytes, int nSamples, char* pCompressed, int maxCompressedBytes/*, bool bFinal*/)
	{
		if (!m_pFrameEncoder)
			return 0;

		const short* pUncompressed = (const short*)pUncompressedBytes;
		absl::Span compressed{ reinterpret_cast<uint8_t*>(pCompressed), static_cast<size_t>(maxCompressedBytes) };
#ifndef NDEBUG
		memset(pCompressed, 0, maxCompressedBytes); // for asserts to work
#endif

		int nCompressedBytes = 0;
		while ((nSamples + m_nEncodeBufferSamples) >= m_nRawSamples && (maxCompressedBytes - nCompressedBytes) >= m_nEncodedBytes)
		{
			// Get the data block out.
			short samples[MAX_FRAMEBUFFER_SAMPLES];
			memcpy_s(samples, MAX_FRAMEBUFFER_SAMPLES, m_EncodeBuffer, m_nEncodeBufferSamples * BYTES_PER_SAMPLE);
			memcpy_s(&samples[m_nEncodeBufferSamples], sizeof(samples) - (&samples[m_nEncodeBufferSamples] - samples),
				pUncompressed, (m_nRawSamples - m_nEncodeBufferSamples) * BYTES_PER_SAMPLE);
			nSamples -= m_nRawSamples - m_nEncodeBufferSamples;
			pUncompressed += m_nRawSamples - m_nEncodeBufferSamples;
			m_nEncodeBufferSamples = 0;

			// Compress it.
			//m_pFrameEncoder->EncodeFrame((const char*)samples, MAX_FRAMEBUFFER_SAMPLES, &pCompressed[nCompressedBytes], maxCompressedBytes);
			m_pFrameEncoder->EncodeFrame(absl::MakeConstSpan(samples), compressed.subspan(nCompressedBytes));
			nCompressedBytes += m_nEncodedBytes;

			// Ensure it didn't write more bytes than expected.
			assert(compressed.size() == nCompressedBytes || compressed[nCompressedBytes] == 0x00);
		}

		// Store the remaining samples.
		int nNewSamples = min(nSamples, min(m_nRawSamples - m_nEncodeBufferSamples, m_nRawSamples));
		if (nNewSamples)
		{
			memcpy_s(&m_EncodeBuffer[m_nEncodeBufferSamples], sizeof(m_EncodeBuffer) - (&m_EncodeBuffer[m_nEncodeBufferSamples] - m_EncodeBuffer),
				&pUncompressed[nSamples - nNewSamples], nNewSamples * BYTES_PER_SAMPLE);
			m_nEncodeBufferSamples += nNewSamples;
		}

// Respawn's Source Engine does not have the bFinal field
#if 0
		// If it must get the last data, just pad with zeros..
		if (bFinal && m_nEncodeBufferSamples && (maxCompressedBytes - nCompressedBytes) >= m_nEncodedBytes)
		{
			auto size = (m_nRawSamples - m_nEncodeBufferSamples) * BYTES_PER_SAMPLE;
			memset(&m_EncodeBuffer[m_nEncodeBufferSamples], 0, size);
			m_pFrameEncoder->EncodeFrame(absl::MakeConstSpan(m_EncodeBuffer, size / sizeof(*m_EncodeBuffer)), compressed.subspan(nCompressedBytes));
			nCompressedBytes += m_nEncodedBytes;
			m_nEncodeBufferSamples = 0;
		}
#endif

#ifndef NDEBUG
		char dbgbuf[256];
		sprintf_s(dbgbuf, "VoiceCodec_Frame[VoiceEncoder_Lyra]::Compress nCompressedBytes:%i nSamples:%i\n",
			nCompressedBytes, nSamples);
		OutputDebugStringA(dbgbuf);
#endif
		return nCompressedBytes;
	}

	virtual int Decompress(const char* pCompressed, int compressedBytes, char* pUncompressed, int maxUncompressedBytes)
	{
		if (!m_pFrameEncoder)
			return 0;

		assert((compressedBytes % m_nEncodedBytes) == 0);
		int nDecompressedBytes = 0;
		int curCompressedByte = 0;
		absl::Span compressed{ reinterpret_cast<const uint8_t*>(pCompressed), static_cast<size_t>(compressedBytes) };
		absl::Span uncompressed{ reinterpret_cast<int16_t*>(pUncompressed), static_cast<size_t>(maxUncompressedBytes) };
		while ((compressedBytes - curCompressedByte) >= m_nEncodedBytes && (maxUncompressedBytes - nDecompressedBytes) >= m_nRawBytes)
		{
			m_pFrameEncoder->DecodeFrame(compressed.subspan(curCompressedByte, m_nEncodedBytes), uncompressed.subspan(nDecompressedBytes / BYTES_PER_SAMPLE));
			curCompressedByte += m_nEncodedBytes;
			nDecompressedBytes += m_nRawBytes;
		}

#ifndef NDEBUG
		char dbgbuf[256];
		sprintf_s(dbgbuf, "VoiceCodec_Frame[VoiceEncoder_Lyra]::Decompress nDecompressedBytes:%i compressedBytes:%i\n",
			nDecompressedBytes, compressedBytes);
		OutputDebugStringA(dbgbuf);
#endif
		return nDecompressedBytes / BYTES_PER_SAMPLE;
	}

	virtual bool ResetState()
	{
		if (m_pFrameEncoder)
			return m_pFrameEncoder->ResetState();
		else
			return false;
	}


public:
	// The codec encodes and decodes samples in fixed-size blocks, so we queue up uncompressed and decompressed data 
	// until we have blocks large enough to give to the codec.
	short				m_EncodeBuffer[MAX_FRAMEBUFFER_SAMPLES];
	int					m_nEncodeBufferSamples;

	IFrameEncoder* m_pFrameEncoder;
	// Params set on Init by the wrapped encoder:
	int					m_nRawBytes, m_nRawSamples;
	int					m_nEncodedBytes;
};


IVoiceCodec* CreateVoiceCodec_Frame(IFrameEncoder* pEncoder)
{
	return new VoiceCodec_Frame(pEncoder);
}
