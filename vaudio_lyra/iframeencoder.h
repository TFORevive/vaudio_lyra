#pragma once

#include "absl/types/span.h"

// A frame encoder is a codec that encodes and decodes data in fixed-size frames.
// VoiceCodec_Frame handles queuing of data and the IVoiceCodec interface.
class IFrameEncoder
{
public:
	virtual			~IFrameEncoder() {}

	// This is called by VoiceCodec_Frame to see if it can initialize..
	// Fills in the uncompressed and encoded frame size (both are in BYTES).
	virtual bool	Init(int quality, int samplerate, int& rawFrameSize, int& encodedFrameSize) = 0;

	virtual void	Release() = 0;

	// pUncompressed is 8-bit signed mono sound data with 'rawFrameSize' bytes.
	// pCompressed is the size of encodedFrameSize.
	virtual void	EncodeFrame(const absl::Span<const int16_t> uncompressed, const absl::Span<uint8_t> compressed) = 0;

	// pCompressed is encodedFrameSize.
	// pDecompressed is where the 8-bit signed mono samples are stored and has space for 'rawFrameSize' bytes.
	virtual void	DecodeFrame(const absl::Span<const uint8_t> compressed, const absl::Span<int16_t> uncompressed) = 0;

	// Some codecs maintain state between Compress and Decompress calls. This should clear that state.
	virtual bool	ResetState() = 0;
};

class IVoiceCodec;
extern IVoiceCodec* CreateVoiceCodec_Frame(IFrameEncoder* pEncoder);
