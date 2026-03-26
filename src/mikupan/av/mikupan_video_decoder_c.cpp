#include "mikupan_video_decoder_c.h"
#include "mikupan_video_decoder.h"

struct MovVidHandle_s
{
    VideoDecoder dec;
};

MovVidHandle movvid_open(const char* path, const char** err)
{
    MovVidHandle movie_handle = new MovVidHandle_s();

    if (!movie_handle->dec.open(path, err))
    {
        delete movie_handle;
        return nullptr;
    }

    return movie_handle;
}

void movvid_close(MovVidHandle h)
{
    if (!h)
    {
        return;
    }

    h->dec.close();
    delete h;
}

int movvid_decode_frame(MovVidHandle h, void*, int, double* pts,
                        const char** err)
{
    if (!h)
    {
        return -1;
    }

    double p = 0.0;

    if (!h->dec.decodeOneFrame(p, err))
    {
        return 2;// EOF
    }

    if (pts)
    {
        *pts = p;
    }

    return 1;
}

const unsigned char* movvid_rgba_ptr(MovVidHandle h, int* strideBytes)
{
    if (!h)
    {
        return nullptr;
    }

    AVFrame* f = h->dec.rgba;

    if (!f)
    {
        return nullptr;
    }

    if (strideBytes)
    {
        *strideBytes = f->linesize[0];
    }

    return f->data[0];
}

int movvid_w(MovVidHandle h)
{
    if (!h)
    {
        return 0;
    }

    return h->dec.w;
}

int movvid_h(MovVidHandle h)
{
    if (!h)
    {
        return 0;
    }

    return h->dec.h;
}

double movvid_fps(MovVidHandle h)
{
    if (!h)
    {
        return 30.0;
    }

    AVStream* st = h->dec.stream;

    if (!st)
    {
        return 30.0;
    }

    if (st->avg_frame_rate.den && st->avg_frame_rate.num)
    {
        return (double) st->avg_frame_rate.num
               / (double) st->avg_frame_rate.den;
    }

    return 30.0;
}