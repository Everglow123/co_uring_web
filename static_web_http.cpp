#include "http.h"

#define USING_IOURING

#ifdef USING_EPOLL
using Scheduler = co_uring_web::core::EpollScheduler;
#elifdef USING_IOURING
using Scheduler = co_uring_web::core::UringScheduler;
#endif


using Server=typename co_uring_web::core::TcpServer<Scheduler , co_uring_web::HttpTask>;

int main(){
    co_uring_web::Config::init("/home/zhouheng/C++/co_uring_http/config.json");
	co_uring_web::utils::GlobalLoggerManager::init();
    Server server(8888,4,&co_uring_web::static_web_http<Scheduler >);
    server.run();
}