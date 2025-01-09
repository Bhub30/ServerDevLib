#include "server/logging/Logging.h"
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

using namespace server::log;

void func1()
{
    for ( int i = 0; i < 10; ++i )
    {
        LOG(INFO) << "func1 number: " << i;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
void func2()
{
    for ( int i = 0; i < 10; ++i )
    {
        LOG(INFO) << "func2 number: " << i;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main (int argc, char *argv[]) {
    InitializeLogger();

    std::thread t1(func1);
    std::thread t2(func2);

    LOG(INFO) << "main thread";

    // std::string msg(10000, 'A');
    // for ( int i = 0; i < 1000; ++i )
    //     LOG(INFO) << msg;

    LOG(FATAL) << "fatal message";

    t1.join();
    t2.join();

    return 0;
}
