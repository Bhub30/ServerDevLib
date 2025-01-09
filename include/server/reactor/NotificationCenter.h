#ifndef NOTIFICAtION_CENTER_H
#define NOTIFICAtION_CENTER_H

#include "Dispatcher.h"
#include "Channel.h"
#include "server/threadpool/ThreadPool.h"
#include <future>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace server {
namespace reactor {

class NotificationCenter {
public:
    enum state {
        ONE = 0x1,
        MORE,
        MOREPLUS
    };

public:
    NotificationCenter(Dispatcher & dpr)
        : _mx()
          , _dispatcher(dpr)
          , _channelMap(dpr.GetAllChannel())
          , _waitToHandleFD()
          , _pool(dpr.GetThreadPool())
    {
        Channel::SetDataReadyNotify([this] (int fd) { NotifyDataReady(fd); });
        Channel::SetClosedNotify([this] (int fd) { NotifyClose(fd); });
    }
    NotificationCenter(NotificationCenter &&) = delete;
    NotificationCenter(const NotificationCenter &) = delete;
    NotificationCenter &operator=(NotificationCenter &&) = delete;
    NotificationCenter &operator=(const NotificationCenter &) = delete;
    ~NotificationCenter() {}

    void NotifyDataReady(int fd)
    {
        auto it = _waitToHandleFD.find(fd);
        if ( it == _waitToHandleFD.end()) {
            _waitToHandleFD.emplace(fd, MORE);
            return;
        }
        std::lock_guard<std::mutex> lk(_mx);
        if ( it->second == MORE )
            it->second = MOREPLUS;
        else if ( it->second == ONE )
            it->second = MORE;
    }

    void NotifyClose(int fd)
    {
        std::lock_guard<std::mutex> lk(_mx);
        if ( _waitToHandleFD.find(fd) != _waitToHandleFD.end() )
            _waitToHandleFD.erase(fd);
    }

    void NotifyResponseReady(int fd, std::string data)
    {
        auto channel = _dispatcher.GetChannel(fd);
        if ( channel == nullptr )
            return;
        channel->NotifyWriteEvent(std::move(data));
    }

    template<typename Fn,
        typename... Args,
        typename FdArg = int,
        typename DataArg = std::string,
        typename R = std::invoke_result_t<std::decay_t<Fn>, FdArg, DataArg, std::decay_t<Args>...>,
        typename = std::enable_if_t<!std::is_void_v<R>>>
    auto HandleReadyData(Fn && fn, Args &&... args)
    {
        std::unordered_map<int, state> copy;
        {
            std::lock_guard<std::mutex> lk(_mx);
            copy = _waitToHandleFD;
        }

        std::vector<std::future<R>> result;
        result.reserve(copy.size());
        for ( auto & it : copy ) {
            auto channel = _dispatcher.GetChannel(it.first);
            if ( channel == nullptr || it.second == ONE )
                continue;

            std::future<R> res = _pool.EnqueueTask(std::forward<Fn>(fn), channel->GetHandle(), channel->GetReceivedData(), std::forward<Args>(args)...);
            result.emplace_back(std::move(res));
            // have been finished the operation for dealing with data
            std::lock_guard<std::mutex> lk(_mx);
            auto at = _waitToHandleFD.find(it.first);
            if ( at->second == MOREPLUS )
                at->second = MORE;
            else if ( at->second == MORE )
                at->second = ONE;
        }
        return result;
    }

private:
    std::mutex _mx;
    Dispatcher & _dispatcher;
    Dispatcher::ChannelMap & _channelMap;
    std::unordered_map<int, state> _waitToHandleFD;;
    server::threadpool::ThreadPool & _pool;
};

} // namespace reactor
} // namespace server

#endif // !NOTIFICAtION_CENTER_H
