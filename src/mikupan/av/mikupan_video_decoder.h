#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <stdint.h>

struct VideoDecoder
{
    AVFormatContext *fmt = nullptr;
    AVCodecContext *dec = nullptr;
    SwsContext *sws = nullptr;

    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *rgba = nullptr;

    uint8_t *rgbaBuf = nullptr;

    AVStream *stream = nullptr;
    int streamIndex = -1;

    int w = 0;
    int h = 0;

    AVRational timeBase = {0, 1};
    double lastPtsSec = 0.0;

    bool sentFlush = false;

    bool open(const char *path, const char **outErr);
    bool decodeOneFrame(double &outPtsSec, const char **outErr);
    void close();
};