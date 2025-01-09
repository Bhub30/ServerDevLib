#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include "Address.h"
#include "IODevice.h"

namespace server {
namespace tcp {

class TcpSocket : public io::IODevice
{
public:
    TcpSocket()
        : _fd(-1)
          , _valid(false)
    {
        _fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if ( _fd > 0 )
            _valid = true;
    }

    TcpSocket(int fd)
        : _fd(fd)
          , _valid(fd > 0 ? true : false)
    {}

    ~TcpSocket() {}

    bool operator==(TcpSocket const & sock) const { return _fd == sock._fd; }

    friend bool operator==(TcpSocket const & lhs, TcpSocket const & rhs);

    int GetFd() { return _fd; }

    bool Valid() const { return _valid; }

    void Close() override
    {
        if ( _valid )
        {
            _valid = false;
            ::close(_fd);
        }
    }

    Address GetAddress()
    {
        if ( !_valid )
            return {};

        struct sockaddr_in in;
        std::memset(&in, 0, sizeof(in));

        socklen_t len = sizeof(in);
        if ( ::getpeername(_fd, (struct sockaddr *) &in, &len) < 0)
            return {};

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &in.sin_addr, ip_str, sizeof(ip_str));
        auto port = ntohs(in.sin_port);
        return { ip_str, port, in.sin_family };
    }

    int Nonblocking(bool flag)
    {
        if ( !_valid )
            return _fd;
        int flags = ::fcntl(_fd, F_GETFL, 0);
        if ( flags < 0 )
            return flags;
        if ( flag )
            return ::fcntl(_fd, F_SETFL, flags | O_NONBLOCK);
        return ::fcntl(_fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    int AvailableBytes()
    {
        if ( !_valid )
            return -1;
        int available{0};
        if ( ioctl(_fd, FIONREAD, &available) < 0 )
            return -1;
        return available;
    }

    bool Open(io::Mode mode) override
    {
        if ( !Valid() ) {
            io::IODevice::SetOpenMode(io::Mode::NONE);
            return false;
        }
        io::IODevice::SetOpenMode(io::Mode::RW);
        return true;
    }

    bool IsSequential() const override { return true; }

    ssize_t ReadData(char * data, std::size_t size) override
    {
        if ( !Valid() )
            return 0;
        return ::read(_fd, data, size);
    }

    ssize_t WriteData(char const * const data, std::size_t size) override
    {
        if ( !Valid() )
            return 0;
        return ::write(_fd, data, size);
    }

private:
    int _fd;
    bool _valid;
};

inline bool operator==(TcpSocket const & lhs, TcpSocket const & rhs) { return lhs._fd == rhs._fd; }

} // namespace tcp
} // namespace server
#endif // !SOCKET_H
