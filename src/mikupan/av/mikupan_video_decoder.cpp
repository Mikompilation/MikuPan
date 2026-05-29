#include "mikupan_video_decoder.h"
#include "mikupan/mikupan_logging.h"
#include <cstdio>
#include <string>

#define VDBG(fmt, ...) info_log("[VIDEO] " fmt, ##__VA_ARGS__)

static std::string ff_err2str(int err)
{
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buf, sizeof(buf));
    return std::string(buf);
}

static double ts_to_sec(int64_t ts, AVRational tb, double fallback)
{
    if (ts == AV_NOPTS_VALUE)
    {
        return fallback;
    }

    return ts * av_q2d(tb);
}

bool VideoDecoder::open(const char* path, const char** outErr)
{
    if (outErr)
    {
        *outErr = nullptr;
    }

    VDBG("Opening file: %s", path);

    int ret = avformat_open_input(&fmt, path, nullptr, nullptr);
    if (ret < 0)
    {
        VDBG("avformat_open_input failed");
        return false;
    }

    avformat_find_stream_info(fmt, nullptr);

    VDBG("Stream dump:");

    for (unsigned i = 0; i < fmt->nb_streams; i++)
    {
        AVCodecParameters* cp = fmt->streams[i]->codecpar;

        VDBG("stream %u type=%d codec=%d", i, cp->codec_type, cp->codec_id);
    }

    streamIndex =
        av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (streamIndex < 0)
    {
        VDBG("No video stream found");
        return false;
    }

    stream = fmt->streams[streamIndex];
    timeBase = stream->time_base;

    VDBG("Video stream index: %d", streamIndex);

    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);

    dec = avcodec_alloc_context3(codec);

    avcodec_parameters_to_context(dec, stream->codecpar);

    if (avcodec_open2(dec, codec, nullptr) < 0)
    {
        VDBG("Failed to open video decoder");
        return false;
    }

    w = dec->width;
    h = dec->height;

    VDBG("Video size: %dx%d", w, h);

    sws = sws_getContext(w, h, dec->pix_fmt, w, h, AV_PIX_FMT_RGBA,
                         SWS_BILINEAR, nullptr, nullptr, nullptr);

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    rgba = av_frame_alloc();

    int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, w, h, 1);

    rgbaBuf = (uint8_t*) av_malloc(bufSize);

    av_image_fill_arrays(rgba->data, rgba->linesize, rgbaBuf, AV_PIX_FMT_RGBA,
                         w, h, 1);

    return true;
}

bool VideoDecoder::decodeOneFrame(double& outPtsSec, const char** outErr)
{
    if (outErr)
    {
        *outErr = nullptr;
    }

    for (;;)
    {
        int r = avcodec_receive_frame(dec, frame);

        if (r == 0)
        {
            outPtsSec =
                ts_to_sec(frame->best_effort_timestamp, timeBase, lastPtsSec);

            lastPtsSec = outPtsSec;

            sws_scale(sws, frame->data, frame->linesize, 0, h, rgba->data,
                      rgba->linesize);

            av_frame_unref(frame);

            return true;
        }

        if (r != AVERROR(EAGAIN))
        {
            VDBG("Decoder finished or error");
            return false;
        }

        int ret = av_read_frame(fmt, pkt);

        if (ret < 0)
        {
            VDBG("End of stream");
            return false;
        }

        if (pkt->stream_index == streamIndex)
        {
            avcodec_send_packet(dec, pkt);
        }

        av_packet_unref(pkt);
    }
}

void VideoDecoder::close()
{
    VDBG("Closing decoder");

    if (pkt)
    {
        av_packet_free(&pkt);
    }

    if (frame)
    {
        av_frame_free(&frame);
    }

    if (rgba)
    {
        av_frame_free(&rgba);
    }

    if (rgbaBuf)
    {
        av_free(rgbaBuf);
    }

    if (sws)
    {
        sws_freeContext(sws);
    }

    if (dec)
    {
        avcodec_free_context(&dec);
    }

    if (fmt)
    {
        avformat_close_input(&fmt);
    }

    fmt = nullptr;
    dec = nullptr;
    sws = nullptr;
}