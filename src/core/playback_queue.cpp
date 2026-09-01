#include "core/playback_queue.hpp"

#include <algorithm>
#include <map>
#include <numeric>

void PlaybackQueue::reset(std::vector<size_t> source, size_t sourcePosition, bool shuffle,
                          uint32_t seed) {
    source_ = std::move(source);
    historyPositions_.clear();
    shuffle_ = shuffle;
    if (source_.empty()) {
        orderPositions_.clear();
        cursor_ = 0;
        return;
    }
    sourcePosition = std::min(sourcePosition, source_.size() - 1);
    if (shuffle_) {
        buildShuffledOrder(sourcePosition, seed);
    } else {
        orderPositions_.resize(source_.size());
        std::iota(orderPositions_.begin(), orderPositions_.end(), 0);
        cursor_ = sourcePosition;
    }
}

void PlaybackQueue::buildShuffledOrder(size_t currentSourcePosition, uint32_t seed) {
    orderPositions_.clear();
    if (source_.empty()) {
        cursor_ = 0;
        return;
    }
    orderPositions_.resize(source_.size());
    std::iota(orderPositions_.begin(), orderPositions_.end(), 0);
    std::swap(orderPositions_.front(), orderPositions_[currentSourcePosition]);
    std::mt19937 generator(seed);
    if (orderPositions_.size() > 2)
        std::shuffle(orderPositions_.begin() + 1, orderPositions_.end(), generator);
    cursor_ = 0;
}

void PlaybackQueue::setShuffle(bool shuffle, uint32_t seed) {
    if (shuffle_ == shuffle || orderPositions_.empty()) return;
    const size_t currentSourcePosition = sourcePosition();
    shuffle_ = shuffle;
    if (shuffle_) {
        buildShuffledOrder(currentSourcePosition, seed);
    } else {
        orderPositions_.resize(source_.size());
        std::iota(orderPositions_.begin(), orderPositions_.end(), 0);
        cursor_ = currentSourcePosition;
    }
}

std::optional<size_t> PlaybackQueue::next(bool repeatAll, uint32_t seed) {
    if (orderPositions_.empty()) return std::nullopt;
    if (cursor_ + 1 < orderPositions_.size()) {
        historyPositions_.push_back(sourcePosition());
        ++cursor_;
        return current();
    }
    if (!repeatAll) return std::nullopt;
    const size_t previousSourcePosition = sourcePosition();
    historyPositions_.push_back(previousSourcePosition);
    orderPositions_.resize(source_.size());
    std::iota(orderPositions_.begin(), orderPositions_.end(), 0);
    if (shuffle_) {
        std::mt19937 generator(seed);
        std::shuffle(orderPositions_.begin(), orderPositions_.end(), generator);
        if (orderPositions_.size() > 1 && orderPositions_.front() == previousSourcePosition)
            std::swap(orderPositions_.front(), orderPositions_[1]);
    }
    cursor_ = 0;
    return current();
}

std::optional<size_t> PlaybackQueue::previous() {
    if (historyPositions_.empty()) {
        if (!shuffle_ && cursor_ > 0) {
            --cursor_;
            return current();
        }
        return std::nullopt;
    }
    const size_t previousSourcePosition = historyPositions_.back();
    historyPositions_.pop_back();
    const auto item =
        std::find(orderPositions_.begin(), orderPositions_.end(), previousSourcePosition);
    if (item != orderPositions_.end())
        cursor_ = static_cast<size_t>(item - orderPositions_.begin());
    return source_[previousSourcePosition];
}

size_t PlaybackQueue::current() const {
    return orderPositions_.empty() ? 0 : source_[orderPositions_[cursor_]];
}

size_t PlaybackQueue::sourcePosition() const {
    return orderPositions_.empty() ? 0 : orderPositions_[cursor_];
}

std::vector<size_t> PlaybackQueue::order() const {
    std::vector<size_t> result;
    result.reserve(orderPositions_.size());
    for (size_t position : orderPositions_) result.push_back(source_[position]);
    return result;
}

std::vector<size_t> PlaybackQueue::history() const {
    std::vector<size_t> result;
    result.reserve(historyPositions_.size());
    for (size_t position : historyPositions_) result.push_back(source_[position]);
    return result;
}

bool PlaybackQueue::restore(std::vector<size_t> source, std::vector<size_t> order,
                            std::vector<size_t> history, size_t cursor, bool shuffle) {
    if (source.empty() || order.empty() || cursor >= order.size()) return false;
    if (order.size() != source.size()) return false;
    std::map<size_t, size_t> sourceCounts;
    std::map<size_t, size_t> orderCounts;
    for (size_t track : source) ++sourceCounts[track];
    for (size_t track : order) ++orderCounts[track];
    if (sourceCounts != orderCounts) return false;
    for (size_t track : history)
        if (sourceCounts.find(track) == sourceCounts.end()) return false;
    std::map<size_t, std::vector<size_t>> availablePositions;
    for (size_t position = 0; position < source.size(); ++position)
        availablePositions[source[position]].push_back(position);
    std::map<size_t, size_t> nextPosition;
    std::vector<size_t> resolvedOrder;
    for (size_t track : order) {
        auto& offset = nextPosition[track];
        const auto& positions = availablePositions[track];
        resolvedOrder.push_back(positions[offset++]);
    }
    std::vector<size_t> resolvedHistory;
    for (size_t track : history) resolvedHistory.push_back(availablePositions[track].front());
    source_ = std::move(source);
    orderPositions_ = std::move(resolvedOrder);
    historyPositions_ = std::move(resolvedHistory);
    cursor_ = cursor;
    shuffle_ = shuffle;
    return true;
}
