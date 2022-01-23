#include "logger.h"

#include <bits/types/time_t.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "config.h"
#include "utils.h"
namespace co_uring_web::utils {
GlobalLoggerManager *GlobalLoggerManager::instance = nullptr;
thread_local char LoggerInfoBuilder::timeHeader[LoggerInfoBuilder::timeHeaderLength+1];
thread_local time_t LoggerInfoBuilder::threadTime;
thread_local pthread_t LoggerInfoBuilder::threadId;

void GlobalLoggerManager::rotate() {
	using namespace std;

	namespace fs = filesystem;

	auto getNum = [&](const fs::path &p) -> int {
		auto pName = p.filename().string();
		if (pName != (fileName_ + '.' + outputFileExtensionName)) {
			auto splited = splitToViews(pName, fileName_);
			if (splited.size() == 2) {
				splited = splitToViews(splited.back(), '.');
				int num = -1;
				try {
					num = stoi(string(splited.back()));
					return num;
				} catch (exception &e) {
					fprintf(stderr, "%s", e.what());
				}
			}
		}
		return -1;
	};
	vector<int> nums;
	try {
		for (auto &&it : fs::directory_iterator(getOutputDir())) {
			auto num = getNum(it.path());
			if (num > 0) nums.push_back(num);
		}
	} catch (exception &e) {
		fprintf(stderr, "%s", e.what());
	}
	sort(nums.begin(), nums.end(), greater<int> {});
	string oldNum, newNum;
	stringstream ss;
	for (int num : nums) {
		oldNum=to_string(num);
		newNum=to_string(num+1);
		try {
			fs::rename(
			    outputDir_ / fs::path(fileName_ + '.' + outputFileExtensionName + '.' + oldNum),
			    outputDir_ / fs::path(fileName_ + '.' + outputFileExtensionName + '.' + newNum));
		} catch (const exception &e) {
			fprintf(stderr, "%s", e.what());
		}
	}
	fs::rename(outputDir_ / fs::path(fileName_ + '.' + outputFileExtensionName),
	           outputDir_ / fs::path(fileName_ + '.' + outputFileExtensionName + ".1"));
	int newfd =
	    open(fs::path(outputDir_ / fs::path(fileName_ + '.' + outputFileExtensionName)).c_str(),
	         O_CREAT | O_WRONLY,0777);
	if (dup2(newfd, loggerFileFd_) == -1) {
		fprintf(stderr, "%s", "dup2 error");
		abort();
	}
}
GlobalLoggerManager &GlobalLoggerManager::getInstance() { return *instance; }
void GlobalLoggerManager::init() {
	assert(instance == nullptr);
	instance = new GlobalLoggerManager();
	instance->fileName_ = Config::getInstance().get_logger_file_name();
	instance->outputDir_ = Config::getInstance().get_logger_output_dir();
	instance->loggerFileFd_ = open((instance->outputDir_ + "/" + instance->getFileName() + "." +
	                                GlobalLoggerManager::outputFileExtensionName)
	                                   .c_str(),O_WRONLY|O_APPEND|O_CREAT,S_IRUSR|S_IWUSR);
	if(instance->loggerFileFd_<0){
		fprintf(stderr,"%s", strerror(errno));
		abort();
	}
	if (instance->checkNeedRotate()) {
		instance->rotate();
	}
	asm volatile("mfence" ::: "memory");
	instance->loggerThread = new std::thread([]() {
		while (true) {
			usleep(200000);
			if (GlobalLoggerManager::getInstance().checkNeedRotate()) {
				GlobalLoggerManager::getInstance().rotate();
			}
		}
	});
}
bool GlobalLoggerManager::checkNeedRotate() {
	struct stat currentFileStat;
	fstat(GlobalLoggerManager::getInstance().getLoggerFileFd(), &currentFileStat);
	if(currentFileStat.st_size<=0) return false;
	return (currentFileStat.st_size >= Logger::singleFileSizeLimit);
}
}  // namespace co_uring_web::utils