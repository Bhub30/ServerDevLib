#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <set>
#include <sys/socket.h>
#include <unistd.h>
#include "Address.h"

namespace server {
namespace tcp {

class TcpServer
{
public:
    TcpServer()
        : _fd(0)
          , _access(false)
          , _saveAllConnection(false)
          , _addr()
          , _allAccepted()
    {}
    TcpServer(TcpServer const &) = delete;
    TcpServer & operator=(TcpServer const &) = delete;
    ~TcpServer() { Shutdown(); }

    bool Init()
    {
        if ( _access )
            return false;

        if ( _fd = socket(AF_INET, SOCK_STREAM, 0); _fd )
            _access = true;

        return _access;
    }

    int GetFd() const { return _fd; };

    Address const & address() const { return _addr; }

    int Bind(Address addr)
    {
        struct sockaddr_in in;
        std::memset(&in, 0, sizeof(in));
        in.sin_family = addr.Family();
        if ( auto converted = inet_pton(AF_INET, addr.GetIP().c_str(), &in.sin_addr); converted <= 0 )
            return converted;
        in.sin_port = htons(addr.GetPort());
        _addr = std::move(addr);

        return ::bind(_fd, (struct sockaddr *) & in, sizeof(in));
    }

    int Listen(int n = 512)
    {
        if ( !_access )
            return -1;
        return ::listen(_fd, n);
    }

    int Accept()
    {
        if ( !_access )
            return -1;

        struct sockaddr_in in;
        socklen_t len = sizeof(in);
        auto accepted =  ::accept(_fd, (struct sockaddr *) &in, &len);
        if ( accepted > 0 )
            _allAccepted.emplace(accepted);

        return accepted;
    }

    int ReuseAddress(int opt)
    {
        if ( !_access )
            return -1;

        return ::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, static_cast<int *>(&opt), sizeof(opt) );
    }

    int DisableNagle(int opt)
    {
        if ( !_access )
            return -1;

        return ::setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY , &opt , sizeof(opt));
    }

    void Shutdown()
    {
        if ( !_access )
            return;
        _access = true;
        ::close(_fd);
        std::for_each(_allAccepted.begin(), _allAccepted.end(), [] (auto & fd) { ::close(fd); });
        std::set<int> s{};
        _allAccepted.swap(s);
    }

    void AutoSaveAcceptedFD(bool b = true) { _saveAllConnection = b; }

private:
    int _fd;
    bool _access;
    bool _saveAllConnection;
    Address _addr;
    std::set<int> _allAccepted;
};

} // namespace tcp
} // namespace server

#endif // !SERVER_H
