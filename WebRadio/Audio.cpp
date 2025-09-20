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

#include "Audio.hpp"

#include <vector>

#include "Utils.hpp"

namespace Audio {

namespace {

static const std::size_t packetChannelSize = 2;
static const std::size_t SDLSampleSize = 1024;
static const ::AVSampleFormat FrameFormat = AV_SAMPLE_FMT_FLT;
static const int SDLSampleFormat = AUDIO_F32SYS;

template <typename V>
void emptyFunction(V *) {}

// not thread safe, we use fibers
template <typename T, void (*FreeFunction)(T *) = emptyFunction<T>>
class ObjectPool {
  std::vector<T *> _free;
  std::vector<T *> _used;
  T *_ready;
  std::size_t _size;
  std::size_t _pos;

 public:
  ObjectPool(std::size_t size = 4096)
      : _free(), _used(), _ready(new T[size]), _size(size), _pos(0) {
    _free.reserve(size / 2);
  }

  ~ObjectPool() {
    std::for_each(_used.begin(), _used.end(), [](T *packet) {
      FreeFunction(packet);
      delete packet;
    });
    for (int i = 0; i < _pos; ++i) {
      FreeFunction(&_ready[i]);
    }
    delete[] _ready;
  }

  ObjectPool(const ObjectPool &) = delete;
  ObjectPool(ObjectPool &&) = delete;

  T *acquire() {
    T *packet = nullptr;
    if (!_free.empty()) {
      packet = _free.back();
      _free.pop_back();
    } else if (_pos < _size) {
      packet = &_ready[_pos++];
    } else {
      packet = new T;
      _used.push_back(packet);
      LOG << _used.size()
          << " object created, consider allocating more to pool than " << _size;
    }
    return packet;
  }

