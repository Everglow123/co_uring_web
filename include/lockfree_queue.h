/**
 *@文件    :lockfree_queue.h
 *@时间    :2022/01/19 18:47:25
 *@作者    :周恒
 *@版本    :1.0
 *@说明    :
 **/

#pragma once

#include <sys/cdefs.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <type_traits>
namespace co_uring_web {

template <class T, bool is_trivial = std::is_trivial<T>::value>
class LockfreeQueue;

template <class T>
class LockfreeQueue<T, false> {};
/**
 * @brief 单生产者单消费者队列，只支持pod类型的元素
 *
 * @tparam T
 */
template <class T>
class LockfreeQueue<T, true> {
	std::atomic<uint32_t> writeIndex_;
	std::atomic<uint32_t> readIndex_;
	uint32_t capicity_;
	T *queue_;
	static constexpr uint32_t DefaultCapicity = 1024;
	char padding[64];//防止伪共享

   public:
	inline uint32_t count2Index(uint32_t count) { return count % capicity_; };
	LockfreeQueue(uint32_t capicity) : capicity_(capicity) {
		queue_ = (T *)malloc(capicity * sizeof(T));
		writeIndex_.store(0);
		readIndex_.store(0);
	};
	LockfreeQueue() : LockfreeQueue(DefaultCapicity) {};
	bool push(const T &data) {
		uint32_t currentWriteIndex = writeIndex_.load();
		if (count2Index(currentWriteIndex + 1) == count2Index(readIndex_.load())) return false;
		queue_[count2Index(currentWriteIndex)] = data;
		writeIndex_.fetch_add(1);
		return true;
	};
	bool pop(T &data) {
		uint32_t currentReadIndex;
		do {
			currentReadIndex = readIndex_.load();
			if (count2Index(readIndex_.load()) == count2Index(writeIndex_.load())) {
				return false;
			}
			data = queue_[count2Index(currentReadIndex)];
			if (readIndex_.compare_exchange_strong(currentReadIndex, currentReadIndex + 1)) {
				return true;
			}
		} while (true);
		return false;
	};
};

}  // namespace co_uring_web