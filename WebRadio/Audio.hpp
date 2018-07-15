/*
 Copyright 2018 - Ivan Landry

 This file is part of WebRadio.

WebRadio is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

WebRadio is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with WebRadio.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <boost/fiber/all.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include <SDL2/SDL.h>

namespace Audio
{
typedef boost::fibers::buffered_channel<::AVPacket*> PacketChannel;
typedef boost::fibers::buffered_channel<float> DataChannel;



void playAudio(const std::string &);

class CustomAvioContext
{
    public:
    CustomAvioContext(std::string);
    ~CustomAvioContext();
    CustomAvioContext(const CustomAvioContext &) = delete;
    CustomAvioContext(CustomAvioContext &&) = delete;

    ::AVIOContext * getContext();

    static int read(void * userData, uint8_t * buffer, int bufferSize);

    static int64_t seek(void * userData, int64_t offset, int whence);

    private:
    std::string _data;
    std::size_t _pos;
    uint8_t * _buffer;
    ::AVIOContext * _context;
};

class FFmpegWrapper
{
    //this legacy C API must be quarantained :) 
    public:
    FFmpegWrapper(const std::string &);
    ~FFmpegWrapper();
    FFmpegWrapper(const FFmpegWrapper &) = delete;
    FFmpegWrapper(FFmpegWrapper &&) = delete;

    int getSampleRate() const;
    int getNbOfChannels() const;
    bool isInit() const;

    void read(PacketChannel & packetChannel);
    void bufferData(PacketChannel & packetChannel, DataChannel & dataChannel); 

    private:
    bool init();

    CustomAvioContext _customCtx;
    ::AVFormatContext * _formatCtx;
    ::AVStream * _audioStream;
    ::AVCodec * _codec;
    ::AVCodecContext * _codecCtx;
    int _idxAudioStream;
    bool _isInit;

};



}


#endif
