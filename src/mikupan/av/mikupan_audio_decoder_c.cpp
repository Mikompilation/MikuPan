#include "mikupan_audio_decoder_c.h"
#include "mikupan_audio_decoder.h"

struct AudioDecoder
{
    MikupanAudioDecoder* impl;
};

extern "C" {

AudioDecoder* mikupan_decoder_open(const char* path)
{
    if (!path)
    {
        return nullptr;
    }

    AudioDecoder* h = new AudioDecoder;
    h->impl = new MikupanAudioDecoder(path);

    return h;
}

void mikupan_decoder_close(AudioDecoder* dec)
{
    if (!dec)
    {
        return;
    }

    delete dec->impl;
    delete dec;
}

int mikupan_decoder_pump(AudioDecoder* dec, size_t minBytes)
{
    if (!dec)
    {
        return 0;
    }

    return dec->impl->pump(minBytes) ? 1 : 0;
}

size_t mikupan_decoder_pop(AudioDecoder* dec, uint8_t* dst, size_t maxBytes)
{
    if (!dec || !dst)
    {
        return 0;
    }

    return dec->impl->pop(dst, maxBytes);
}

int mikupan_decoder_get_rate(AudioDecoder* dec)
{
    if (!dec)
    {
        return 0;
    }

    return dec->impl->get_rate();
}

int mikupan_decoder_get_channels(AudioDecoder* dec)
{
    if (!dec)
    {
        return 0;
    }

    return dec->impl->get_channels();
}
}