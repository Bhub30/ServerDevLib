#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "server/TcpServer.h"
#include "server/TcpSocket.h"

struct person
{
    int len;
    int id;
    int age;
};

int main(int argc, char *argv[]) {
    if ( argc < 2 ) {
        std::cout << "Usage: ChatRoom [port]\n";
        return -1;
    }

    server::tcp::TcpServer ser;

    if ( auto inited = ser.Init(); inited )
        std::cout << "Initialize server " << inited << "\n";

    auto reuse = ser.ReuseAddress(1);
    if ( reuse < 0 )
        std::cout << "reuse address failed\n";

    if ( auto fd = ser.GetFd(); fd )
        std::cout << "FD: " << fd << "\n";

    Address addr("127.0.0.1", atoi(argv[1]));
    if ( auto bind = ser.Bind(addr); bind > 0 )
        std::cout << "bind initialized fd with specified Address: " << addr.GetIP() << ", Port: " << addr.GetPort() <<
            "sccessfully\n";

    if ( auto listen = ser.Listen(); listen < 0 )
        std::cout << "listen on initialized fd: " << listen << "\n";

    std::vector<server::tcp::TcpSocket> fds;
    fds.reserve(10);
    int count = 1;
    while (count <= 10)
    {
        auto clientSock = ser.Accept();
        if ( clientSock < 0 )
        {
            std::cout << "accept on server failed\n";
            continue;
        }
        std::cout << "accepted connection fd: " << clientSock << "\n";
        fds.emplace_back(clientSock);

        ++count;
    }

    std::string reply = "Hey, You. How you doing?";
    std::for_each(fds.begin(), fds.end(), [&] (auto & sock) {

        sock.Nonblocking(true);

        char buf[128];
        std::string str(128, ' ');
        // auto read = sock.readData(buf, 128);
        auto read = sock.ReadData(str.data(), 128);
        struct person data;
        if ( read > 0 )
        {
            // std::cout << "fd: " << sock.fd() << ", from client: " << buf << "\n";
            std::cout << "fd: " << sock.GetFd() << ", from client: " << str << "\n";
            // std::memcpy(&data, buf, sizeof(data));
            std::memcpy(&data, str.data(), sizeof(data));
            std::cout << "data: { Persion[ len: " << data.len << ", id: " << data.id << ", age: " << data.age << "]}\n";
        }
        else
            std::cout << "can't receive message from client\n";


        auto wrote = sock.WriteData(reply.data(), reply.size());
        if ( wrote > 0 )
            std::cout << "fd: " << sock.GetFd() << ", to client: " << reply << "\n";
        else
            std::cout << "can't send message to client.\n";
    });

    return 0;
}
