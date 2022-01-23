#include "utils.h"
#include <unistd.h>

#include <cstring>
#include <optional>
namespace co_uring_web::utils {
std::string getcwdPath(){
	using namespace std;
	string s(256,0);
	getcwd(s.data(), 256);
	s.resize(strlen(s.data()));
	return s;
};


 std::optional<std::string> addr2str(sockaddr_in addr) {
	std::string res(16, 0);//255.255.255.255
	int written = 0;
	for (int i = 0; i < sizeof(uint32_t); ++i) {
		uint8_t c = *(((const uint8_t *)&(addr.sin_addr.s_addr)) + i);                 // NOLINT
		written += std::snprintf(res.data() + written, res.size() - written, "%u", c);  // NOLINT
		res[written++] = '.';
	}
	res[written - 1] = 0;
	res.resize(written - 1);
	return res;
};
};  // namespace co_uring_web::utils