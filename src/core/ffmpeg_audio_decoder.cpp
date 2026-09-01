#include "core/ffmpeg_audio_decoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

namespace {
constexpr AVSampleFormat kOutputFormat = AV_SAMPLE_FMT_S16;

std::string ffmpegError(int code) {
    char message[AV_ERROR_MAX_STRING_SIZE]{};
    return av_make_error_string(message, sizeof(message), code);
}
}  // namespace

struct FfmpegAudioDecoder::Impl {
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    SwrContext* resampler = nullptr;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int streamIndex = -1;
    bool drainingCodec = false;
    bool drainingResampler = false;
    int64_t outputSamples = 0;
    std::string error;

    ~Impl() {
        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&resampler);
        avcodec_free_context(&codec);
        avformat_close_input(&format);
    }

    DecodeResult fail(std::string message) {
        error = std::move(message);
        return DecodeResult::Failed;
    }

    DecodeResult convert(std::vector<uint8_t>& output, const uint8_t** input, int inputSamples) {
        const int sampleRate = codec->sample_rate;
        const int capacity =
            av_rescale_rnd(swr_get_delay(resampler, sampleRate) + inputSamples,
                           FfmpegAudioDecoder::outputSampleRate, sampleRate, AV_ROUND_UP);
        if (capacity <= 0) return DecodeResult::End;
        output.resize(static_cast<size_t>(capacity) * FfmpegAudioDecoder::outputChannels *
                      sizeof(int16_t));
        uint8_t* outputData[] = {output.data()};
        const int converted = swr_convert(resampler, outputData, capacity, input, inputSamples);
        if (converted < 0) return fail("Could not convert audio: " + ffmpegError(converted));
        output.resize(static_cast<size_t>(converted) * FfmpegAudioDecoder::outputChannels *
                      sizeof(int16_t));
        outputSamples += converted;
        return converted > 0 ? DecodeResult::Audio : DecodeResult::End;
    }
};

FfmpegAudioDecoder::FfmpegAudioDecoder() : impl_(std::make_unique<Impl>()) {}
FfmpegAudioDecoder::~FfmpegAudioDecoder() = default;

bool FfmpegAudioDecoder::open(const std::filesystem::path& path) {
    auto& state = *impl_;
    if (!state.packet || !state.frame) {
        state.error = "Could not allocate decode buffers";
        return false;
    }
    int result = avformat_open_input(&state.format, path.c_str(), nullptr, nullptr);
    if (result < 0) {
        state.error = "Could not open audio: " + ffmpegError(result);
        return false;
    }
    result = avformat_find_stream_info(state.format, nullptr);
    if (result < 0) {
        state.error = "Could not inspect audio: " + ffmpegError(result);
        return false;
    }
    const AVCodec* decoder = nullptr;
    state.streamIndex = av_find_best_stream(state.format, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (state.streamIndex < 0 || !decoder) {
        state.error = "No supported audio stream: " + ffmpegError(state.streamIndex);
        return false;
    }
    state.codec = avcodec_alloc_context3(decoder);
    if (!state.codec) {
        state.error = "Could not allocate audio decoder";
        return false;
    }
    result = avcodec_parameters_to_context(state.codec,
                                           state.format->streams[state.streamIndex]->codecpar);
    if (result < 0 || (result = avcodec_open2(state.codec, decoder, nullptr)) < 0) {
        state.error = "Could not configure decoder: " + ffmpegError(result);
        return false;
    }

    AVChannelLayout inputLayout = state.codec->ch_layout;
    if (inputLayout.nb_channels == 0) av_channel_layout_default(&inputLayout, 2);
    const AVChannelLayout outputLayout = AV_CHANNEL_LAYOUT_STEREO;
    result = swr_alloc_set_opts2(&state.resampler, &outputLayout, kOutputFormat, outputSampleRate,
                                 &inputLayout, state.codec->sample_fmt, state.codec->sample_rate, 0,
                                 nullptr);
    if (result < 0 || (result = swr_init(state.resampler)) < 0) {
        state.error = "Could not configure audio conversion: " + ffmpegError(result);
        return false;
    }
    return true;
}

DecodeResult FfmpegAudioDecoder::read(std::vector<uint8_t>& output) {
    auto& state = *impl_;
    output.clear();
    while (true) {
        int result = avcodec_receive_frame(state.codec, state.frame);
        if (result >= 0) {
            const auto converted =
                state.convert(output, const_cast<const uint8_t**>(state.frame->extended_data),
                              state.frame->nb_samples);
            av_frame_unref(state.frame);
            if (converted != DecodeResult::End) return converted;
            continue;
        }
        if (result == AVERROR_EOF) {
            state.drainingResampler = true;
        } else if (result != AVERROR(EAGAIN)) {
            return state.fail("Could not receive decoded audio: " + ffmpegError(result));
        }

        if (state.drainingResampler) return state.convert(output, nullptr, 0);
        if (state.drainingCodec) {
            result = avcodec_send_packet(state.codec, nullptr);
            if (result < 0 && result != AVERROR_EOF)
                return state.fail("Could not flush audio decoder: " + ffmpegError(result));
            continue;
        }

        result = av_read_frame(state.format, state.packet);
        if (result < 0) {
            state.drainingCodec = true;
            continue;
        }
        if (state.packet->stream_index != state.streamIndex) {
            av_packet_unref(state.packet);
            continue;
        }
        result = avcodec_send_packet(state.codec, state.packet);
        av_packet_unref(state.packet);
        if (result < 0) return state.fail("Could not decode audio: " + ffmpegError(result));
    }
}

bool FfmpegAudioDecoder::seek(double seconds) {
    auto& state = *impl_;
    const auto* stream = state.format->streams[state.streamIndex];
    const int64_t timestamp = av_rescale_q(static_cast<int64_t>(seconds * AV_TIME_BASE),
                                           AV_TIME_BASE_Q, stream->time_base);
    if (av_seek_frame(state.format, state.streamIndex, timestamp, AVSEEK_FLAG_BACKWARD) < 0)
        return false;
    avcodec_flush_buffers(state.codec);
    swr_close(state.resampler);
    if (swr_init(state.resampler) < 0) return false;
    state.drainingCodec = false;
    state.drainingResampler = false;
    state.outputSamples = static_cast<int64_t>(seconds * outputSampleRate);
    return true;
}

double FfmpegAudioDecoder::durationSeconds() const {
    return impl_->format->duration > 0 ? static_cast<double>(impl_->format->duration) / AV_TIME_BASE
                                       : 0;
}

double FfmpegAudioDecoder::positionSeconds() const {
    return static_cast<double>(impl_->outputSamples) / outputSampleRate;
}

bool FfmpegAudioDecoder::seekable() const {
    return impl_->format->pb && impl_->format->pb->seekable;
}

const std::string& FfmpegAudioDecoder::error() const { return impl_->error; }
