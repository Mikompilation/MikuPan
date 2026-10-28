// Keep checks active in Release builds too.
#undef NDEBUG
#include <cassert>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

int main(int argc, char **argv)
{
    assert(av_find_input_format("mpeg"));
    assert(av_find_input_format("mpegvideo"));
    assert(avio_find_protocol_name("file:smoke.mpg"));
    assert(avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO));
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
    assert(codec);
    AVCodecContext *context = avcodec_alloc_context3(codec);
    assert(context && avcodec_open2(context, codec, nullptr) == 0);
    AVCodecParserContext *parser = av_parser_init(AV_CODEC_ID_MPEG2VIDEO);
    assert(parser);
    SwsContext *scaler = sws_getContext(16, 16, AV_PIX_FMT_YUV420P,
                                      16, 16, AV_PIX_FMT_RGBA,
                                      SWS_BILINEAR, nullptr, nullptr, nullptr);
    assert(scaler);
    sws_freeContext(scaler);
    av_parser_close(parser);
    avcodec_free_context(&context);

    // Exercise PSS stream probing and decode when a local movie is supplied.
    for (int i = 1; i < argc; ++i) {
        AVFormatContext *format = nullptr;
        assert(avformat_open_input(&format, argv[i], nullptr, nullptr) == 0);
        assert(avformat_find_stream_info(format, nullptr) >= 0);
        int index = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        assert(index >= 0 && codec);
        context = avcodec_alloc_context3(codec);
        assert(context);
        assert(avcodec_parameters_to_context(context, format->streams[index]->codecpar) == 0);
        assert(avcodec_open2(context, codec, nullptr) == 0);
        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        assert(packet && frame);
        bool decoded = false;
        while (av_read_frame(format, packet) >= 0) {
            if (packet->stream_index == index) {
                assert(avcodec_send_packet(context, packet) == 0);
                if (avcodec_receive_frame(context, frame) == 0) {
                    decoded = true;
                    break;
                }
            }
            av_packet_unref(packet);
        }
        assert(decoded && frame->width > 0 && frame->height > 0);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&context);
        avformat_close_input(&format);
    }
}
