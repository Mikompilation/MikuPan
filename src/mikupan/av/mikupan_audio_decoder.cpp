#include "mikupan_audio_decoder.h"
#include <cstring>
#include <algorithm>

static constexpr uint8_t START_CODE[3] = {0x00,0x00,0x01};
static constexpr uint8_t PRIVATE_STREAM1 = 0xBD;

MikupanAudioDecoder::MikupanAudioDecoder(const char* path)
{
    file_ = std::fopen(path,"rb");
    if(!file_){
        std::perror("MikupanAudioDecoder");
        eof_ = true;
    }
}

MikupanAudioDecoder::~MikupanAudioDecoder()
{
    if(file_) std::fclose(file_);
}


bool MikupanAudioDecoder::readN(uint8_t* dst,size_t n)
{
    if(!n) return true;

    if(std::fread(dst,1,n,file_) != n){
        eof_ = true;
        return false;
    }

    return true;
}

bool MikupanAudioDecoder::findStartCode(uint8_t sid)
{
    uint8_t w[4];

    while(!eof_)
    {
        if(!readN(w,4)) return false;

        if(!memcmp(w,START_CODE,3) && w[3]==sid)
            return true;

        if(std::fseek(file_,-3,SEEK_CUR))
        {
            eof_=true;
            return false;
        }
    }

    return false;
}

static size_t findSig(const std::vector<uint8_t>& b,const char sig[4])
{
    for(size_t i=0;i+4<=b.size();i++)
        if(!memcmp(&b[i],sig,4))
            return i;

    return SIZE_MAX;
}

bool MikupanAudioDecoder::readNextPrivateStream1PES()
{
    if(!findStartCode(PRIVATE_STREAM1))
        return false;

    uint8_t lenbe[2];
    if(!readN(lenbe,2)) return false;

    uint16_t pesLen = rd_be16(lenbe);
    if(!pesLen) return false;

    uint8_t hdr[3];
    if(!readN(hdr,3)) return false;

    uint8_t hdrLen = hdr[2];

    if(hdrLen)
        if(std::fseek(file_,hdrLen,SEEK_CUR))
            return false;

    int payloadLen = pesLen - 3 - hdrLen;
    if(payloadLen <= 0) return true;

    std::vector<uint8_t> payload(payloadLen);

    if(!readN(payload.data(), payloadLen))
        return false;

    /* saltar prefijo FF A0 00 00 */
    if (payloadLen >= 4 &&
        payload[0] == 0xFF &&
        payload[1] == 0xA0 &&
        payload[2] == 0x00 &&
        payload[3] == 0x00)
    {
        payload.erase(payload.begin(), payload.begin() + 4);
    }

    size_t old = payloadAcc_.size();
    payloadAcc_.resize(old + payload.size());

    memcpy(payloadAcc_.data() + old, payload.data(), payload.size());

    return true;
}

void MikupanAudioDecoder::parseAccumulatedPayload()
{
    while(true)
    {
        if(!haveHeader_)
        {
            size_t p = findSig(payloadAcc_,"SShd");
            if(p==SIZE_MAX || p+32>payloadAcc_.size())
                return;

            size_t o = p+8;

            header_.type       = rd_le32(&payloadAcc_[o]); o+=4;
            header_.rate       = rd_le32(&payloadAcc_[o]); o+=4;
            header_.channels   = rd_le32(&payloadAcc_[o]); o+=4;
            header_.interleave = rd_le32(&payloadAcc_[o]);

            haveHeader_=true;

            payloadAcc_.erase(payloadAcc_.begin(),payloadAcc_.begin()+o+12);

            std::printf(
                "[MIKUPAN] type=%u rate=%u ch=%u interleave=%u\n",
                header_.type,
                header_.rate,
                header_.channels,
                header_.interleave
            );

            continue;
        }

        if(!haveBody_)
        {
            size_t p = findSig(payloadAcc_,"SSbd");
            if(p==SIZE_MAX || p+8>payloadAcc_.size())
                return;

            bodyRemaining_=rd_le32(&payloadAcc_[p+4]);
            haveBody_=true;

            payloadAcc_.erase(payloadAcc_.begin(),payloadAcc_.begin()+p+8);

            std::printf("[MIKUPAN] audio bytes: %u\n",bodyRemaining_);

            continue;
        }

        if(!bodyRemaining_ || payloadAcc_.empty())
            return;

        size_t take = std::min((size_t)bodyRemaining_,payloadAcc_.size());

        size_t old = rawAudio_.size();
        rawAudio_.resize(old+take);

        std::memcpy(rawAudio_.data()+old,payloadAcc_.data(),take);

        payloadAcc_.erase(payloadAcc_.begin(),payloadAcc_.begin()+take);

        bodyRemaining_ -= take;

        convertRawToPCM();
    }
}

void MikupanAudioDecoder::convertRawToPCM()
{
    if(header_.channels!=2 || header_.type==2)
        return;

    size_t inter = header_.interleave;
    if(!inter || inter%2) return;

    size_t frame = inter*2;
    size_t usable = (rawAudio_.size()/frame)*frame;

    if(!usable) return;

    size_t prev = pcmOut_.size();
    pcmOut_.resize(prev + usable);

    uint8_t* dst = pcmOut_.data()+prev;

    for(size_t b=0;b<usable;b+=frame)
    {
        uint8_t* L = rawAudio_.data()+b;
        uint8_t* R = rawAudio_.data()+b+inter;

        for(size_t i=0;i<inter;i+=2)
        {
            *dst++ = L[i];
            *dst++ = L[i+1];
            *dst++ = R[i];
            *dst++ = R[i+1];
        }
    }

    rawAudio_.erase(rawAudio_.begin(),rawAudio_.begin()+usable);
}

bool MikupanAudioDecoder::pump(size_t minBytes)
{
    if(!file_ || eof_) return false;
    if(haveHeader_ && header_.type==2) return false;

    while(pcmOut_.size()<minBytes && !eof_)
    {
        if(!readNextPrivateStream1PES())
            break;

        parseAccumulatedPayload();
    }

    return !pcmOut_.empty();
}

size_t MikupanAudioDecoder::pop(uint8_t* dst,size_t maxBytes)
{
    size_t n = std::min(maxBytes,pcmOut_.size());

    std::memcpy(dst,pcmOut_.data(),n);

    pcmOut_.erase(pcmOut_.begin(),pcmOut_.begin()+n);

    return n;
}