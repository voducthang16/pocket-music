#include "core/ffmpeg_sdl_player.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace {
constexpr int kOutputRate = 48000;
constexpr int kOutputChannels = 2;
constexpr AVSampleFormat kOutputFormat = AV_SAMPLE_FMT_S16;
constexpr Uint32 kMaximumQueuedBytes = kOutputRate * kOutputChannels * sizeof(int16_t) / 2;

std::string ffmpegError(int code) {
    char message[AV_ERROR_MAX_STRING_SIZE]{};
    return av_make_error_string(message, sizeof(message), code);
}
}  // namespace

FfmpegSdlPlayer::~FfmpegSdlPlayer() { stop(); }

uint64_t FfmpegSdlPlayer::load(const std::filesystem::path& path) {
    stop();
    if (!std::filesystem::is_regular_file(path)) {
        std::lock_guard lock(errorMutex_);
        error_ = "Audio file does not exist";
        return 0;
    }
    {
        std::lock_guard lock(errorMutex_);
        error_.clear();
    }
    stopping_ = false;
    paused_ = false;
    positionSeconds_ = 0;
    const uint64_t generation = generation_.fetch_add(1) + 1;
    worker_ = std::thread(&FfmpegSdlPlayer::decode, this, path, generation);
    return generation;
}

bool FfmpegSdlPlayer::togglePause() { return setPaused(!paused_.load()); }

bool FfmpegSdlPlayer::setPaused(bool paused) {
    const auto device = device_.load();
    if (device == 0 || stopping_) return false;
    paused_ = paused;
    SDL_PauseAudioDevice(device, paused ? 1 : 0);
    emit({PlayerEventType::PauseChanged, generation_.load(), 0, paused});
    return true;
}

bool FfmpegSdlPlayer::seekRelative(int seconds) {
    return seekAbsolute(positionSeconds_.load() + seconds);
}

bool FfmpegSdlPlayer::seekAbsolute(double seconds) {
    if (device_.load() == 0 || stopping_) return false;
    std::lock_guard lock(seekMutex_);
    pendingSeek_ = std::max(0.0, seconds);
    return true;
}

std::vector<PlayerEvent> FfmpegSdlPlayer::drainEvents() {
    std::lock_guard lock(eventMutex_);
    std::vector<PlayerEvent> result;
    result.swap(events_);
    return result;
}

std::string FfmpegSdlPlayer::error() const {
    std::lock_guard lock(errorMutex_);
    return error_;
}

void FfmpegSdlPlayer::emit(PlayerEvent event) {
    std::lock_guard lock(eventMutex_);
    events_.push_back(std::move(event));
}

void FfmpegSdlPlayer::fail(uint64_t generation, std::string message) {
    {
        std::lock_guard lock(errorMutex_);
        error_ = message;
    }
    emit({PlayerEventType::Failed, generation, 0, false, std::move(message)});
}

void FfmpegSdlPlayer::stop() {
    stopping_ = true;
    if (const auto device = device_.load()) {
        SDL_ClearQueuedAudio(device);
        SDL_PauseAudioDevice(device, 0);
    }
    if (worker_.joinable()) worker_.join();
    device_ = 0;
    std::lock_guard lock(seekMutex_);
    pendingSeek_.reset();
}

