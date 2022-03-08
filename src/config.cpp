#include "config.h"

#include <atomic>
#include <cassert>
namespace co_uring_web {
Config *Config::instance = new Config();
void Config::init(const std::string &config_file) {
	using namespace std;
	using namespace utils;
	if(config_file.empty())return;
	JsonParser parser;
	std::string content = utils::readTextFile(config_file);
	auto temp = parser.parse(content);
	instance->map_ = move(*((JsonMap *)temp.release()));
	instance->init_http_dir();
	instance->init_logger_file_name();
	instance->init_logger_output_dir();
    asm volatile("mfence":::"memory");
	
}
Config&Config::getInstance(){
    return *instance;
}
}  // namespace co_uring_web