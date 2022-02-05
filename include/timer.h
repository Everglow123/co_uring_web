/**
 *@文件    :timer.h
 *@时间    :2022/01/24 16:19:24
 *@作者    :周恒
 *@版本    :1.0
 *@说明    :
 **/

#pragma once
#include <cstdint>
#include <ctime>
#include <map>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>
namespace co_uring_web::core {
/**
 * @brief
 *
 */
class TimerQueue {
	uint32_t index_ {0};
	/**
	 * @brief 从key 到 {expiredTime,id}的映射
	 *
	 */
	std::unordered_map<uint64_t, std::pair<uint32_t, uint32_t>> key2timeid_;

	/**
	 * @brief 从 {expiredTime,id}到 key 的映射
	 *
	 */
	std::map<uint64_t, uint64_t> timeid2key_;

   public:
	bool add(uint64_t key, uint64_t expiredTimePoint);
	bool remove(uint64_t key);
	void popExpired(std::vector<uint64_t> &keys);
};
}  // namespace co_uring_web::core
