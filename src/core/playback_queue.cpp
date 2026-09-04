#include "core/playback_queue.hpp"

#include <algorithm>
#include <unordered_set>

namespace {
bool fillUniqueTracks(const std::vector<size_t>& values, std::unordered_set<size_t>& uniqueTracks) {
    uniqueTracks.reserve(values.size());
    for (const size_t track : values)
        if (!uniqueTracks.insert(track).second) return false;
    return true;
}
}  // namespace

bool PlaybackQueue::reset(std::vector<size_t> source, size_t sourcePosition, bool shuffle,
                          uint32_t seed) {
    std::unordered_set<size_t> uniqueTracks;
    if (!fillUniqueTracks(source, uniqueTracks)) return false;
    source_ = std::move(source);
    history_.clear();
    shuffle_ = shuffle;
    if (source_.empty()) {
        order_.clear();
        cursor_ = 0;
        return true;
    }
    sourcePosition = std::min(sourcePosition, source_.size() - 1);
    if (shuffle_) {
        buildShuffledOrder(source_[sourcePosition], seed);
    } else {
        order_ = source_;
        cursor_ = sourcePosition;
    }
    return true;
}

void PlaybackQueue::buildShuffledOrder(size_t currentTrack, uint32_t seed) {
    order_ = source_;
    if (source_.empty()) {
        cursor_ = 0;
        return;
    }
    const auto current = std::find(order_.begin(), order_.end(), currentTrack);
    if (current != order_.end()) std::swap(order_.front(), *current);
    std::mt19937 generator(seed);
    if (order_.size() > 2) std::shuffle(order_.begin() + 1, order_.end(), generator);
    cursor_ = 0;
}

void PlaybackQueue::setShuffle(bool shuffle, uint32_t seed) {
    if (shuffle_ == shuffle || order_.empty()) return;
    const size_t currentTrack = current();
    shuffle_ = shuffle;
    if (shuffle_) {
        buildShuffledOrder(currentTrack, seed);
    } else {
        order_ = source_;
        const auto current = std::find(order_.begin(), order_.end(), currentTrack);
        cursor_ = current == order_.end() ? 0 : static_cast<size_t>(current - order_.begin());
    }
}

std::optional<size_t> PlaybackQueue::next(bool repeatAll, uint32_t seed) {
    if (order_.empty()) return std::nullopt;
    if (cursor_ + 1 < order_.size()) {
        history_.push_back(current());
        ++cursor_;
        return current();
    }
    if (!repeatAll) return std::nullopt;
    const size_t previousTrack = current();
    history_.push_back(previousTrack);
    order_ = source_;
    if (shuffle_) {
        std::mt19937 generator(seed);
        std::shuffle(order_.begin(), order_.end(), generator);
        if (order_.size() > 1 && order_.front() == previousTrack)
            std::swap(order_.front(), order_[1]);
    }
    cursor_ = 0;
    return current();
}

std::optional<size_t> PlaybackQueue::previous() {
    if (history_.empty()) {
        if (!shuffle_ && cursor_ > 0) {
            --cursor_;
            return current();
        }
        return std::nullopt;
    }
    const size_t previousTrack = history_.back();
    history_.pop_back();
    const auto item = std::find(order_.begin(), order_.end(), previousTrack);
    if (item != order_.end()) cursor_ = static_cast<size_t>(item - order_.begin());
    return previousTrack;
}

size_t PlaybackQueue::current() const { return order_.empty() ? 0 : order_[cursor_]; }

bool PlaybackQueue::restore(std::vector<size_t> source, std::vector<size_t> order,
                            std::vector<size_t> history, size_t cursor, bool shuffle) {
    if (source.empty() || order.empty() || cursor >= order.size()) return false;
    if (order.size() != source.size()) return false;
    std::unordered_set<size_t> sourceTracks;
    std::unordered_set<size_t> orderTracks;
    if (!fillUniqueTracks(source, sourceTracks) || !fillUniqueTracks(order, orderTracks) ||
        sourceTracks != orderTracks)
        return false;
    for (const size_t track : history)
        if (sourceTracks.find(track) == sourceTracks.end()) return false;
    source_ = std::move(source);
    order_ = std::move(order);
    history_ = std::move(history);
    cursor_ = cursor;
    shuffle_ = shuffle;
    return true;
}
