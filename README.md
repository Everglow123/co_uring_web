# co_uring_web
这个项目是把C++20协程与io_uring/epoll结合的一个web server。需要clang-13和linux内核大于5.10
使用ab -n 600000 -c 10000 http://127.0.0.1:8888/tux.png进行本机测试，epoll版本能在16秒左右全部处理完，
io_uring实现在20秒左右处理完。(因为io_uring_wait_cqe_timeout函数有bug,没有按照给定超时时间返回,进而影响其接收主线程的连接)。


此项目主要内容包括:

- C++20协程调度器的实现
- 使用io_uring--linux上的新型高性能异步io接口
- 使用epoll,并封装成proactor的形式
- 一个简易的单生产者环形无锁队列用来传递accept的连接
- 定时器队列的实现，支持对数复杂度的添加，移除，和提取过期定时器
- Json解析器的实现，能解析和导出Json
- 多线程日志模块，支持日志滚动(使用dup2系统调用),格式化打印(支持函数名,行号等)
- http报文解析和导出
- 一个静态的http web server实现
- 杂项
  - 尝试SFINAE来定义调度器接口
  - 使用编译期字符串哈希来进行字符串的switch



## C++20协程
对于网络编程来说，逻辑如果能写成
```C++
co_await asyncWrite(xxx,timeout);
co_await asyncRead(xxx,timeout);
co_await asyncConnect(xxx,timeout);
```
那确实非常直观。
以往的写法都是写成io操作完成之后运行的回调函数。逻辑比较割裂，而且还要维护状态机。
C++20的协程内容比较复杂。基本上对一个协程的每一个生命周期变化的点都能进行控制。
但是总的来说，定义与网络编程相关的协程的运行逻辑大致是
```C++
//定义一个协程函数
Task coroutineFunc(args){
    auto result=co_await AsyncWrite{...}; //1

    ...
    co_return ;                   //2
}
//运行时就是
{
    Task coroutine1= coroutineFunc(args);//程序会自动调用new在堆上分配一个coroutine1，
    //然后直接进入coroutine1一直运行到1处，coroutine1挂起,
    //然后返回当前函数作用域运行do_something
    do_something...//
    ...
    //然后等到coroutine1 等待的事情完成了,
    //需要手动恢复运行coroutine1
    auto handle=xxx;//前面通过某种方式获取到的coroutine1的句柄
    handle.resume();//继续运行coroutine1,获取result，
    //然后运行到2处，运行完毕，退出协程，
    //此时coroutine1在堆上的内容会被自动回收。
    
}
```
就是定义这么一个函数，
首先，需要定义一个task类型，task类型里还需要定义promise类型。promise类型需要定义几个固定的成员函数，get_return_object，initial_suspend，final_suspend，unhandled_exception，比如说我写的HttpTask
```C++
struct HttpTask {
	struct promise_type {
		HttpTask get_return_object() {
			return std::experimental::coroutine_handle<promise_type>::from_promise(*this);
		}
        // initial_suspend函数，定义是否一进入协程就直接挂起
		auto initial_suspend() noexcept { 
            //返回suspend_never就是不要挂起
            return std::experimental::suspend_never {};
        }             
        // final_suspend函数，定义是否在协程结束之前挂起
		auto final_suspend() noexcept { 
            //返回suspend_never就是不要挂起
            return std::experimental::suspend_never {}; 
        }
        //协程里抛出了没有捕获的异常怎么处理
		void unhandled_exception() {};
        //定义了return_void就说明协程没有返回值
		void return_void() {};
	};
    //这个handle就是用来恢复协程运行的句柄
	std::experimental::coroutine_handle<promise_type> handle;
	HttpTask(std::experimental::coroutine_handle<promise_type> h) : handle(h) {};
};
```
以上就是定义协程的大致思路。
接下来是相关的其他的基础设施。awaitable可等待体。一元运算符 co_await 暂停协程并将控制返回给调用方。其操作数是一个表达式，其类型必须要么定义 operator co_await，要么能以当前协程的 Promise::await_transform 转换到这种类型。项目里的AsyncWrite就是后者。
```C++
struct AsyncWrite {
		
    /*await_ready如果返回true,那就意味着 
    co_await AsyncWrite{};表达式立即返回，不会让协程挂起，
    而是继续运行。
    当使用epoll时这里其实可以优化一下，非阻塞fd可以直接尝试写，如果恰好一次
    性就写完，就直接返回true，协程继续运行，就没必要再往epoll注册事件
    */
	inline bool await_ready() noexcept { return false; }
	inline void await_suspend(std::experimental::coroutine_handle<typename Task::promise_type> h) {
		//在这里可以往epoll注册监听事件，或者提交io_uring的读写请求
	}
        
    //co_await AsyncWrite{};异步操作完成，获取这个表达式的返回值，在调用 handle.resume()之后运行。
	inline IoRequest *await_resume() { return req; }
};
```


