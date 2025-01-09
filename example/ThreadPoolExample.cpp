#include "server/threadpool/ThreadPool.h"
#include <iostream>
#include <mutex>
#include <thread>

using namespace server::threadpool;

class A
{
public:
    A() : _a(10) {}
    int Get() {
        // std::lock_guard<std::mutex> lk(_mx);
        std::cout << "[ Thread ID: " << std::this_thread::get_id() << " -- " << __FILE_NAME__ << ":" << __FUNCTION__ << ":" << __LINE__ << " ] " << "return _a: " << _a << "\n";
        return _a;
    }
    int Add(int num)
    {
        // std::lock_guard<std::mutex> lk(_mx);
        std::cout << "[ Thread ID: " << std::this_thread::get_id() << " -- " << __FILE_NAME__ << ":" << __FUNCTION__ << ":" << __LINE__ << " ] " << "after Add: " << _a << "\n";
        _a += num;
        return _a;
    }
private:
    int _a;
    std::mutex _mx;
};

void foo()
{
    std::cout << "[ Thread ID: " << std::this_thread::get_id() << " -- " << __FILE_NAME__ << ":" << __FUNCTION__ << ":" << __LINE__ << " ]\n";
}

int get_value()
{
    std::cout << "[ Thread ID: " << std::this_thread::get_id() << " -- " << __FILE_NAME__ << ":" << __FUNCTION__ << ":" << __LINE__ << " ]\n";
    return 1;
}

int add(int a, int b)
{
    std::cout << "[ Thread ID: " << std::this_thread::get_id() << " -- " << __FILE_NAME__ << ":" << __FUNCTION__ << ":" << __LINE__ << " ] " << ( a + b ) << "\n";
    return a + b;
}

int main (int argc, char *argv[]) {
    std::cout << "[Main Thread ID: " << std::this_thread::get_id() << " ]\n";

    auto & pool = ThreadPool::GetGlobalThreadPool();
    pool.EnqueueTask(foo);
    auto f1 = pool.EnqueueTask(get_value);
    auto f2 = pool.EnqueueTask(add, 1, 2);

    A a;
    auto f3 = pool.EnqueueTask(&A::Get, &a);
    auto f4 = pool.EnqueueTask(&A::Add, &a, 20);

    f1.get();
    f2.get();
    f3.get();
    f4.get();

    return 0;
}
