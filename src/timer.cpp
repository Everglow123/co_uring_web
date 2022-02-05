#include "timer.h"

#include <bits/types/timer_t.h>

#include <cstdint>

#include "utils.h"
namespace co_uring_web::core {
bool TimerQueue::add(uint64_t key, uint64_t expiredTimePoint) {
	if (key2timeid_.find(key) != key2timeid_.end()) return false;
	uint32_t id = index_;
	uint64_t timeid = expiredTimePoint << 32;
	timeid |= id;
	this->key2timeid_.insert({key, {(uint32_t)expiredTimePoint, id}});
	this->timeid2key_.insert({timeid, key});
	index_++;
	return true;
}
bool TimerQueue::remove(uint64_t key) {
	auto key2timeidIt = key2timeid_.find(key);
	if (key2timeidIt == key2timeid_.end()) return false;
	auto [expiredTimePoint, id] = key2timeidIt->second;
	uint64_t timeid = ((uint64_t)expiredTimePoint) << 32;
	timeid |= id;
	auto timeid2keyIt = timeid2key_.find(timeid);
	if (timeid2keyIt == timeid2key_.end()) return false;
	key2timeid_.erase(key2timeidIt);
	timeid2key_.erase(timeid2keyIt);
	return true;
}
void TimerQueue::popExpired(std::vector<uint64_t> &keys) {
	int64_t t = utils::getTimeInMilisecond();
	uint64_t flag = (int64_t(t)) << 32;
	flag |= UINT32_MAX;
	auto endIt = this->timeid2key_.upper_bound(flag);
	for (auto it = timeid2key_.begin(); it != endIt; ++it) {
		keys.push_back(it->second);
		this->key2timeid_.erase(it->second);
	}
	if (endIt != timeid2key_.begin()) {
		timeid2key_.erase(timeid2key_.begin(), endIt);
	}
}
}  // namespace co_uring_web::core