void FfmpegSdlPlayer::decode(std::filesystem::path path, uint64_t generation) {
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    SwrContext* resampler = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    SDL_AudioDeviceID audioDevice = 0;

    const auto cleanup = [&] {
        if (audioDevice) SDL_CloseAudioDevice(audioDevice);
        device_ = 0;
        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&resampler);
        avcodec_free_context(&codec);
        avformat_close_input(&format);
    };
    const auto failAndCleanup = [&](const std::string& message) {
        if (!stopping_) fail(generation, message);
        cleanup();
    };

    int result = avformat_open_input(&format, path.c_str(), nullptr, nullptr);
    if (result < 0) return failAndCleanup("Could not open audio: " + ffmpegError(result));
    result = avformat_find_stream_info(format, nullptr);
    if (result < 0) return failAndCleanup("Could not inspect audio: " + ffmpegError(result));

    const AVCodec* decoder = nullptr;
    const int streamIndex =
        av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (streamIndex < 0 || !decoder)
        return failAndCleanup("No supported audio stream: " + ffmpegError(streamIndex));

    codec = avcodec_alloc_context3(decoder);
    if (!codec) return failAndCleanup("Could not allocate audio decoder");
    result = avcodec_parameters_to_context(codec, format->streams[streamIndex]->codecpar);
    if (result < 0) return failAndCleanup("Could not configure decoder: " + ffmpegError(result));
    result = avcodec_open2(codec, decoder, nullptr);
    if (result < 0) return failAndCleanup("Could not start decoder: " + ffmpegError(result));

    AVChannelLayout inputLayout = codec->ch_layout;
    if (inputLayout.nb_channels == 0)
        av_channel_layout_default(&inputLayout, codec->ch_layout.nb_channels > 0
                                                    ? codec->ch_layout.nb_channels
                                                    : 2);
    const AVChannelLayout outputLayout = AV_CHANNEL_LAYOUT_STEREO;
    result = swr_alloc_set_opts2(&resampler, &outputLayout, kOutputFormat, kOutputRate,
                                 &inputLayout, codec->sample_fmt, codec->sample_rate, 0, nullptr);
    if (result < 0 || swr_init(resampler) < 0)
        return failAndCleanup("Could not configure audio conversion");

    SDL_AudioSpec desired{};
    desired.freq = kOutputRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = kOutputChannels;
    desired.samples = 1024;
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (!audioDevice) return failAndCleanup("Could not open audio output: " + std::string(SDL_GetError()));
    device_ = audioDevice;

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!packet || !frame) return failAndCleanup("Could not allocate decode buffers");

    const double duration = format->duration > 0
                                ? static_cast<double>(format->duration) / AV_TIME_BASE
                                : 0;
    emit({PlayerEventType::DurationChanged, generation, duration});
    emit({PlayerEventType::SeekableChanged, generation, 0, format->pb && format->pb->seekable});
    emit({PlayerEventType::FileLoaded, generation});
    emit({PlayerEventType::PauseChanged, generation, 0, false});
    SDL_PauseAudioDevice(audioDevice, 0);

    double lastReportedPosition = -1;
    bool reachedEnd = false;
    while (!stopping_) {
        std::optional<double> seek;
        {
            std::lock_guard lock(seekMutex_);
            seek.swap(pendingSeek_);
        }
        if (seek) {
            const auto* stream = format->streams[streamIndex];
            const int64_t timestamp = av_rescale_q(static_cast<int64_t>(*seek * AV_TIME_BASE),
                                                   AV_TIME_BASE_Q, stream->time_base);
            if (av_seek_frame(format, streamIndex, timestamp, AVSEEK_FLAG_BACKWARD) >= 0) {
                avcodec_flush_buffers(codec);
                swr_close(resampler);
                swr_init(resampler);
                SDL_ClearQueuedAudio(audioDevice);
                positionSeconds_ = *seek;
                emit({PlayerEventType::PositionChanged, generation, *seek});
                reachedEnd = false;
            }
        }
        if (paused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (SDL_GetQueuedAudioSize(audioDevice) >= kMaximumQueuedBytes) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        result = av_read_frame(format, packet);
        if (result < 0) {
            reachedEnd = true;
            break;
        }
        if (packet->stream_index != streamIndex) {
            av_packet_unref(packet);
            continue;
        }
        result = avcodec_send_packet(codec, packet);
        av_packet_unref(packet);
        if (result < 0) return failAndCleanup("Could not decode audio: " + ffmpegError(result));

        while (!stopping_ && (result = avcodec_receive_frame(codec, frame)) >= 0) {
            const int outputSamples = av_rescale_rnd(
                swr_get_delay(resampler, codec->sample_rate) + frame->nb_samples, kOutputRate,
                codec->sample_rate, AV_ROUND_UP);
            std::vector<uint8_t> output(static_cast<size_t>(outputSamples) * kOutputChannels *
                                        sizeof(int16_t));
            uint8_t* outputData[] = {output.data()};
            const int converted = swr_convert(resampler, outputData, outputSamples,
                                              const_cast<const uint8_t**>(frame->extended_data),
                                              frame->nb_samples);
            if (converted < 0)
                return failAndCleanup("Could not convert audio: " + ffmpegError(converted));
            output.resize(static_cast<size_t>(converted) * kOutputChannels * sizeof(int16_t));
            if (SDL_QueueAudio(audioDevice, output.data(), static_cast<Uint32>(output.size())) < 0)
                return failAndCleanup("Could not queue audio: " + std::string(SDL_GetError()));
            positionSeconds_ = positionSeconds_.load() +
                               static_cast<double>(converted) / kOutputRate;
            if (positionSeconds_.load() - lastReportedPosition >= 0.25) {
                lastReportedPosition = positionSeconds_.load();
                emit({PlayerEventType::PositionChanged, generation, lastReportedPosition});
            }
            av_frame_unref(frame);
        }
        if (result != AVERROR(EAGAIN) && result != AVERROR_EOF)
            return failAndCleanup("Could not receive decoded audio: " + ffmpegError(result));
    }

    while (!stopping_ && reachedEnd && SDL_GetQueuedAudioSize(audioDevice) > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!stopping_ && reachedEnd) emit({PlayerEventType::Ended, generation});
    cleanup();
}
