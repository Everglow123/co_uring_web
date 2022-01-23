/**
 *@文件    :core.h
 *@时间    :2022/01/19 13:45:32
 *@作者    :周恒
 *@版本    :1.0
 *@说明    :🤣
 **/

#include <fcntl.h>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/cdefs.h>
#include <sys/epoll.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <experimental/coroutine>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

#include "lockfree_queue.h"
#include "logger.h"
namespace co_uring_web::core {
static constexpr uint32_t BufferSize = 1024;
enum class IoRequestOp : int { OP_READ, OP_WRITE };
struct IoRequest {
	char *data {nullptr};
	uint32_t capicaty {0};
	uint32_t size {0};
	int32_t fd {0};
	int32_t retCode {0};
	void *context {nullptr};
	IoRequestOp op;
};
static inline int setNonblocking(int fd) {
	int oldOption = fcntl(fd, F_GETFL);  // NOLINT
	int newOption = oldOption | O_NONBLOCK;
	fcntl(fd, F_SETFL, newOption);
	return oldOption;
}
struct TcpConnection {
	sockaddr_in remoteAddr;
	int fd;
	void *context;
};

class EpollScheduler {
	int epollfd_;
	// std::vector<IoRequest *> uncompletedReqs;
	std::vector<void *> completedHandleAddrs_;
	static constexpr uint32_t EpollTimeoutMiliseconds = 1;  //超时设置成1毫秒
   public:
	void handleWrite(IoRequest &req);
	void handleRead(IoRequest &req);
	void poll(std::vector<void *> &readyHandleAddrs);
	EpollScheduler();
	EpollScheduler(const EpollScheduler &) = delete;
	EpollScheduler(EpollScheduler &&) = default;
	EpollScheduler &operator=(const EpollScheduler &) = delete;
	EpollScheduler &operator=(EpollScheduler &&) = default;
};

class UringScheduler {
	static constexpr uint32_t UringSize = 256;              // io_uring队列深度
	static constexpr uint32_t UringTimeoutMiliseconds = 1;  //超时设置成1毫秒
	io_uring uring_;

   public:
	void handleWrite(IoRequest &req);
	void handleRead(IoRequest &req);
	void poll(std::vector<void *> &readyHandleAddrs);
	UringScheduler();
	UringScheduler(const UringScheduler &) = delete;
	UringScheduler(UringScheduler &&) = default;
	UringScheduler &operator=(const UringScheduler &) = delete;
	UringScheduler &operator=(UringScheduler &&) = default;
};

template <class SchdulerImpl>
class ScheduleImpl_SFINAE {
   public:
	static constexpr bool has_func_handle_write =
	    std::is_member_function_pointer<decltype(&SchdulerImpl::handleWrite)>::value;
	static constexpr bool has_func_handle_read =
	    std::is_member_function_pointer<decltype(&SchdulerImpl::handleRead)>::value;
	;
	static constexpr bool has_func_poll =
	    std::is_member_function_pointer<decltype(&SchdulerImpl::poll)>::value;
	static constexpr bool value = has_func_handle_write && has_func_handle_read && has_func_poll;
};

template <class SchedulerImpl, class TaskImpl,
          bool Match = ScheduleImpl_SFINAE<SchedulerImpl>::value>
class Scheduler;
template <class SchedulerImpl, class TaskImpl>
class Scheduler<SchedulerImpl, TaskImpl, false> {};
template <class SchedulerImpl, class TaskImpl>
class Scheduler<SchedulerImpl, TaskImpl, true> {
	SchedulerImpl impl_;
	LockfreeQueue<TcpConnection> *queue_;
	using CoroFunc = TaskImpl (*)(TcpConnection, Scheduler<SchedulerImpl, TaskImpl> *);
	CoroFunc func_;

