#ifndef MIKUPAN_AUDIO_DECODER_C_H
#define MIKUPAN_AUDIO_DECODER_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AudioDecoder AudioDecoder;

AudioDecoder* mikupan_decoder_open(const char* path);

void mikupan_decoder_close(AudioDecoder* dec);

int mikupan_decoder_pump(AudioDecoder* dec, size_t minBytes);

size_t mikupan_decoder_pop(
    AudioDecoder* dec,
    uint8_t* dst,
    size_t maxBytes
);

int mikupan_decoder_get_rate(AudioDecoder* dec);

int mikupan_decoder_get_channels(AudioDecoder* dec);

#ifdef __cplusplus
}
#endif

#endif
