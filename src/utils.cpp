#include "utils.h"

#include <unistd.h>
#include <zconf.h>
#include <zlib.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>
void *zliballoc(void *q, unsigned n, unsigned m) { return calloc(n, m); }
void zlibfree(void *q, void *p) { free(p); }

namespace co_uring_web::utils {

std::string getcwdPath() {
	using namespace std;
	string s(256, 0);
	getcwd(s.data(), 256);
	s.resize(strlen(s.data()));
	return s;
};

std::string addr2str(sockaddr_in addr) {
	std::string res(16, 0);  // 255.255.255.255
	int written = 0;
	for (int i = 0; i < sizeof(uint32_t); ++i) {
		uint8_t c = *(((const uint8_t *)&(addr.sin_addr.s_addr)) + i);  // NOLINT

		written += std::snprintf(res.data() + written, res.size() - written, "%u", c);  // NOLINT
		res[written++] = '.';
	}
	res[written - 1] = 0;
	res.resize(written - 1);
	return res;
};
std::string deflateCompress(std::string_view view) {
	using namespace std;
	z_stream zs;
	int err;
	uLong len = view.size();
	uLong resSize = max(2 * len, 1024ul);
	string res(resSize, 0);
	zs.zalloc = &zliballoc;
	zs.zfree = &zlibfree;
	zs.opaque = nullptr;

	err = deflateInit(&zs, Z_DEFAULT_COMPRESSION);
	if (err != Z_OK) {
		return {};
	}
	zs.next_out = (Byte *)res.data();
	zs.avail_out = resSize;
	zs.next_in = (unsigned char *)view.data();
	zs.avail_in = len;
	// err = deflate(&zs, Z_NO_FLUSH);
	// if (err != Z_OK) {
	//   return {};
	// }

	err = deflate(&zs, Z_FINISH);
	if (err != Z_STREAM_END) {
		return {};
	}
	if (zs.avail_in != 0) {
		return {};
	}
	res.resize(zs.total_out);
	err = deflateEnd(&zs);
	if (err != Z_OK) {
		return {};
	}
	return res;
};
std::string deflateUncompress(std::string_view view) {
	using namespace std;
	int err;
	z_stream zs;
	zs.zalloc = &zliballoc;
	zs.zfree = &zlibfree;
	zs.opaque = nullptr;
	zs.next_in = (Byte *)view.data();
	zs.avail_in = view.size();
	uLong resSize = max(view.size() * 4, 1024ul);
	string res(resSize, 0);

	zs.next_out = (Byte *)res.data();
	zs.avail_out = resSize;
	err = inflateInit(&zs);
	if (err != Z_OK) {
		return {};
	}
	err = inflate(&zs, Z_FINISH);
	if (err != Z_STREAM_END) return {};
	if (zs.avail_in != 0) return {};

	res.resize(zs.total_out);
	err = inflateEnd(&zs);
	if (err != Z_OK) {
		return {};
	}
	return res;
};
};  // namespace co_uring_web::utils