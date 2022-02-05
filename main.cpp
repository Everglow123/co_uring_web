#include <liburing/io_uring.h>
#include <sys/msg.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "config.h"
#include "core.h"
#include "logger.h"
#include "utils.h"
using std::experimental::coroutine_handle;

struct Task {
	struct promise_type {
		Task get_return_object() { return coroutine_handle<promise_type>::from_promise(*this); }
		auto initial_suspend() noexcept { return std::experimental::suspend_never {}; }
		auto final_suspend() noexcept { return std::experimental::suspend_never {}; }
		void unhandled_exception() {};
		void return_void() {};
	};
	coroutine_handle<promise_type> handle;
	Task(coroutine_handle<promise_type> h) : handle(h) {};
};
using EchoScheduler =
    typename co_uring_web::core::Scheduler<co_uring_web::core::UringScheduler, Task>;

	
Task Echo(co_uring_web::core::TcpConnection conn, EchoScheduler *scheduler) {
	using namespace co_uring_web::core;
	auto addrstr = co_uring_web::utils::addr2str(conn.remoteAddr);
	printf("%s\n",addrstr.c_str());
	LOG_INFO<<addrstr;
	while (true) {
		IoRequest req  {};
		req.data = (char *)malloc(1024);
		req.capicaty = 1024;
		req.fd = conn.fd;
		req.timeout=3000;
		co_await scheduler->asyncRead(&req);
		if (req.retCode <= 0) {
			free(req.data);
			LOG_INFO<<"关闭";
			close(req.fd);
			co_return;
			
		}
		LOG_INFO<<std::string_view(req.data,req.retCode);
		req.size = req.retCode;
		co_await scheduler->asyncWrite(&req);
		free(req.data);
	}
	close(conn.fd);
	co_return;
}

using EchoServer = typename co_uring_web::core::TcpServer<co_uring_web::core::UringScheduler, Task>;

int main(int args, char **argv) {
	// fprintf(stderr, "%s",argv[1]);
	co_uring_web::Config::init("/home/zhouheng/C++/co_uring_http/config.json");
	co_uring_web::utils::GlobalLoggerManager::init();
	EchoServer server(8888, 4, &Echo);
	server.run();
}
