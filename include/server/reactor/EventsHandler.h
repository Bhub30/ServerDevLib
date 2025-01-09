#ifndef EVENTSHANDLER_H
#define EVENTSHANDLER_H

#include "Handler.h"
#include "Channel.h"
#include <memory>
#include <sys/epoll.h>

namespace server {
namespace reactor {

class EventsHandler : public Handler {
public:
    EventsHandler()
        :_channel(nullptr)
    {}
    EventsHandler(EventsHandler &&) = default;
    EventsHandler(const EventsHandler &) = default;
    EventsHandler &operator=(EventsHandler &&) = default;
    EventsHandler &operator=(const EventsHandler &) = default;
    ~EventsHandler() {}

    void HandleEvent(uint32_t events) override
    {
        if ( _channel == nullptr || !_channel->Active() )
            return;
        if ( events & EPOLLERR || events & EPOLLHUP || events & EPOLLRDHUP )
        {
            _channel->DisableSend();
            _channel->DisableReceive();
        }
        else if ( events & EPOLLIN )
            _channel->Read();
        else if ( events & EPOLLOUT )
            _channel->Write();
    }

    void SetChannel(std::shared_ptr<Channel> channel) override
    {
        _channel = std::move(channel);
    }

    std::shared_ptr<Channel> GetChannel() override { return _channel; }

private:
    std::shared_ptr<Channel> _channel;
};
} // namespace reactor
} // namespace server

#endif // !EVENTSHANDLER_H
