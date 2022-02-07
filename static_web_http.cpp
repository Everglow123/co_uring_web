#include "http.h"



using Server=typename co_uring_web::core::TcpServer<co_uring_web::core::EpollScheduler, co_uring_web::HttpTask>;

int main(){
    co_uring_web::Config::init("/home/zhouheng/C++/co_uring_http/config.json");
	co_uring_web::utils::GlobalLoggerManager::init();
    Server server(8888,4,&co_uring_web::static_web_http<co_uring_web::core::EpollScheduler>);
    server.run();
}