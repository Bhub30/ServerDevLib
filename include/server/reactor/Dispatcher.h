#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <sys/epoll.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include "AcceptHandler.h"
#include "Demultiplexer.h"
#include "EventsHandler.h"
#include "Handler.h"
#include "server/logging/LogMessage.h"
#include "server/logging/Logging.h"
#include "server/threadpool/ThreadPool.h"

namespace server {
namespace reactor {

using namespace server::threadpool;
class Dispatcher {
public:
   typedef std::unordered_map<int, std::shared_ptr<Channel>> ChannelMap;
   typedef std::unordered_map<int, std::shared_ptr<Handler>> HandlerMap;
   typedef std::vector<std::shared_ptr<Dispatcher>> DispatcherVec;
public:
    Dispatcher()
        : _stop(false)
          , _enableSlave(false)
          , _masterfd(0)
          , _demultiplexer()
          , _slaves()
          , _events(512)
          , _handlers()
          , _pool(ThreadPool::GetGlobalThreadPool())
          , _pendingFn()
    {}

    Dispatcher(Dispatcher &&) = delete;
    Dispatcher(const Dispatcher &) = delete;
    Dispatcher &operator=(Dispatcher &&) = delete;
    Dispatcher &operator=(const Dispatcher &) = delete;
    ~Dispatcher() { Shutdown(); }

    void Dispatch()
    {
        while ( !_stop ) {
            int numEvents = _demultiplexer.WaitForEvents(_events);
            if ( numEvents <= 0 )
                continue;
            auto it = _events.begin();
            std::for_each(it, it + numEvents, [this] (struct epoll_event & event) {
                auto fd = event.data.fd;
                auto it = _handlers.find(fd);
                int accepted = 0;
                if ( it == _handlers.end() )
                    return;
                if ( fd == _masterfd ) {
                    auto acceptor = dynamic_cast<AcceptHandler *>(it->second.get());
                    acceptor->HandleEvent(event.events);
                    HandleNewConnection(acceptor->getAccepted());
                } else {
                    auto handler = it->second;
                    auto events = event.events;
                    _pool.EnqueueTask([handler, events] { handler->HandleEvent(events); });
                }

                HandleUnexpected(fd, event.events);

                std::vector<std::function<void()>> pendingFn;
                {
                    std::lock_guard<std::mutex> lk(_pendingMx);
                    pendingFn = std::move(_pendingFn);
                }
                for ( auto & fn : pendingFn )
                    fn();
            });
        }
    }

    bool RegisterHandler(int fd, std::shared_ptr<Handler> handler)
    {
        if ( _stop || fd < 0 )
            return false;
        _demultiplexer.RegisterFd(fd);
        std::lock_guard<std::mutex> lk(_globalMx);
        return _handlers.emplace(fd, handler).second;
    }

    int RemoveHandler(int fd)
    {
        if ( _stop || fd < 0 )
            return -1;
        std::lock_guard<std::mutex> lk(_globalMx);
        auto it = _allChannel.find(fd);
        if ( it != _allChannel.end() && it->first == fd ) {
            _allChannel.erase(fd);
            it->second->Inactive();
        }
        return _handlers.erase(fd);
    }

    void EnableSlave(bool b)
    {
        if ( _stop )
            return;
        _enableSlave = b;
    }

    void AddSlaveDispatcher(int n = 1)
    {
        if ( _stop || !_enableSlave )
            return;
        for ( int i = 0; i < n; ++i )
        {
            _slaves.emplace_back(std::make_shared<Dispatcher>());
            auto & slave = _slaves.back();
            _pool.EnqueueTask(&Dispatcher::Dispatch, slave.get());;
        }
    }

    Demultiplexer * const GetDemultiplexer()
    {
        if ( _stop )
            return nullptr;
        return &_demultiplexer;
    }

    bool Stop() const { return _stop; }

    void Shutdown()
    {
        if ( _stop )
            return;
        _stop = true;
        std::for_each(_handlers.begin(), _handlers.end(), [&] (auto & pair) { ::close(pair.first); });
        _handlers.clear();
        if ( _enableSlave )
        {
            std::for_each(_slaves.begin(), _slaves.end(), [&] (auto & d) { d->Shutdown(); });
            _slaves.clear();
        }
        if ( _masterfd != 0 )
        {
            _pool.Shutdown();
            std::for_each(_allChannel.begin(), _allChannel.end(), [this] (auto & pair) { pair.second->Inactive(); });
            _allChannel.clear();
        }
        _demultiplexer.Shutdown();
    }

