/**
 *@文件    :core.cpp
 *@时间    :2022/01/19 21:20:26
 *@作者    :周恒
 *@版本    :1.0
 *@说明    :
 **/
#include "core.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <linux/time_types.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <experimental/coroutine>
#include <vector>

#include "logger.h"
#include "utils.h"
namespace co_uring_web::core {
UringScheduler::UringScheduler() {
	io_uring_queue_init(UringScheduler::UringSize, &uring_, 0);
	this->ioIndex2req_.reserve(10000);
}
void UringScheduler::handleRead(IoRequest &req) {
	assert(req.op == IoRequestOp::OP_READ);
	uint64_t id = ioIndex_++;
	io_uring_sqe *sqe = io_uring_get_sqe(&uring_);
	io_uring_prep_read(sqe, req.fd, req.data, req.capicaty - req.size, 0);
	io_uring_sqe_set_data(sqe, (void *)id);
	io_uring_submit(&uring_);

	this->ioIndex2req_.insert({id, &req});
	if (req.timeout > 0) {
		this->timerQueue_.add(id, utils::getTimeInMilisecond() + req.timeout);
	}
}
void UringScheduler::handleWrite(IoRequest &req) {
	assert(req.op == IoRequestOp::OP_WRITE);

	uint64_t id = ioIndex_++;
	io_uring_sqe *sqe = io_uring_get_sqe(&uring_);
	io_uring_prep_write(sqe, req.fd, req.data, req.size, 0);
	io_uring_sqe_set_data(sqe, (void *)id);
	io_uring_submit(&uring_);

	this->ioIndex2req_.insert({id, &req});
	if (req.timeout > 0) {
		this->timerQueue_.add(id, utils::getTimeInMilisecond() + req.timeout);
	}
}
void UringScheduler::poll(std::vector<void *> &readyHandleAddrs) {
	struct io_uring_cqe *cqe;
	__kernel_timespec ts = {.tv_sec = 0, .tv_nsec = UringTimeoutMiliseconds * 100};
	while (true) {
		int ret = io_uring_wait_cqe_timeout(&uring_, &cqe, &ts);
		if (ret < 0) {
			if (ret == -ETIME) {
				//超时
				break;
			}
			char errBuff[64];
			LOG_ERROR << "io_uring_wait_cqe_timeout异常,错误码为:" << ret << " 即 "
			          << strerror_r(-ret, errBuff, 64);
			break;
		}
		uint64_t id = cqe->user_data;
		auto it = this->ioIndex2req_.find(id);
		if (it == ioIndex2req_.end()) continue;  //说明之前已经被删除了
		auto *req = (IoRequest *)it->second;
		req->retCode = cqe->res;
		void *handleAddr = req->context;
		io_uring_cqe_seen(&uring_, cqe);
		readyHandleAddrs.push_back(handleAddr);

		this->ioIndex2req_.erase(it);
		this->timerQueue_.remove(id);
	}

	std::vector<uint64_t> expireds;
	this->timerQueue_.popExpired(expireds);
	for (auto key : expireds) {
		//处理过期的请求
		auto it = this->ioIndex2req_.find(key);
		if (it == this->ioIndex2req_.end()) continue;
		auto *req = (IoRequest *)it->second;
		req->retCode = -1;
		readyHandleAddrs.push_back(req->context);
		this->ioIndex2req_.erase(it);
	}
}
EpollScheduler::EpollScheduler() {
	using namespace std::string_view_literals;
	epollfd_ = epoll_create1(0);
	if (epollfd_ < 0) {
		int err = errno;
		char errBuff[64];
		LOG_FATAL << "epoll 创建失败 : "sv << err << " : "sv << strerror_r(err, errBuff, 64);
		assert(0);
	}
	this->ioIndex2req_.reserve(10000);
}
void EpollScheduler::handleRead(IoRequest &req) {
	//先直接读
	setNonblocking(req.fd);  //异步非阻塞
	assert(req.op == IoRequestOp::OP_READ);
	req.retCode = 0;
	int readsz = read(req.fd, req.data, req.capicaty);
	if (readsz < 0) {  //一上来就不能读
		int err = errno;
		if (err != EAGAIN) {
			req.retCode = -1;
			completedHandleAddrs_.push_back(req.context);
			return;
		}
		epoll_event event {0};
		event.events = EPOLLIN | EPOLLET;
		uint64_t id = ioIndex_++;
		event.data.u64=id;
		this->ioIndex2req_.insert({id, &req});
		int ret = epoll_ctl(epollfd_, EPOLL_CTL_ADD, req.fd, &event);

		if (ret == -1) {
			req.retCode = -1;
			int err = errno;
			char errBuff[64];
			LOG_ERROR << "epoll: " << epollfd_ << " add error : " << err << " 即 "
			          << strerror_r(err, errBuff, 64);
			completedHandleAddrs_.push_back(req.context);
			return;
		}
		
		
		if (req.timeout > 0)  //不能一次性读完，且需要定时器的情况
			this->timerQueue_.add(id, utils::getTimeInMilisecond() + req.timeout);
		// uncompletedReqs.push_back(&req);
	} else {
		req.retCode = readsz;
		completedHandleAddrs_.push_back(req.context);
	}
}
void EpollScheduler::handleWrite(IoRequest &req) {
	using namespace std;
	//先直接写
	setNonblocking(req.fd);
	assert(req.op == IoRequestOp::OP_WRITE);
	req.retCode = 0;
	int written = write(req.fd, req.data, req.size);
	if (written < 0) {  //如果一上来就不可写的话
		int err = errno;
		if (err != EAGAIN) {  //排除掉其他错误
			req.retCode = -1;
			completedHandleAddrs_.push_back(req.context);
			return;
		}
		//加入epoll监听队列
		goto uncompleted;
	}
	if (written == 0) {  //关闭了连接
		req.retCode = 0;
		completedHandleAddrs_.push_back(req.context);
		return;
	}
	if (req.size == written) {  //全部写完
		req.retCode = written;
		completedHandleAddrs_.push_back(req.context);
		return;
	}
	//只写了一部分的情况，必须继续写，直到返回-1,否则边缘触发不会通知
	req.retCode += written;
	while (written != -1) {
		written = write(req.fd, req.data + req.retCode, req.size - req.retCode);
		if (written == -1) {
			int err = errno;
			if (err != EAGAIN) {
				req.retCode = -1;
				completedHandleAddrs_.push_back(req.context);
				return;
			}
			break;
		}
		if (written == 0) {
			req.retCode = 0;
			completedHandleAddrs_.push_back(req.context);
			return;
		}
		req.retCode += written;
		if (req.retCode == req.size) {
			completedHandleAddrs_.push_back(req.context);
			return;
		}
	}

uncompleted:
	uint64_t id = ioIndex_++;
	this->ioIndex2req_.insert({id, &req});
	epoll_event event {0};
	event.events = EPOLLOUT | EPOLLET;
	event.data.u64 = id;
	int ret = epoll_ctl(epollfd_, EPOLL_CTL_ADD, id, &event);
	if (ret == -1) {
		int err = errno;
		char errBuff[64];
		LOG_ERROR << "epoll:"sv << epollfd_ << " add error : "sv << err << " 即 "sv
		          << strerror_r(err, errBuff, 64);
		completedHandleAddrs_.push_back(req.context);

		return;
	}

	if (req.timeout > 0)  //不能一次性写完，且需要定时器的情况
		this->timerQueue_.add(id, utils::getTimeInMilisecond() + req.timeout);
	// uncompletedReqs.push_back(&req);
}
void EpollScheduler::poll(std::vector<void *> &readyHandleAddrs) {
	using namespace std;
	epoll_event epollEventsBuffer[32];
	int count = epoll_wait(epollfd_, epollEventsBuffer, 32, EpollTimeoutMiliseconds);
	for (int i = 0; i < count; ++i) {
		epoll_event &event = epollEventsBuffer[i];
		if (event.events & EPOLLIN) {
			uint64_t id = event.data.u64;
			auto it = this->ioIndex2req_.find(id);
			if (it == ioIndex2req_.end()) continue;  //说明已经删除了
			auto *req = (IoRequest *)(it->second);
			int readsz = read(req->fd, req->data, req->capicaty);
			if (readsz == 0) {
				req->retCode = 0;  //对端关闭了连接
			} else if (readsz < 0) {
				int err = errno;
				assert(err != EAGAIN);  //必不可能是资源不可用
				req->retCode = -1;
				char errBuff[64];
				LOG_ERROR << "epoll:"sv << epollfd_ << " add error : "sv << err << " 即 "sv
				          << strerror_r(err, errBuff, 64);
			} else {
				req->retCode = readsz;
			}
			int ret = epoll_ctl(epollfd_, EPOLL_CTL_DEL, req->fd, nullptr);
			if (ret < 0) {
				int err = errno;
				char errBuff[64];
				LOG_ERROR << "epoll:"sv << epollfd_ << " del error : "sv << err << " 即 "sv
				          << strerror_r(err, errBuff, 64);
			}
			this->ioIndex2req_.erase(it);
			this->timerQueue_.remove(id);
			completedHandleAddrs_.push_back(req->context);
		} else if (event.events & EPOLLOUT) {
			//需要epoll_del的情况 ret==0 ret<0 (ret+req->retCode==req->size)
			uint64_t id = event.data.u64;
			auto it = this->ioIndex2req_.find(id);
			if (it == ioIndex2req_.end()) continue;  //说明已经删除了
			auto *req = (IoRequest *)(it->second);

			int ret = write(req->fd, req->data + req->retCode, req->size - req->retCode);
			if (ret > 0 && ret + req->retCode != req->size) {
				req->retCode += ret;
				ret = write(req->fd, req->data + req->retCode, req->size - req->retCode);
				assert(ret == -1);
				int err = errno;
				assert(err == EAGAIN);
				continue;
			}
			if (ret < 0) {
				int err = errno;
				assert(err != EAGAIN);  //必不可能是资源不可用
				req->retCode = -1;
				char errBuff[64];
				LOG_ERROR << "epoll:"sv << epollfd_ << " add error : "sv << err << " 即 "sv
				          << strerror_r(err, errBuff, 64);
			} else if (ret == 0) {
				req->retCode = 0;  //对端关闭了连接
			} else {
				req->retCode += ret;  //全部写完
			}
			this->ioIndex2req_.erase(it);
			this->timerQueue_.remove(id);
			completedHandleAddrs_.push_back(req->context);
			ret = epoll_ctl(epollfd_, EPOLL_CTL_DEL, req->fd, nullptr);
			if (ret < 0) {
				int err = errno;
				char errBuff[64];
				LOG_ERROR << "epoll:"sv << epollfd_ << " del error : "sv << err << " 即 "sv
				          << strerror_r(err, errBuff, 64);
			}

		} else {
			LOG_ERROR << "未知的epoll_event"sv;
			assert(0);
		}
	}
	std::vector<uint64_t> expireds;
	this->timerQueue_.popExpired(expireds);
	for (auto id : expireds) {
		//处理过期的请求
		auto it = this->ioIndex2req_.find(id);
		if (it == this->ioIndex2req_.end()) continue;
		auto *ptr = (IoRequest *)(it->second);
		ptr->retCode = -1;
		readyHandleAddrs.push_back(ptr->context);
	}
	swap(readyHandleAddrs, completedHandleAddrs_);
}

}  // namespace co_uring_web::core