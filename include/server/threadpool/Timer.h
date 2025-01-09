#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <functional>
#include <thread>

namespace server {
namespace threadpool {

class Timer
{
public:
    Timer()
        : _interval(0)
          , _stop(false)
          , _shotedCount(0)
          , _cb()
    {
    }

    Timer(uint64_t interval, std::function<void()> cb)
        : _interval(interval)
          , _stop(false)
          , _shotedCount(0)
          , _cb(std::move(cb))
    {
        Start();
    }

    Timer(Timer const &) = delete;
    Timer & operator=(Timer const &) = delete;
    ~Timer() { _stop = true; }

    void Start()
    {
        while ( !_stop )
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(_interval));
            if ( _cb )
                _cb();
            ++_shotedCount;
        }

    }

    void Stop() { _stop = true; }

    uint64_t Interval() const { return _interval; }

    void SetInterval(uint64_t interval) { _interval = interval; }

    std::function<void()> Callback() const { return _cb; }

    void SetCallback(uint64_t interval, std::function<void()> cb)
    {
        _interval = interval;
        _cb = std::move(cb);
        _shotedCount = 0;
    }

    uint64_t ShotedCount() const { return _shotedCount; }

    void Reset(Timer const & timer)
    {
        _interval = timer.Interval();
        _cb = std::move(timer.Callback());
    }

private:
    uint64_t _interval;
    bool _stop;
    uint64_t _shotedCount;
    std::function<void()> _cb;
};

} // namespace timer
} // namespace server

#endif // !TIMER_H
