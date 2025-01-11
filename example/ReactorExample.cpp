#include "server/logging/Logging.h"
#include "server/reactor/Channel.h"
#include "server/reactor/Dispatcher.h"
#include "server/reactor/Demultiplexer.h"
#include "server/reactor/NotificationCenter.h"
#include "server/threadpool/ThreadPool.h"
#include "server/TcpServer.h"
#include <iostream>
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <unistd.h>

using namespace server::reactor;
using namespace server::tcp;

struct person
{
    int len;
    int id;
    int age;
};

class Request {
public:
    Request(int fd, std::string data)
        : _fd(fd)
          , _data(std::move(data))
    {}

    int GetFD() const { return _fd; }
    std::string GetData() const { return _data; }

private:
    int _fd;
    std::string _data;
};

Request GetRequest(int fd, std::string data)
{
    struct person p;
    std::memcpy(&p, data.data(), sizeof(p));
    std::cout << __FILE_NAME__ << ":" << __FUNCTION__ << ":" << __LINE__ << ", received bytes: " << data.length() << ", data: " << data << "\n";;
    std::cout << "data: { Persion: [ len: " << p.len << ", id: " << p.id << ", age: " << p.age << "] }\n";
    return Request(fd, std::move(data));
}

int main (int argc, char *argv[]) {

    server::log::InitializeLogger();

    server::threadpool::GlobalThreadPoolConfig = {3};

    TcpServer server;
    if ( auto inited = server.Init(); inited )
        std::cout << "Initialize server " << inited << "\n";

    auto reuse = server.ReuseAddress(1);
    if ( reuse < 0 )
        std::cout << "reuse address failed\n";

    if ( auto fd = server.GetFd(); fd )
        std::cout << "FD: " << fd << "\n";

    Address addr("127.0.0.1", 9090);
    int bind = 0;
    if ( bind = server.Bind(addr); bind >= 0 )
        std::cout << "bind initialized fd with specified Address: " << addr.GetIP() << ", Port: " << addr.GetPort() <<
            "sccessfully\n";

    if ( auto listen = server.Listen(); listen >= 0 )
        std::cout << "listen on initialized fd: " << listen << "\n";

    Demultiplexer::DEFAULT_EVENTS |= EPOLLET;
    Dispatcher dispatcher;
    dispatcher.EnableSlave(true);
    dispatcher.SetMasterFD(server.GetFd());

    Channel::SetGlobalReceiveCallback([] (int receivedBytes, int err, std::string data)
    {
        std::cout << __FILE_NAME__ << ":" << __FUNCTION__ << ":" << __LINE__ << "errno: " << err << ", received bytes: " << receivedBytes << ", data: " << data << "\n";;
    });
    Channel::SetGlobalSendCallback([] (int sentBytes, int err, std::string data)
    {
        std::cout << __FILE_NAME__ << ":" << __FUNCTION__ << ":" << __LINE__ << "errno: " << err << ", sent bytes: " << sentBytes << ", data: " << data << "\n";;
    });

    std::thread t1(std::bind(&Dispatcher::Dispatch, &dispatcher));
    std::thread t2([&] { sleep(15); dispatcher.Shutdown(); });

    NotificationCenter center(dispatcher);

    sleep(10); // Simulate waiting for connection
    while ( !dispatcher.Stop() )
    {
        auto ret = center.HandleReadyData(GetRequest);
        for ( auto & future : ret )
        {
            auto request = future.get();
            std::cout << "{ FD: " << request.GetFD() << ", Data: " << request.GetData() << " }\n";
            center.NotifyResponseReady(request.GetFD(), "hello, client, thank you for your message.");
        }
    }

    t1.join();
    t2.join();

    return 0;
}