    void SetMasterFD(int fd)
    {
        if ( fd < 0 ) return;
        _masterfd = fd;
        RegisterHandler(fd, std::make_shared<AcceptHandler>(fd));
    }

    std::shared_ptr<Channel> GetChannel(int fd)
    {
        std::lock_guard<std::mutex> lk(_globalMx);
        auto it = _allChannel.find(fd);
        return it == _allChannel.end() ? nullptr : it->second;
    }

    ChannelMap & GetAllChannel() { return _allChannel; }

    ChannelMap ExtractExistedChannel()
    {
        std::lock_guard<std::mutex> lk(_globalMx);
        auto tmp = std::unordered_map<int, std::shared_ptr<Channel>>();
        _allChannel.swap(tmp);
        return tmp;
    }

    void RestoreAllChannel(ChannelMap & channels)
    {
        std::lock_guard<std::mutex> lk(_globalMx);
        for( auto & fd : _waitToRemovedChannel) {
            channels.erase(fd);
        }
        _waitToRemovedChannel.clear();
        _allChannel.insert( std::make_move_iterator(channels.begin()), std::make_move_iterator(channels.end()) );
    }

    server::threadpool::ThreadPool & GetThreadPool() { return _pool; }

    void AddPendingFunctor(std::function<void()> fn)
    {
        if ( !_enableSlave )
        {
            std::lock_guard<std::mutex> lk(_pendingMx);
            _pendingFn.emplace_back(std::move(fn));
        }
        else {
            auto & slave = _slaves.at(DispatchToSlave());
            slave->AddPendingFunctor(std::move(fn));
        }
    }

private:
    void HandleUnexpected(int fd, uint32_t events)
    {
        if ( events & EPOLLERR || events & EPOLLHUP || events & EPOLLRDHUP ) {
            _handlers.erase(fd);
            _demultiplexer.RemoveFd(fd);
            ::close(fd);

            char ip_str[INET_ADDRSTRLEN];
            uint16_t port = 0;
            AcceptHandler::GetPeerHostInfo(ip_str, INET_ADDRSTRLEN, fd, port);
            LOG_IF(INFO, port != 0) << "Close accepted connection: [ FD = " << fd << ", IP = " << ip_str << ", PORT = " << port << " ].";

            std::lock_guard<std::mutex> lk(_globalMx);
            auto it = _allChannel.find(fd);
            if ( it != _allChannel.end() && it->first == fd ) {
                it->second->Inactive();
                _allChannel.erase(fd);
            } else {
                _waitToRemovedChannel.emplace_back(fd);
            }
        }
    }

    void HandleNewConnection(int fd)
    {
        std::shared_ptr<Handler> handler = std::make_shared<EventsHandler>();
        auto channel = std::make_shared<Channel>(fd, &_demultiplexer);
        handler->SetChannel(channel);
        {
            std::lock_guard<std::mutex> lk(_globalMx);
            _allChannel.emplace(fd, channel);
        }
        if ( !_enableSlave || !_slaves.size() )
            RegisterHandler(fd, handler);
        else
        {
            auto & slave = _slaves.at(DispatchToSlave());
            slave->RegisterHandler(fd, handler);
        }
    }

    int DispatchToSlave()
    {
        std::lock_guard<std::mutex> lk(_globalMx);
        auto count = _slaves.size();
        int cur = 0;
        return ( cur++ ) % count;
    }

private:
    bool _stop;
    bool _enableSlave;
    int _masterfd;
    Demultiplexer _demultiplexer;
    DispatcherVec _slaves;
    std::vector<struct epoll_event> _events;
    HandlerMap _handlers;
    std::vector<int> _waitToRemovedChannel;
    inline static ChannelMap _allChannel = {};
    std::mutex _globalMx;
    std::mutex _pendingMx;
    server::threadpool::ThreadPool & _pool;
    std::vector<std::function<void()>> _pendingFn;
};

} // namespace reactor
} // namespace server

#endif // !DISPATCHER_H
