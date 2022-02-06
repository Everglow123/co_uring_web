#pragma once
#include <netinet/in.h>
#include <sys/stat.h>
#include <zlib.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <ratio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace co_uring_web::utils {
std::string deflateCompress(std::string_view);
std::string deflateUncompress(std::string_view);

static inline uint64_t stringHash(std::string_view str){
	constexpr uint64_t p = 31;
	constexpr uint64_t m = 1e9 + 9;
	uint64_t powerOfP = 1;
	uint64_t hashVal = 0;
	for (char i : str) {
		hashVal = (hashVal + (i - 'a' + 1) * powerOfP)%m;
		powerOfP=(powerOfP*p)%m;
	}
	return (hashVal%m+m)%m;
};
static constexpr uint64_t stringHashConstexpr(const std::string_view &str) {
	constexpr uint64_t p = 31;
	constexpr uint64_t m = 1e9 + 9;
	uint64_t powerOfP = 1;
	uint64_t hashVal = 0;
	for (char i : str) {
		hashVal = (hashVal + (i - 'a' + 1) * powerOfP)%m;
		powerOfP=(powerOfP*p)%m;
	}
	return (hashVal%m+m)%m;
}

static inline int64_t getTimeInMilisecond() {
	auto timepoint = std::chrono::system_clock::now();
	int64_t res =
	    std::chrono::duration_cast<std::chrono::milliseconds>(timepoint.time_since_epoch()).count();
	return res;
};
std::string getcwdPath();
std::string addr2str(sockaddr_in addr);

static inline int getFileSizeByName(const std::string &name) {
	struct stat s;
	if (stat(name.c_str(), &s)) {
		return -1;
	}
	return s.st_size;
};
static inline int getFileSizeByFd(int fd) {
	struct stat s;
	if (fstat(fd, &s)) {
		return -1;
	}
	return s.st_size;
}

static inline std::vector<std::string_view>
splitToViews(std::string_view str, std::string_view delimiter, int maxsplit = -1) {
	using namespace std;
	vector<string_view> res;

	if (maxsplit == 0 || str.size() < delimiter.size() || str.size() == 0 || delimiter.size() == 0)
		return res;

	int point = 0;
	for (int i = 0; i < str.size();) {
		if (maxsplit) {
			if (str.substr(i, delimiter.size()) == delimiter) {
				res.emplace_back(str.substr(point, i - point));

				i += delimiter.size();
				point = i;
				maxsplit--;
			} else {
				++i;
			}
		} else {
			return res;
		}
	}
	if (maxsplit && point <= str.size()) {
		res.emplace_back(str.substr(point, str.size() - point));
	}
	return res;
}
static inline std::vector<std::string_view> splitToViews(std::string_view str, char delimiter,
                                                         int maxsplit = -1) {
	using namespace std;
	vector<string_view> res;

	if (maxsplit == 0 || str.size() == 0) return res;

	int point = 0;
	for (int i = 0; i < str.size();) {
		if (maxsplit) {
			if (str[i] == delimiter) {
				res.emplace_back(str.substr(point, i - point));

				i++;
				point = i;
				maxsplit--;
			} else {
				++i;
			}
		} else {
			return res;
		}
	}
	if (maxsplit && point <= str.size()) {
		res.emplace_back(str.substr(point, str.size() - point));
	}
	return res;
}

static inline std::vector<std::string>
splitToStrings(std::string_view str, std::string_view delimiter, int maxsplit = -1) {
	using namespace std;
	vector<string> res;

	if (maxsplit == 0 || str.size() < delimiter.size() || str.size() == 0 || delimiter.size() == 0)
		return res;

	int point = 0;
	for (int i = 0; i < str.size();) {
		if (maxsplit) {
			if (str.substr(i, delimiter.size()) == delimiter) {
				res.emplace_back(str.substr(point, i - point));

				i += delimiter.size();
				point = i;
				maxsplit--;
			} else {
				++i;
			}
		} else {
			return res;
		}
	}
	if (maxsplit && point <= str.size()) {
		res.emplace_back(str.substr(point, str.size() - point));
	}
	return res;
}
static inline std::vector<std::string> splitToStrings(std::string_view str, char delimiter,
                                                      int maxsplit = -1) {
	using namespace std;
	vector<string> res;

	if (maxsplit == 0 || str.size() == 0) return res;

	int point = 0;
	for (int i = 0; i < str.size();) {
		if (maxsplit) {
			if (str[i] == delimiter) {
				res.emplace_back(str.substr(point, i - point));

				i++;
				point = i;
				maxsplit--;
			} else {
				++i;
			}
		} else {
			return res;
		}
	}
	if (maxsplit && point <= str.size()) {
		res.emplace_back(str.substr(point, str.size() - point));
	}
	return res;
}

