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
#include <cstring>
#include <experimental/coroutine>

#include "logger.h"
namespace co_uring_web::core {
UringScheduler::UringScheduler() { io_uring_queue_init(UringScheduler::UringSize, &uring_, 0); }
void UringScheduler::handleRead(IoRequest &req) {
	assert(req.op == IoRequestOp::OP_READ);
	io_uring_sqe *sqe = io_uring_get_sqe(&uring_);
	io_uring_prep_read(sqe, req.fd, req.data, req.capicaty - req.size, 0);
	io_uring_sqe_set_data(sqe, &req);
	io_uring_submit(&uring_);
}
void UringScheduler::handleWrite(IoRequest &req) {
	assert(req.op == IoRequestOp::OP_WRITE);
	io_uring_sqe *sqe = io_uring_get_sqe(&uring_);
	io_uring_prep_write(sqe, req.fd, req.data, req.capicaty - req.size, 0);
	io_uring_sqe_set_data(sqe, &req);
	io_uring_submit(&uring_);
}
void UringScheduler::poll(std::vector<void *> &readyHandleAddrs) {
	struct io_uring_cqe *cqe;
	__kernel_timespec ts = {.tv_sec = 0, .tv_nsec = UringTimeoutMiliseconds * 1000000};
	while (true) {
		int ret = io_uring_wait_cqe_timeout(&uring_, &cqe, &ts);
		if (ret < 0) {
			if (ret == -ETIME) {
				//超时
				return;
			}
			char errBuff[64];
			LOG_ERROR << "io_uring_wait_cqe_timeout异常,错误码为:" << ret << " 即 "
			          << strerror_r(-ret, errBuff, 64);
			return;
		}
		auto *req = (IoRequest *)(cqe->user_data);
		req->retCode = cqe->res;
		void *handleAddr = req->context;
		io_uring_cqe_seen(&uring_, cqe);
		readyHandleAddrs.push_back(handleAddr);
	}
}
EpollScheduler::EpollScheduler(){
    using namespace std::string_view_literals;
    epollfd_=epoll_create1(0);
    if(epollfd_<0){
        int err=errno;
        char errBuff[64];
        LOG_FATAL<<"epoll 创建失败 : "sv<<err<<" : "sv<<strerror_r(err, errBuff, 64);
        assert(0);
    }
}
void EpollScheduler::handleRead(IoRequest &req) {
	//先直接读
    setNonblocking(req.fd);
	assert(req.op == IoRequestOp::OP_READ);
	req.retCode = 0;
	int readsz = read(req.fd, req.data, req.capicaty);
	if (readsz < 0) {
		int err = errno;
		if (err != EAGAIN && err != EINTR) {
			req.retCode = -1;
			completedHandleAddrs_.push_back(req.context);
			return;
		}
		epoll_event event {0};
		event.events = EPOLLIN | EPOLLET;
		event.data.ptr = &req;
		int ret = epoll_ctl(epollfd_, EPOLL_CTL_ADD, req.fd, &event);
		if (ret == -1) {
			int err = errno;
			char errBuff[64];
			LOG_ERROR << "epoll:" << epollfd_ << " add error : " << err << " 即 "
			          << strerror_r(err, errBuff, 64);
			completedHandleAddrs_.push_back(req.context);
			return;
		}
		// uncompletedReqs.push_back(&req);
	} else if (readsz == 0) {
		req.retCode = 0;
		completedHandleAddrs_.push_back(req.context);
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
	if (written < 0) {
		int err = errno;
		if (err != EAGAIN && err != EINTR) {
			req.retCode = -1;
			completedHandleAddrs_.push_back(req.context);
			return;
		}
		epoll_event event {0};
		event.events = EPOLLOUT | EPOLLET;
		event.data.ptr = &req;
		int ret = epoll_ctl(epollfd_, EPOLL_CTL_ADD, req.fd, &event);
		if (ret == -1) {
			int err = errno;
			char errBuff[64];
			LOG_ERROR << "epoll:"sv << epollfd_ << " add error : "sv << err << " 即 "sv
			          << strerror_r(err, errBuff, 64);
			completedHandleAddrs_.push_back(req.context);
			return;
		}
		// uncompletedReqs.push_back(&req);
		return;
	}
	if (written == 0) {
		completedHandleAddrs_.push_back(req.context);
		return;
	}
	if (req.size == written) {
		req.retCode = written;
		completedHandleAddrs_.push_back(req.context);
		return;
	}
	req.retCode = written;
	int ret = write(req.fd, req.data + req.retCode, req.size - req.retCode);
	assert(ret == -1);
	int err = errno;
	if (err != EAGAIN && err != EINTR) {
		req.retCode = -1;
		completedHandleAddrs_.push_back(req.context);
		return;
	}
	epoll_event event {0};
	event.events = EPOLLOUT | EPOLLET;
	event.data.ptr = &req;
	ret = epoll_ctl(epollfd_, EPOLL_CTL_ADD, req.fd, &event);
	if (ret == -1) {
		int err = errno;
		char errBuff[64];
		LOG_ERROR << "epoll:"sv << epollfd_ << " add error : "sv << err << " 即 "sv
		          << strerror_r(err, errBuff, 64);
		completedHandleAddrs_.push_back(req.context);
		return;
	}
	// uncompletedReqs.push_back(&req);
}
void EpollScheduler::poll(std::vector<void *> &readyHandleAddrs) {
	using namespace std;
	epoll_event epollEventsBuffer[32];
	int count = epoll_wait(epollfd_, epollEventsBuffer, 32, EpollTimeoutMiliseconds);
	for (int i = 0; i < count; ++i) {
		epoll_event &event = epollEventsBuffer[i];
		if (event.events & EPOLLIN) {
			auto *req = (IoRequest *)(event.data.ptr);
			int ret = read(req->fd, req->data, req->capicaty);
			if (ret == 0) {
				req->retCode = 0; //对端关闭了连接
			} else if (ret < 0) {
				int err = errno;
				assert(err != EAGAIN && err != EINTR);  //必不可能是资源不可用
				req->retCode = -1;
				char errBuff[64];
				LOG_ERROR << "epoll:"sv << epollfd_ << " add error : "sv << err << " 即 "sv
				          << strerror_r(err, errBuff, 64);
			} else {
				req->retCode = ret;
			}
			ret = epoll_ctl(epollfd_, EPOLL_CTL_DEL, req->fd, nullptr);
			if (ret < 0) {
				int err = errno;
				char errBuff[64];
				LOG_ERROR << "epoll:"sv << epollfd_ << " del error : "sv << err << " 即 "sv
				          << strerror_r(err, errBuff, 64);
			}
			completedHandleAddrs_.push_back(req->context);
		} else if (event.events & EPOLLOUT) {
			//需要epoll_del的情况 ret==0 ret<0 (ret+req->retCode==req->size)
			auto *req = (IoRequest *)(event.data.ptr);
			int ret = write(req->fd, req->data + req->retCode, req->size - req->retCode);
			if (ret > 0 && ret + req->retCode != req->size) {
				req->retCode += ret;
				ret = write(req->fd, req->data + req->retCode, req->size - req->retCode);
				assert(ret == -1);
				int err = errno;
				assert(err == EAGAIN || err == EINTR);
				continue;
			}
			if (ret < 0) {
				int err = errno;
				assert(err != EAGAIN && err != EINTR);  //必不可能是资源不可用
				req->retCode = -1;
				char errBuff[64];
				LOG_ERROR << "epoll:"sv << epollfd_ << " add error : "sv << err << " 即 "sv
				          << strerror_r(err, errBuff, 64);
			} else if (ret == 0) {
				req->retCode = 0;  //对端关闭了连接
			} else {
				req->retCode += ret;  //全部写完
			}
			completedHandleAddrs_.push_back(req->context);
			ret = epoll_ctl(epollfd_, EPOLL_CTL_DEL, req->fd, nullptr);
			if (ret < 0) {
				int err = errno;
				char errBuff[64];
				LOG_ERROR << "epoll:"sv << epollfd_ << " del error : "sv << err << " 即 "sv
				          << strerror_r(err, errBuff, 64);
			}
			completedHandleAddrs_.push_back(req->context);
		} else {
			LOG_ERROR << "未知的epoll_event"sv;
            assert(0);
		}
	}
    swap(readyHandleAddrs,completedHandleAddrs_);
}

}  // namespace co_uring_web::core