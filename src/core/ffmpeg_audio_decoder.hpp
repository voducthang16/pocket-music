#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

enum class DecodeResult { Audio, End, Failed };

class FfmpegAudioDecoder {
   public:
    static constexpr int outputSampleRate = 48000;
    static constexpr int outputChannels = 2;

    FfmpegAudioDecoder();
    ~FfmpegAudioDecoder();
    FfmpegAudioDecoder(const FfmpegAudioDecoder&) = delete;
    FfmpegAudioDecoder& operator=(const FfmpegAudioDecoder&) = delete;

    bool open(const std::filesystem::path& path);
    DecodeResult read(std::vector<uint8_t>& output);
    bool seek(double seconds);
    double durationSeconds() const;
    double positionSeconds() const;
    bool seekable() const;
    const std::string& error() const;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
