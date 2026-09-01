#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

class PlaybackQueue {
   public:
    void reset(std::vector<size_t> source, size_t sourcePosition, bool shuffle,
               uint32_t seed = std::random_device{}());
    void setShuffle(bool shuffle, uint32_t seed = std::random_device{}());
    std::optional<size_t> next(bool repeatAll, uint32_t seed = std::random_device{}());
    std::optional<size_t> previous();

    bool empty() const { return orderPositions_.empty(); }
    bool shuffle() const { return shuffle_; }
    size_t current() const;
    size_t sourcePosition() const;
    size_t cursor() const { return cursor_; }
    const std::vector<size_t>& source() const { return source_; }
    std::vector<size_t> order() const;
    std::vector<size_t> history() const;
    bool restore(std::vector<size_t> source, std::vector<size_t> order, std::vector<size_t> history,
                 size_t cursor, bool shuffle);

   private:
    void buildShuffledOrder(size_t currentSourcePosition, uint32_t seed);
    std::vector<size_t> source_;
    std::vector<size_t> orderPositions_;
    std::vector<size_t> historyPositions_;
    size_t cursor_ = 0;
    bool shuffle_ = false;
};
