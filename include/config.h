/**
 *@文件    :config.h
 *@时间    :2022/01/18 14:22:51
 *@作者    :周恒
 *@版本    :1.0
 *@说明    :
 **/
#pragma once
#include <a.out.h>
#include <pthread.h>

#include <atomic>
#include <string>
#include <utility>

#include "json.h"
#include "utils.h"
namespace co_uring_web {

#define CONFIG_PROP(type, name, default_value)                  \
   private:                                                     \
	type name##_;                                               \
                                                                \
   public:                                                      \
	inline type get_##name() const { return name##_; };         \
                                                                \
   private:                                                     \
	void init_##name() {                                        \
		auto it = this->map_.data.find(#name);                  \
		if (it == map_.data.end()) {                            \
			name##_ = default_value;                            \
		} else {                                                \
			name##_ = *((type *)(it->second.get()->getData())); \
		}                                                       \
	}

class Config {
	static std::atomic_bool inited;
	static Config *instance;
	utils::JsonMap map_;
	CONFIG_PROP(std::string, logger_file_name, "co_uring_http")
	CONFIG_PROP(std::string, logger_output_dir, "/home/zhouheng/C++/co_uring_http/log")
	CONFIG_PROP(std::string, http_dir, "/home/zhouheng/C++/co_uring_http/public")
   public:
	static void init(const std::string &config_file);
	static Config &getInstance();
};
}  // namespace co_uring_web