   public:
	Scheduler(LockfreeQueue<TcpConnection> *queue, CoroFunc func) : queue_(queue), func_(func) {}
	struct AsyncWrite {
		IoRequest *req;
		Scheduler<SchedulerImpl, TaskImpl, true> *scheduler;
		inline bool await_ready() noexcept { return false; }
		inline void
		await_suspend(std::experimental::coroutine_handle<typename TaskImpl::promise_type> h) {
			req->context = h.address();
			req->op = IoRequestOp::OP_WRITE;
			scheduler->impl_.handleWrite(*req);
		}
		inline IoRequest *await_resume() { return req; }
	};
	struct AsyncRead {
		IoRequest *req;
		Scheduler<SchedulerImpl, TaskImpl, true> *scheduler;
		inline bool await_ready() noexcept { return false; }
		inline void
		await_suspend(std::experimental::coroutine_handle<typename TaskImpl::promise_type> h) {
			req->context = h.address();
			req->op = IoRequestOp::OP_READ;
			scheduler->impl_.handleRead(*req);
		}
		inline IoRequest *await_resume() { return req; }
	};

	inline AsyncWrite asyncWrite(IoRequest *req) {
		if (req->size == 0) {
			LOG_FATAL << "req的size不能为0!";
			abort();
		}
		return AsyncWrite {.req = req, .scheduler = this};
	}
	inline AsyncRead asyncRead(IoRequest *req) { return AsyncRead {.req = req, .scheduler = this}; }
	__attribute__((noreturn)) void loop() {
		std::vector<void *> readyHandleAddrs;
		while (true) {
			readyHandleAddrs.clear();
			TcpConnection conn = {0};
			if (queue_->pop(conn)) {
				auto coroutine = func_(conn, this);
			}
			impl_.poll(readyHandleAddrs);
			for (auto *addr : readyHandleAddrs) {
				auto handle = std::experimental::coroutine_handle<
				    typename TaskImpl::promise_type>::from_address(addr);
				handle.resume();
			}
		}
	}
};
template <class SchedulerImpl, class TaskImpl>
class TcpServer {
	int port_;
	int sock_;
	using CoroFunc = TaskImpl (*)(TcpConnection, Scheduler<SchedulerImpl, TaskImpl> *);
	CoroFunc func_;
	static constexpr int defaultThreadCount = 4;
	LockfreeQueue<TcpConnection> *queues_;
	std::thread *threads_;
	int thread_count_;

   public:
	TcpServer(int port, int thread_count, CoroFunc func)
	    : port_(port), thread_count_(thread_count), func_(func) {
		struct sockaddr_in srv_addr;
		sock_ = socket(PF_INET, SOCK_STREAM, 0);
		if (sock_ == -1) {
			perror("socket()");
			assert(0);
			abort();
		}

		int enable = 1;
		if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
			perror("setsockopt");
			assert(0);
			abort();
		}
		memset(&srv_addr, 0, sizeof(srv_addr));
		srv_addr.sin_family = AF_INET;
		srv_addr.sin_port = htons(port);
		srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(sock_, (const struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
			perror("bind()");
			assert(0);
			abort();
		}

		if (listen(sock_, 65535) < 0) {
			perror("listen()");
			assert(0);
			abort();
		}

		queues_ = (LockfreeQueue<TcpConnection> *)malloc(thread_count *
		                                                 sizeof(LockfreeQueue<TcpConnection>));
		for (int i = 0; i < thread_count; ++i) {
			new (queues_ + i) LockfreeQueue<TcpConnection>();
		}
		threads_ = (std::thread *)malloc(thread_count * sizeof(std::thread));
		asm volatile("mfence" ::: "memory");
		for (int i = 0; i < thread_count; ++i) {
			new (threads_ + i) std::thread(
			    [&](int i) {
				    Scheduler<SchedulerImpl, TaskImpl> scheduler(&(queues_[i]), func_);
				    scheduler.loop();
			    },
			    i);
		}
	};
	TcpServer();
	void run() {
		uint64_t index = 0;
		while (true) {
			
			TcpConnection tcpConn = {0};
			socklen_t len = sizeof(tcpConn.remoteAddr);
			tcpConn.fd = ::accept(sock_, (sockaddr *)&(tcpConn.remoteAddr), &len);  // NOLINT

			while (!(queues_[index % thread_count_].push(tcpConn))) {
				index += 1;
			};
			index++;
		}
	};
};
}  // namespace co_uring_web::core