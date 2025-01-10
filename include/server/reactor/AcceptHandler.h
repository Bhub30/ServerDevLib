#ifndef ACCEPTHANDLER_H
#define ACCEPTHANDLER_H

#include "server/logging/LogMessage.h"
#include "server/logging/Logging.h"
#include "Handler.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>

namespace server {
namespace reactor {

class AcceptHandler : public Handler
{
public:
    AcceptHandler(int fd)
        : _accepted(-1)
            , _master(fd)
    {}

    AcceptHandler(AcceptHandler &&) = default;
    AcceptHandler(const AcceptHandler &) = default;
    AcceptHandler & operator=(AcceptHandler &&) = default;
    AcceptHandler & operator=(const AcceptHandler &) = default;
    ~AcceptHandler() {}

    void HandleEvent(uint32_t event) override
    {
        if ( event & EPOLLIN )
        {
            struct sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            socklen_t len = sizeof(addr);

            _accepted = ::accept(_master, (struct sockaddr *) &addr, &len);
            LOG_IF(ERROR, _accepted < 0) << "Failed to accept new connection.";

            char ip_str[INET_ADDRSTRLEN];
            uint16_t port = 0;
            GetPeerHostInfo(ip_str, INET_ADDRSTRLEN, _accepted, port);
            LOG(INFO) << "Accecpting new connection: { FD = " << _accepted << ", IP = " << ip_str << ", PORT = " << port << " }.";

            int flags = fcntl(_accepted, F_GETFL, 0);
            fcntl(_accepted, F_SETFL, flags | O_NONBLOCK);
        }
    }

    static void GetPeerHostInfo(char * buf, int n, int & fd, uint16_t & port)
    {
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        if ( getpeername(fd, (struct sockaddr*)&peer_addr, &peer_addr_len) == -1 )
        {
            LOG(ERROR) << "Failed to get peer ip address and port.";
            return;
        }
        inet_ntop(AF_INET, &peer_addr.sin_addr, buf, peer_addr_len);
        port = ntohs(peer_addr.sin_port);
    }

    void SetChannel(std::shared_ptr<Channel> channel) override {}
    std::shared_ptr<Channel> GetChannel() override { return nullptr; }

    int getAccepted() const { return _accepted; }

private:
    int _master;
    int _accepted;
};

} // namespace reactor
} // namespace server

#endif // !ACCEPTHANDLER_H
