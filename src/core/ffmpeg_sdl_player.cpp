#include "core/ffmpeg_sdl_player.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include "core/ffmpeg_audio_decoder.hpp"

namespace {
constexpr Uint32 kMaximumQueuedBytes =
    FfmpegAudioDecoder::outputSampleRate * FfmpegAudioDecoder::outputChannels * sizeof(int16_t) / 2;
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
    FfmpegAudioDecoder decoder;
    SDL_AudioDeviceID audioDevice = 0;

    const auto cleanup = [&] {
        if (audioDevice) SDL_CloseAudioDevice(audioDevice);
        device_ = 0;
    };
    const auto failAndCleanup = [&](const std::string& message) {
        if (!stopping_) fail(generation, message);
        cleanup();
    };

    if (!decoder.open(path)) return failAndCleanup(decoder.error());

    SDL_AudioSpec desired{};
    desired.freq = FfmpegAudioDecoder::outputSampleRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = FfmpegAudioDecoder::outputChannels;
    desired.samples = 1024;
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (!audioDevice)
        return failAndCleanup("Could not open audio output: " + std::string(SDL_GetError()));
    device_ = audioDevice;

    emit({PlayerEventType::DurationChanged, generation, decoder.durationSeconds()});
    emit({PlayerEventType::SeekableChanged, generation, 0, decoder.seekable()});
    emit({PlayerEventType::FileLoaded, generation});
    emit({PlayerEventType::PauseChanged, generation, 0, false});
    SDL_PauseAudioDevice(audioDevice, 0);

    double lastReportedPosition = -1;
    DecodeResult decodeResult = DecodeResult::Audio;
    while (!stopping_) {
        std::optional<double> seek;
        {
            std::lock_guard lock(seekMutex_);
            seek.swap(pendingSeek_);
        }
        if (seek) {
            if (decoder.seek(*seek)) {
                SDL_ClearQueuedAudio(audioDevice);
                positionSeconds_ = *seek;
                emit({PlayerEventType::PositionChanged, generation, *seek});
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
        std::vector<uint8_t> output;
        decodeResult = decoder.read(output);
        if (decodeResult == DecodeResult::Failed) return failAndCleanup(decoder.error());
        if (decodeResult == DecodeResult::End) break;
        if (SDL_QueueAudio(audioDevice, output.data(), static_cast<Uint32>(output.size())) < 0)
            return failAndCleanup("Could not queue audio: " + std::string(SDL_GetError()));
        positionSeconds_ = decoder.positionSeconds();
        if (positionSeconds_.load() - lastReportedPosition >= 0.25) {
            lastReportedPosition = positionSeconds_.load();
            emit({PlayerEventType::PositionChanged, generation, lastReportedPosition});
        }
    }

    while (!stopping_ && decodeResult == DecodeResult::End &&
           SDL_GetQueuedAudioSize(audioDevice) > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!stopping_ && decodeResult == DecodeResult::End) emit({PlayerEventType::Ended, generation});
    cleanup();
}
