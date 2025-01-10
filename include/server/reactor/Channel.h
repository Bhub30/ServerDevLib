#ifndef CHANNEL_H
#define CHANNEL_H

#include <cerrno>
#include <cstddef>
#include <functional>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include "Demultiplexer.h"
#include "Handler.h"
#include "server/reactor/AcceptHandler.h"

namespace server {
namespace reactor {

class Channel {
public:
    constexpr static std::size_t BUFSIZE = 128;

    typedef std::function<void(int)> DataReadyNotifaction;
    typedef std::function<void(int)> ClosedNotifaction;
    typedef std::function<void(int receivedBytes, int err, std::string data)> ReceiveCB;
    typedef std::function<void(int sentBytes, int err, std::string data)> SendCB;
    typedef std::function<void(int)> ClosedCb;

public:
    Channel(int fd, Demultiplexer * const ptr = nullptr)
        : _fd(fd)
          , _active(fd > 0 ? true : false)
          , _receivedCount(0)
          , _sendingBuf(BUFSIZE, '-')
          , _receivedBuf(BUFSIZE, '-')
          , _sendMutex()
          , _receiveMutex()
          , _demultiplexer(ptr)
    {
        _sendingBuf.clear();
    }
    Channel(Channel &&) = delete;
    Channel(const Channel &) = delete;
    Channel &operator=(Channel &&) = delete;
    Channel &operator=(const Channel &) = delete;
    ~Channel()
    {
        _active = false;
        _demultiplexer = nullptr;
        DisableSend();
        DisableReceive();
    };

    void Read()
    {
        if ( !_active )
            return;
        int got = 0;
        {
            std::lock_guard<std::mutex> lk(_receiveMutex);
            auto validSize = _receivedBuf.capacity() - _receivedCount;
            do {
                if ( validSize == got )
                    _receivedBuf.reserve(1.5 * _receivedBuf.capacity());
                _receivedCount += got;
                validSize = _receivedBuf.capacity() - _receivedCount;
            } while ( ( got = ::read(_fd, _receivedBuf.data() + _receivedCount, validSize) ) > 0 );
            if ( _globalReceivedCb )
                _globalReceivedCb(_receivedCount, errno, _receivedBuf.substr(0, _receivedCount));
            char ip_str[INET_ADDRSTRLEN];
            uint16_t port = 0;
            AcceptHandler::GetPeerHostInfo(ip_str, INET_ADDRSTRLEN, _fd, port);
            LOG(INFO) << "Has been read data { FD = " << _fd << ", IP = " << ip_str << ", PORT = " << port << ", DATA: " << _receivedBuf << " }";
        }
        if ( got == 0 )
        {
            DisableReceive();
            DisableSend();
            Inactive();
            if ( _closedNotify )
                _closedNotify(_fd);
            return;
        }
        if ( _dataReadyNotify )
            _dataReadyNotify(_fd);
    }

    void Write()
    {
        if ( !_active )
            return;
        std::lock_guard<std::mutex> lk(_sendMutex);
        auto size = _sendingBuf.size();
        if ( size < 1 )
        {
            auto events = _demultiplexer->GetEvents();
            _demultiplexer->ModifyEvent(_fd, events & ~EPOLLOUT);
            return;
        }
        auto sent = ::write(_fd, _sendingBuf.data(), size);
        char ip_str[INET_ADDRSTRLEN];
        uint16_t port = 0;
        AcceptHandler::GetPeerHostInfo(ip_str, INET_ADDRSTRLEN, _fd, port);
        LOG(INFO) << "Has been read data { FD = " << _fd << ", IP = " << ip_str << ", PORT = " << port << ", Buffering Data: " << _sendingBuf << ", Sent DATA: " << _sendingBuf.substr(0, sent) << " }";
        if ( sent > 0 && _globalSentCb )
            _globalSentCb(sent, errno, _sendingBuf.substr(0, sent));
        if ( sent > 0 ) {
            _sendingBuf.erase(0 , sent);
        } else if ( sent == 0 ) {
            if ( _demultiplexer )
                _demultiplexer->ModifyEvent(_fd, _demultiplexer->GetEvents());
        } else {
            DisableSend();
            return;
        }
    }

    void NotifyWriteEvent(std::string data)
    {
        if ( !_active )
            return;
        {
            std::lock_guard<std::mutex> lk(_sendMutex);
            auto size = _sendingBuf.size();
            auto waitToSend = data.length();
            if ( auto capacity = _sendingBuf.capacity(); size + waitToSend >= capacity ) {
                _sendingBuf.reserve(capacity + waitToSend);
                _sendingBuf.insert(size, data);
            }
            else if ( size == 0 )
                _sendingBuf.insert(_sendingBuf.begin(), data.begin(), data.end());
            else
                _sendingBuf.insert(size, data);
        }

        if ( _demultiplexer == nullptr )
            return;
        auto events = _demultiplexer->GetEvents();
        _demultiplexer->ModifyEvent(_fd, events | EPOLLOUT);
    }

    void SetDemultiplexer(Demultiplexer * const ptr) { _demultiplexer = ptr; }

    static void SetDataReadyNotify(DataReadyNotifaction notify) { _dataReadyNotify = std::move(notify); }
    static void SetClosedNotify(ClosedNotifaction notify) { _closedNotify = std::move(notify); }
    static void SetGlobalReceiveCallback(ReceiveCB cb) { _globalReceivedCb = std::move(cb); }
    static void SetGlobalSendCallback(SendCB cb) { _globalSentCb = std::move(cb); }
    static void SetGlobalClosedCallBack(ClosedCb cb) { _globalClosedCb = std::move(cb); }

    std::string GetReceivedData()
    {
        if ( !_active )
            return {};
        std::lock_guard<std::mutex> lk(_receiveMutex);
        if ( _receivedCount < 1 )
            return {};
        std::string data = std::move(_receivedBuf.substr(0, _receivedCount));
        _receivedCount = 0;
        return data;
    }

    void DisableReceive()
    {
        if ( !_active )
            return;
        ::shutdown(_fd, SHUT_RD);
    }

    void DisableSend()
    {
        if ( !_active )
            return;
        ::shutdown(_fd, SHUT_WR);
    }

    int GetHandle() const { return _fd; }

    bool Active() const{ return _active; }
    void Inactive() { _active = false; }

private:
    int _fd;
    bool _active;
    int _receivedCount;
    std::string _sendingBuf;
    std::string _receivedBuf;
    std::mutex _sendMutex;
    std::mutex _receiveMutex;
    Demultiplexer * _demultiplexer;
    inline static DataReadyNotifaction _dataReadyNotify;
    inline static ClosedNotifaction _closedNotify;
    inline static ReceiveCB _globalReceivedCb;
    inline static SendCB _globalSentCb;
    inline static ClosedCb _globalClosedCb;
};

} // namespace reactor
} // namespace server

#endif // !CHANNEL_H