static inline std::string stringsJoin(std::vector<std::string> &v, std::string_view sv = "") {
	using namespace std;
	if (v.size() == 0) return "";
	if (v.size() == 1) return v.back();
	stringstream ss;
	if (!sv.empty()) {
		for (int i = 0; i < v.size() - 1; ++i) {
			ss << v[i] << sv;
		}
		ss << v.back();
	} else {
		for (auto &&s : v) {
			ss << s;
		}
	}
	return ss.str();
}

struct Utf8CodePoint {
	char bytes[8];
};
static inline Utf8CodePoint fromUnicode(wchar_t uChar) {
	Utf8CodePoint res;
	char *pOutput = &(res.bytes[1]);
	int outSize = 6;

	if (uChar <= 0x0000007F) {
		// * U-00000000 - U-0000007F:  0xxxxxxx
		*pOutput = (uChar & 0x7F);
		res.bytes[0] = 1;
	} else if (uChar >= 0x00000080 && uChar <= 0x000007FF) {
		// * U-00000080 - U-000007FF:  110xxxxx 10xxxxxx
		*(pOutput + 1) = (uChar & 0x3F) | 0x80;
		*pOutput = ((uChar >> 6) & 0x1F) | 0xC0;
		res.bytes[0] = 2;
	} else if (uChar >= 0x00000800 && uChar <= 0x0000FFFF) {
		// * U-00000800 - U-0000FFFF:  1110xxxx 10xxxxxx 10xxxxxx
		*(pOutput + 2) = (uChar & 0x3F) | 0x80;
		*(pOutput + 1) = ((uChar >> 6) & 0x3F) | 0x80;
		*pOutput = ((uChar >> 12) & 0x0F) | 0xE0;
		res.bytes[0] = 3;
	} else if (uChar >= 0x00010000 && uChar <= 0x001FFFFF) {
		// * U-00010000 - U-001FFFFF:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		*(pOutput + 3) = (uChar & 0x3F) | 0x80;
		*(pOutput + 2) = ((uChar >> 6) & 0x3F) | 0x80;
		*(pOutput + 1) = ((uChar >> 12) & 0x3F) | 0x80;
		*pOutput = ((uChar >> 18) & 0x07) | 0xF0;
		res.bytes[0] = 4;
	} else if (uChar >= 0x00200000 && uChar <= 0x03FFFFFF) {
		// * U-00200000 - U-03FFFFFF:  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		*(pOutput + 4) = (uChar & 0x3F) | 0x80;
		*(pOutput + 3) = ((uChar >> 6) & 0x3F) | 0x80;
		*(pOutput + 2) = ((uChar >> 12) & 0x3F) | 0x80;
		*(pOutput + 1) = ((uChar >> 18) & 0x3F) | 0x80;
		*pOutput = ((uChar >> 24) & 0x03) | 0xF8;
		res.bytes[0] = 5;
	} else if (uChar >= 0x04000000 && uChar <= 0x7FFFFFFF) {
		// * U-04000000 - U-7FFFFFFF:  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		*(pOutput + 5) = (uChar & 0x3F) | 0x80;
		*(pOutput + 4) = ((uChar >> 6) & 0x3F) | 0x80;
		*(pOutput + 3) = ((uChar >> 12) & 0x3F) | 0x80;
		*(pOutput + 2) = ((uChar >> 18) & 0x3F) | 0x80;
		*(pOutput + 1) = ((uChar >> 24) & 0x3F) | 0x80;
		*pOutput = ((uChar >> 30) & 0x01) | 0xFC;
		res.bytes[0] = 6;
	}

	return res;
};
static inline std::string readTextFile(const std::string &path) {
	using namespace std;
	ifstream ifs(path, ios::ate);
	if (!ifs.is_open()) {
		throw invalid_argument("没有这个文件: " + path);
	}
	auto size = ifs.tellg();
	string res(size, 0);
	ifs.seekg(0);
	ifs.read(&res[0], size);
	ifs.close();
	return res;
}

}  // namespace co_uring_web::utils