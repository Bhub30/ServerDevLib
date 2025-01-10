#ifndef DEMULTIPLEXER_H
#define DEMULTIPLEXER_H

#include "server/logging/LogMessage.h"
#include "server/logging/Logging.h"
#include <cstdint>
#include <memory>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

namespace server {
namespace reactor {

class Demultiplexer {
public:
    inline static uint32_t DEFAULT_EVENTS = EPOLLET | EPOLLIN | EPOLLHUP | EPOLLERR;

public:
    typedef std::vector<struct epoll_event> EventsVec;
    typedef std::shared_ptr<EventsVec> EventsVecPtr;

public:
    Demultiplexer()
        : _fd(-1)
          , _events(DEFAULT_EVENTS)
    {
        Init();
    }

    Demultiplexer(Demultiplexer &&) = default;
    Demultiplexer(const Demultiplexer &) = delete;
    Demultiplexer &operator=(Demultiplexer &&) = default;
    Demultiplexer &operator=(const Demultiplexer &) = delete;
    ~Demultiplexer() { Shutdown(); }

    int Init()
    {
        _fd = ::epoll_create(1);
        LOG_IF(ERROR, _fd < 0) << "Failed to create epollfd";
        return _fd;
    }

    bool Valid() { return _fd; }

    uint32_t & GetEvents() { return _events; }
    uint32_t const & SetEvents() const { return _events; }
    void SetEvents(uint32_t events) { _events = events; }

    int RegisterFd(int fd)
    {
        if ( fd < 0 || !Valid() )
            return -1;
        return RegisterFd(fd, _events, nullptr);
    }

    int RegisterFd(int fd, uint32_t events)
    {
        if ( !Valid() || fd < 0 )
            return -1;
        return RegisterFd(fd, events, nullptr);
    }

    int RegisterFd(int fd, uint32_t events, void * data)
    {
        if ( !Valid() || fd < 0 )
            return -1;

        struct epoll_event event;
        event.events = events;
        if ( data != nullptr )
            event.data.ptr = data;
        else
            event.data.fd = fd;
        auto ret = epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event);
        LOG_IF(ERROR, ret < 0) << "Failed to register FD " << fd << " with EVENTS " << events << " to epollfd " << _fd;
        return ret;
    }

    int ModifyEvent(int fd, uint32_t events)
    {
        if ( !Valid() || fd < 0 )
            return -1;
        return ModifyEvent(fd, events, nullptr);
    }

    int ModifyEvent(int fd, void * data)
    {
        if ( !Valid() || fd < 0 )
            return -1;
        return ModifyEvent(fd, _events, data);
    }

    int ModifyEvent(int fd, uint32_t events, void * data)
    {
        if ( !Valid() || fd < 0 )
            return -1;

        struct epoll_event event;
        event.events = events;
        if ( data != nullptr )
            event.data.ptr = data;
        else
            event.data.fd = fd;
        auto ret = epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event);
        LOG_IF(ERROR, ret < 0) << "Failed to modify FD " << fd << " orignal EVENTS to " << events << " inside epollfd " << _fd;
        return ret;
    }

    int RemoveFd(int fd)
    {
        if ( !Valid() || fd < 0 )
            return -1;
        auto ret = epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr);
        LOG_IF(ERROR, ret < 0) << "Failed to unregister FD " << fd << " from epollfd " << _fd;
        return ret;
    }

    int WaitForEvents(EventsVec & events)
    {
        if ( !Valid() )
            return _fd;

        auto size = events.size();
        if ( size  < 1 )
            return -1;
        return epoll_wait(_fd, events.data(), size, -1);
    }

    int WaitForEvents(EventsVecPtr ptr)
    {
        if ( !Valid() )
            return _fd;
        return WaitForEvents(*ptr.get());
    }

    void Shutdown()
    {
        if( !Valid() )
            return;
        ::close(_fd);
        LOG(INFO) << "Close epollfd " << _fd;
    }

private:
    int _fd;
    uint32_t _events;
};

} // namespace reactor
} // namespace server

#endif // !DEMULTIPLEXER_H
