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
#include <unordered_map>
#include <utility>
#include <vector>

#include "lockfree_queue.h"
#include "logger.h"
#include "timer.h"
namespace co_uring_web::core {

enum class IoRequestOp : int { OP_READ, OP_WRITE };
struct IoRequest {
	char *data {nullptr};
	uint32_t capicaty {0};
	uint32_t size {0};
	int32_t fd {0};
	int32_t retCode {0};
	void *context {nullptr};
	IoRequestOp op;
	int timeout {0};
};
inline int setNonblocking(int fd) {
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
	uint64_t ioIndex_ {0};
	std::unordered_map<uint64_t, void *> ioIndex2req_;
	int epollfd_;
	// std::vector<IoRequest *> uncompletedReqs;
	std::vector<void *> completedHandleAddrs_;
	static constexpr uint32_t EpollTimeoutMiliseconds = 1;  //超时设置成1毫秒
	TimerQueue timerQueue_;

   public:
	void handleWrite(IoRequest &req);
	void handleRead(IoRequest &req);
	void poll(std::vector<void *> &readyHandleAddrs);
	EpollScheduler();
	EpollScheduler(const EpollScheduler &) = delete;
	EpollScheduler(EpollScheduler &&) = default;
	EpollScheduler &operator=(const EpollScheduler &) = delete;
	EpollScheduler &operator=(EpollScheduler &&other) {
		if(&other==this)return *this;
		ioIndex_=other.ioIndex_;
		ioIndex2req_.swap(other.ioIndex2req_);
		epollfd_=other.epollfd_;
		completedHandleAddrs_.swap(other.completedHandleAddrs_);
		timerQueue_=std::move(other.timerQueue_);
		return *this;
	};
};

class UringScheduler {
	uint64_t ioIndex_ {0};
	std::unordered_map<uint64_t, void *> ioIndex2req_;
	static constexpr uint32_t UringSize = 10240;              // io_uring队列深度
	static constexpr uint32_t UringTimeoutMiliseconds = 1;  //超时设置成1毫秒
	io_uring uring_;
	TimerQueue timerQueue_;

   public:
	void handleWrite(IoRequest &req);
	void handleRead(IoRequest &req);
	void poll(std::vector<void *> &readyHandleAddrs);
	UringScheduler();
	UringScheduler(const UringScheduler &) = delete;
	UringScheduler(UringScheduler &&) = default;
	UringScheduler &operator=(const UringScheduler &) = delete;
	UringScheduler &operator=(UringScheduler &&other){
		if(this==&other)[[unlikely]]return *this;
		ioIndex_=other.ioIndex_;
		ioIndex2req_.swap(other.ioIndex2req_);
		uring_=other.uring_;
		timerQueue_=std::move(other.timerQueue_);
		return *this;
	};
};

template <class SchdulerImpl>
class ScheduleImpl_SFINAE {
   public:
	static constexpr bool check_func_handle_write =
	    std::is_member_function_pointer<decltype(&SchdulerImpl::handleWrite)>::value &&
	    std::is_same<decltype(std::declval<SchdulerImpl>().handleWrite(
	                     std::declval<IoRequest &>())),
	                 void>::value;
	static constexpr bool check_func_handle_read =
	    std::is_member_function_pointer<decltype(&SchdulerImpl::handleRead)>::value &&
	    std::is_same<decltype(std::declval<SchdulerImpl>().handleRead(std::declval<IoRequest &>())),
	                 void>::value;
	static constexpr bool check_func_poll =
	    std::is_member_function_pointer<decltype(&SchdulerImpl::poll)>::value &&
	    std::is_same<decltype(std::declval<SchdulerImpl>().poll(
	                     std::declval<std::vector<void *> &>())),
	                 void>::value;
	;
	static constexpr bool value =
	    check_func_handle_write && check_func_handle_read && check_func_poll;
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
			while(queue_->pop(conn)) {
				 func_(conn, this);
				
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
	int port_;  //端口
	int sock_;  // socket fd
	using CoroFunc = TaskImpl (*)(TcpConnection, Scheduler<SchedulerImpl, TaskImpl> *);
	CoroFunc func_;  //协程执行函数
	static constexpr int defaultThreadCount = 4;
	LockfreeQueue<TcpConnection> *queues_;  //用于传递主线程accept的tcp连接到工作线程
	std::thread *threads_;                  //工作线程
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

		if (listen(sock_,23333) < 0) {
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
		//必须要在把queues_和threads_写入内存之后才能启动新线程，确保可见性
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
			TcpConnection tcpConn = {};
			socklen_t len = sizeof(tcpConn.remoteAddr);
			//主线程始终监听socket并接受新连接。
			tcpConn.fd = ::accept(sock_, (sockaddr *)&(tcpConn.remoteAddr), &len);  // NOLINT
			if(tcpConn.fd==-1)continue;
			while (!(queues_[index % thread_count_].push(tcpConn))) {
				//不成功的话，说明已经满了
				index += 1;
			};
			index++;
		}
	};
};
}  // namespace co_uring_web::core