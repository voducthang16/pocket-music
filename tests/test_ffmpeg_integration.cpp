#include <SDL.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include "core/ffmpeg_sdl_player.hpp"

namespace fs = std::filesystem;

namespace {
void write16(std::ofstream& stream, uint16_t value) {
    stream.put(static_cast<char>(value & 0xff));
    stream.put(static_cast<char>((value >> 8) & 0xff));
}

void write32(std::ofstream& stream, uint32_t value) {
    write16(stream, static_cast<uint16_t>(value & 0xffff));
    write16(stream, static_cast<uint16_t>(value >> 16));
}

void writeSilentWav(const fs::path& path) {
    constexpr uint32_t sampleRate = 8000;
    constexpr uint32_t seconds = 2;
    constexpr uint32_t dataSize = sampleRate * seconds * 2;
    std::ofstream stream(path, std::ios::binary);
    stream.write("RIFF", 4);
    write32(stream, 36 + dataSize);
    stream.write("WAVEfmt ", 8);
    write32(stream, 16);
    write16(stream, 1);
    write16(stream, 1);
    write32(stream, sampleRate);
    write32(stream, sampleRate * 2);
    write16(stream, 2);
    write16(stream, 16);
    stream.write("data", 4);
    write32(stream, dataSize);
    for (uint32_t index = 0; index < dataSize; ++index) stream.put(0);
}

template <typename Predicate>
bool waitFor(FfmpegSdlPlayer& player, Predicate predicate, int timeoutMs = 4000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        for (const auto& event : player.drainEvents()) {
            if (predicate(event)) return true;
            if (event.type == PlayerEventType::Failed)
                std::cerr << "FFmpeg event error: " << event.message << '\n';
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}
}  // namespace

int main(int argc, char** argv) {
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL audio initialization failed: " << SDL_GetError() << '\n';
        return 1;
    }

    const auto directory =
        fs::temp_directory_path() /
        ("pocket-music-ffmpeg-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory);
    const bool externalFixture = argc > 1;
    const auto audio = externalFixture ? fs::path(argv[1]) : directory / "silence.wav";
    if (!externalFixture) writeSilentWav(audio);

    int result = 0;
    {
        FfmpegSdlPlayer player;
        const uint64_t generation = player.load(audio);
        if (generation == 0 || !waitFor(player, [&](const PlayerEvent& event) {
                return event.type == PlayerEventType::FileLoaded && event.generation == generation;
            })) {
            std::cerr << "FFmpeg backend did not load WAV: " << player.error() << '\n';
            result = 1;
        } else if (externalFixture &&
                   !waitFor(player, [](const PlayerEvent& event) {
                       return event.type == PlayerEventType::PositionChanged && event.number > 0;
                   })) {
            std::cerr << "FFmpeg backend did not decode fixture audio\n";
            result = 1;
        } else if (!player.setPaused(true) ||
                   !waitFor(player, [](const PlayerEvent& event) {
                       return event.type == PlayerEventType::PauseChanged && event.flag;
                   })) {
            std::cerr << "FFmpeg backend did not pause\n";
            result = 1;
        } else if (!externalFixture &&
                   (!player.seekAbsolute(1.0) || !player.setPaused(false) ||
                   !waitFor(player, [](const PlayerEvent& event) {
                       return event.type == PlayerEventType::Ended;
                   }))) {
            std::cerr << "FFmpeg backend did not seek and finish\n";
            result = 1;
        }
    }

    SDL_Quit();
    std::error_code error;
    fs::remove_all(directory, error);
    return result;
}
