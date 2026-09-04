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

    bool empty() const { return order_.empty(); }
    bool shuffle() const { return shuffle_; }
    size_t current() const;
    size_t cursor() const { return cursor_; }
    const std::vector<size_t>& source() const { return source_; }
    const std::vector<size_t>& order() const { return order_; }
    const std::vector<size_t>& history() const { return history_; }
    bool restore(std::vector<size_t> source, std::vector<size_t> order, std::vector<size_t> history,
                 size_t cursor, bool shuffle);

   private:
    void buildShuffledOrder(size_t currentTrack, uint32_t seed);
    std::vector<size_t> source_;
    std::vector<size_t> order_;
    std::vector<size_t> history_;
    size_t cursor_ = 0;
    bool shuffle_ = false;
};
