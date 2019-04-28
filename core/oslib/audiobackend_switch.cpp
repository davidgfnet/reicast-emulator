
#if HOST_OS == OS_HORIZON

#include <switch.h>
#include <malloc.h>
#include "oslib/audiostream.h"

static AudioOutBuffer audiooutbuffer;

static void switchaudio_init() {
	// For some reason it seems important to align the buffer to page boundary
	// which happens to be 4KB for ARM57/Horizon
    unsigned buffer_size = 16*1024;
	void *audio_buffer = memalign(0x1000, buffer_size);

	audiooutbuffer.next = NULL;
	audiooutbuffer.buffer = audio_buffer;
	audiooutbuffer.buffer_size = buffer_size;
	audiooutbuffer.data_size = buffer_size;
	audiooutbuffer.data_offset = 0;

	// TODO: assert it returns zero
	audoutInitialize();
	audoutStartAudioOut();

	printf("Sample rate: %d\n", audoutGetSampleRate());
	printf("Channel count: %d\n", audoutGetChannelCount());
	printf("PCM format: 0x%x\n", audoutGetPcmFormat());
	printf("Device state: 0x%x\n", audoutGetDeviceState());
}

// Reicast's audio format is S16LE at 44.1KHz, but Switch's format
// seems to be S16LE at 48Hz :/
static u32 switchaudio_push(void* frame, u32 samples, bool wait) {
	// Copy samples to buffer and append data to out device
	uint32_t *frame32 = (uint32_t*)frame;
	unsigned bufsamples = audiooutbuffer.buffer_size / sizeof(uint32_t);
	for (unsigned i = 0; i < samples; i += bufsamples) {
		AudioOutBuffer *rel = NULL;
		unsigned tocopy = std::min(bufsamples, samples - i) * sizeof(uint32_t);
		memcpy(audiooutbuffer.buffer, &frame32[i], tocopy);
		audiooutbuffer.data_size = tocopy;
		audoutPlayBuffer(&audiooutbuffer, &rel);
	}

	return 1;
}

static void switchaudio_term() {
    // Stop audio playback.
    audoutStopAudioOut();
    audoutExit();
}

audiobackend_t audiobackend_switchaudio = {
		"switch", // Slug
		"switch", // Name
		&switchaudio_init,
		&switchaudio_push,
		&switchaudio_term
};

static bool swaudiobe = RegisterAudioBackend(&audiobackend_switchaudio);

#endif


