#ifndef MIKUPAN_AUDIO_DECODER_H
#define MIKUPAN_AUDIO_DECODER_H

#include <cstdint>
#include <cstdio>
#include <vector>

class MikupanAudioDecoder {
public:
    explicit MikupanAudioDecoder(const char* path);
    ~MikupanAudioDecoder();

    bool pump(size_t minBytes);
    size_t pop(uint8_t* dst, size_t maxBytes);
    
    int get_rate() const { return haveHeader_ ? (int)header_.rate : 0; }
    int get_channels() const { return haveHeader_ ? (int)header_.channels : 0; }

private:
    struct SS2Header {
        uint32_t type       = 0;
        uint32_t rate       = 0;
        uint32_t channels   = 0;
        uint32_t interleave = 0;
    };

    static uint16_t rd_be16(const uint8_t* p) {
        return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
    }

    static uint32_t rd_le32(const uint8_t* p) {
        return (uint32_t(p[0])      ) |
               (uint32_t(p[1]) <<  8) |
               (uint32_t(p[2]) << 16) |
               (uint32_t(p[3]) << 24);
    }

    bool readN(uint8_t* dst, size_t n);
    bool findStartCode(uint8_t sid);
    bool readNextPrivateStream1PES();

    void parseAccumulatedPayload();
    void convertRawToPCM();

private:
    std::FILE* file_ = nullptr;
    bool eof_ = false;

    bool haveHeader_ = false;
    bool haveBody_   = false;

    SS2Header header_{};
    uint32_t bodyRemaining_ = 0;

    std::vector<uint8_t> payloadAcc_;
    std::vector<uint8_t> rawAudio_;
    std::vector<uint8_t> pcmOut_;
};

#endif // MIKUPAN_AUDIO_DECODER_H