  void release(T *packet) {
    FreeFunction(packet);
    _free.push_back(packet);
  }
};

ObjectPool<::AVPacket> &getPool() {
  // static ObjectPool<::AVPacket, &::av_free_packet> pool;
  static ObjectPool<::AVPacket> pool;

  return pool;
}

// SDL audio callback
void audioCallback(void *userData, std::uint8_t *stream, int len) {
  DataChannel *dataChannel = static_cast<DataChannel *>(userData);
  static std::vector<float> vecData;
  vecData.reserve(len);

  boost::fibers::fiber pullData([dataChannel, stream, len]() {
    float data;
    while (boost::fibers::channel_op_status::success ==
               dataChannel->pop(data) &&
           vecData.size() * sizeof(float) < len) {
      vecData.push_back(data);
    }
    std::memcpy(stream, (uint8_t *)vecData.data(), len);
    vecData.clear();
  });

  pullData.join();
}

}  // namespace

void playAudio(const std::string &response, bool isRepeat) {
  LOG << "start playing audio ";

  av_register_all();

  FFmpegWrapper ffmpeg(response);
  if (!ffmpeg.isInit()) {
    LOG << "could not initialize ffmpeg";
    return;
  }

  DataChannel dataChannel(SDLSampleSize / sizeof(float));
  SDL_Init(SDL_INIT_AUDIO);
  SDL_AudioSpec inputSpec, outputSpec;
  inputSpec.freq = ffmpeg.getSampleRate();
  inputSpec.format = SDLSampleFormat;
  inputSpec.channels = ffmpeg.getNbOfChannels();
  inputSpec.silence = 0;
  inputSpec.samples = SDLSampleSize;
  inputSpec.callback = audioCallback;
  inputSpec.userdata = &dataChannel;

  if (SDL_OpenAudio(&inputSpec, &outputSpec) < 0) {
    LOG << "SDL could not open audio : " << SDL_GetError();
    return;
  } else {
    LOG << "SDL audio opened ";
  }

  SDL_PauseAudio(0);

  PacketChannel packetChannel(packetChannelSize);

  boost::fibers::fiber pushPacket(
      [&ffmpeg, &packetChannel]() { ffmpeg.read(packetChannel); });

  std::vector<float> songData;
  boost::fibers::fiber pullPacket(
      [&ffmpeg, &packetChannel, &dataChannel, &songData]() {
        songData = ffmpeg.bufferData(packetChannel, dataChannel);
      });

  pushPacket.join();
  pullPacket.join();

  if (isRepeat && !songData.empty()) {
    // no need to redo packet decoding
    boost::fibers::fiber replay([&dataChannel, &songData]() {
      for (;;) {
        LOG << "Replay song";
        for (float f : songData) {
          dataChannel.push(f);
        }
      }
    });
    replay.join();
  }

  dataChannel.close();
  LOG << "end playing audio";

  SDL_CloseAudio();
  SDL_Quit();
}

CustomAvioContext::CustomAvioContext(std::string input)
    : _data(std::move(input)),
      _pos(0),
      _buffer(static_cast<uint8_t *>(::av_malloc(_data.size()))),
      _context(avio_alloc_context(_buffer, _data.size(),
                                  // not writable
                                  0, this, &CustomAvioContext::read,
                                  // write fct ptr
                                  nullptr, &CustomAvioContext::seek)) {}

CustomAvioContext::~CustomAvioContext() {
  ::av_free(_context->buffer);
  ::av_free(_context);
}

int CustomAvioContext::read(void *userData, std::uint8_t *buffer,
                            int bufferSize) {
  if (userData == nullptr) {
    LOG << "customAvioContext read fail : nullptr ";
    return -1;
  }

  CustomAvioContext *ctx = static_cast<CustomAvioContext *>(userData);
  const int count =
      std::min(static_cast<int>(ctx->_data.size() - ctx->_pos), bufferSize);

  std::memcpy(buffer,
              reinterpret_cast<const uint8_t *>(ctx->_data.data() + ctx->_pos),
              count);
  ctx->_pos += count;
  return count;
}

int64_t CustomAvioContext::seek(void *userData, int64_t offset, int whence) {
  if (userData == nullptr) {
    LOG << "customAvioContext seek fail : nullptr ";
    return -1;
  }

  CustomAvioContext *ctx = static_cast<CustomAvioContext *>(userData);

  switch (whence) {
    case AVIO_SEEKABLE_NORMAL:
      ctx->_pos = offset;
      return ctx->_pos;
    case AVSEEK_SIZE:
      return ctx->_data.size();
    default:
      LOG << "customAvio context seek unrecognized whence value: " << whence;
      return -1;
  }
}

::AVIOContext *CustomAvioContext::getContext() { return _context; }

FFmpegWrapper::FFmpegWrapper(const std::string &data)
    : _customCtx(data),
      _formatCtx(::avformat_alloc_context()),
      _audioStream(nullptr),
      _codec(nullptr),
      _codecCtx(nullptr),
      _isInit(false) {
  _formatCtx->pb = _customCtx.getContext();
  _isInit = init();
}

FFmpegWrapper::~FFmpegWrapper() {
  LOG << "close codec ctx ";
  avcodec_close(_codecCtx);

  LOG << "close format input";
  avformat_close_input(&_formatCtx);
}

bool FFmpegWrapper::init() {
  int err = avformat_open_input(
      &_formatCtx, "no file, we look in memory with custom context", nullptr,
      nullptr);
  if (err < 0) {
    LOG << "av_format_open_input error : " << err;
    return false;
  }

  err = avformat_find_stream_info(_formatCtx, nullptr);
  if (err < 0) {
    LOG << "av_format_find_stream_info error : " << err;
    return false;
  }

  // for debug purpose
  av_dump_format(_formatCtx, 0, "", 0);

  _idxAudioStream =
      ::av_find_best_stream(_formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &_codec, 0);
  if (_idxAudioStream < 0) {
    LOG << "could not find audio stream";
    return false;
  } else {
    LOG << "audio stream found";
  }

  _codecCtx = _formatCtx->streams[_idxAudioStream]->codec;
  if (_codecCtx == nullptr || _codec == nullptr) {
    LOG << "could not find codec context";
    return false;
  }
  _codecCtx->codec = _codec;

  return true;
}

int FFmpegWrapper::getSampleRate() const {
  assert(_codecCtx);
  return _codecCtx->sample_rate;
}

int FFmpegWrapper::getNbOfChannels() const {
  assert(_codecCtx);
  return _codecCtx->channels;
}

bool FFmpegWrapper::isInit() const { return _isInit; }

void FFmpegWrapper::read(PacketChannel &packetChannel) {
  if (avcodec_open2(_codecCtx, _codec, nullptr) != 0) {
    LOG << "could open codec";
    packetChannel.close();
    return;
  }
  SDL_Event event;
  int retRead = 0;
  while (retRead == 0) {
    AVPacket *packet = getPool().acquire();
    retRead = ::av_read_frame(_formatCtx, packet);

    if (packet->stream_index == _idxAudioStream) {
      packetChannel.push(packet);
    } else {
      getPool().release(packet);
    }

    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        LOG << "SDL_QUIT";
        SDL_Quit();
        retRead = -9999;
        return;
      default:
        break;
    }
  }
  if (retRead != 0) {
    LOG << "av_read_frame returned " << retRead;
  }
  packetChannel.close();
}

std::vector<float> FFmpegWrapper::bufferData(PacketChannel &packetChannel,
                                             DataChannel &dataChannel) {
  ::AVPacket *packet;
  ::AVFrame *frame = ::av_frame_alloc();
  std::vector<float> songData;
  if (frame == nullptr) {
    LOG << "could not allocate frame ";
    ::av_frame_free(&frame);
    dataChannel.close();
    return songData;
  }

  songData.reserve(4096);

  while (boost::fibers::channel_op_status::success ==
         packetChannel.pop(packet)) {
    std::size_t read = 0;
    while (read < packet->size) {
      int hasFrame = 0;

      const int retDecode =
          ::avcodec_decode_audio4(_codecCtx, frame, &hasFrame, packet);
      if (retDecode < 0) {
        LOG << "error decode audio 4 : " << retDecode;
        break;
      }
      read += retDecode;

      if (hasFrame) {
        float **data = reinterpret_cast<float **>(frame->data);
        for (int smp = 0; smp < frame->nb_samples; ++smp) {
          for (int chn = 0; chn < getNbOfChannels(); ++chn) {
            dataChannel.push(data[chn][smp]);
            songData.push_back(data[chn][smp]);
          }
        }
      }
    }
    getPool().release(packet);
  }
  LOG << "end bufferData";

  ::av_frame_free(&frame);

  // RVO
  return songData;
}

}  // namespace Audio
