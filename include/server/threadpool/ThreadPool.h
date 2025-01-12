#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Timer.h"

namespace server {
namespace threadpool {

struct ThreadPoolConfig
{
    uint32_t min_core_thread;
    uint32_t max_thread;
    bool start_monitor_timer;
    uint32_t monitor_period; // microsecond
    uint8_t verify_count;
};

inline static ThreadPoolConfig GlobalThreadPoolConfig = {
    1,
    std::thread::hardware_concurrency(),
    false,
    30000,
    3,
};

class ThreadPool {
    enum ThredShold {
        FIRST = 50,
        SECOND = 70,
        THIRD = 90,
    };

    enum Stat : uint8_t {
        EMPTY = 0,
        ACTIVE = 1,
        DEAD = 2,
    };

public:
    ThreadPool()
        : _workers()
          , _workerStat()
          , _tasks()
          , _queueMutex()
          , _cv()
          , _config(GlobalThreadPoolConfig)
          , _timer()
          , _prevIdleTime(0)
          , _prevTotalTime(0)
          , _thredShold(FIRST)
          , _stop(false)
    {
        Init();
    }

    ThreadPool(ThreadPoolConfig config)
          : _workers()
          , _tasks()
          , _queueMutex()
          , _cv()
          , _config(config)
          , _timer()
          , _prevIdleTime(0)
          , _prevTotalTime(0)
          , _thredShold(FIRST)
          , _stop(false)
    {
        Init();
    }

    ThreadPool(ThreadPool const &) = delete;
    ThreadPool & operator=(ThreadPool const &) = delete;

    ~ThreadPool()
    {
        Shutdown();
    }

    static ThreadPool & GetGlobalThreadPool()
    {
        static std::shared_ptr<ThreadPool> pool;
        static std::mutex mx;
        if ( pool == nullptr ) {
            std::lock_guard<std::mutex> lk(mx);
            if ( pool == nullptr )
                pool = std::make_shared<ThreadPool>();
        }
        return *pool;
    }

    // template<typename Fn,
    //     typename ... Args,
    //     typename = std::enable_if_t<std::is_void_v<std::invoke_result_t<std::decay_t<Fn>, std::decay_t<Args>...>>>>
    // void EnqueueTask(Fn && fn, Args &&... args)
    // {
    //     {
    //         std::lock_guard<std::mutex> lock(_queueMutex);
    //         auto f = std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
    //         _tasks.push(f);
    //     }
    //     _cv.notify_one();
    // }

    template<typename Fn,
        typename... Args,
        typename R = std::invoke_result_t<std::decay_t<Fn>, std::decay_t<Args>...>>
        // typename = std::enable_if_t<!std::is_void_v<R>>>
    std::future<R> EnqueueTask(Fn && fn, Args &&... args)
    {
        auto func = std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
        auto task = std::make_shared<std::packaged_task<R()>>(std::move(func));
        std::future<R> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(_queueMutex);
            _tasks.push([task] () { (*task)(); });
        }
        _cv.notify_one();
        return result;
    }

    void Shutdown()
    {
        if ( _stop )
            return;
        _stop = true;
        {
            std::lock_guard<std::mutex> lk(_queueMutex);
            _timer.Stop();
            _tasks = {};
            for ( auto & pair : _workerStat )
                pair.second = DEAD;
        }
        _cv.notify_all();
        for ( auto & pair : _workers )
            pair.second.join();
    }

private:
    void Init()
    {
        _workers.reserve(_config.max_thread);
        _workerStat.reserve(_config.max_thread);

        for ( size_t i = 0; i < _config.min_core_thread; ++i )
        {
            std::thread t(&ThreadPool::WorkerThread, this);
            auto id = t.get_id();
            _workers.emplace(id, std::move(t));
            _workerStat.emplace(id, ACTIVE);
        }

        GetCPUStats(_prevIdleTime, _prevIdleTime);

        if ( _config.start_monitor_timer && _config.min_core_thread < _config.max_thread )
        {
            _timer.SetCallback(_config.monitor_period, [this] { Monitor(); });
            std::thread t(&Timer::Start, &_timer);
            _workers.emplace(t.get_id(), std::move(t));
        }
    }

    void WorkerThread()
    {
        std::thread::id id = std::this_thread::get_id();
        auto it = _workerStat.find(id);
        while ( it != _workerStat.end() && it->second == ACTIVE ) {
            it->second = EMPTY;
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(_queueMutex);
                _cv.wait(lock, [this] { return _stop || !_tasks.empty(); });
                if ( it->second == DEAD )
                    break;
                task = std::move(_tasks.front());
                _tasks.pop();
                it->second = ACTIVE;
            }
            task();
        }
    }
    void GetCPUStats(long &idle_time, long &total_time) {
        static std::ifstream file("/proc/stat");
        std::string line;
        std::getline(file, line);
        file.close();

        std::istringstream ss(line);
        std::string cpu;
        long user, nice, system, idle, iowait, irq, softirq, steal;
        ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        idle_time = idle + iowait;
        total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    }

    void GetThredShold(long const & d)
    {
        if ( d < FIRST )
            _thredShold = FIRST;
        else if ( d < SECOND )
            _thredShold = SECOND;
        else
            _thredShold = THIRD;
    }

    void Monitor()
    {
        if (_config.min_core_thread < _config.max_thread )
            return;

        long idle_time, total_time;
        GetCPUStats(idle_time, total_time);
        long idle_diff = idle_time - _prevIdleTime;
        long total_diff = total_time - _prevTotalTime;
        _prevIdleTime = idle_time;
        _prevTotalTime = total_time;
        double percent = (1.0 - (idle_diff * 1.0 / total_diff)) * 100.0;
        GetThredShold(static_cast<long>(percent));
        Adjust(percent);
    }

    void Adjust(double & d)
    {
        static uint8_t countForAdd = 0;
        static uint8_t countForSub = 0;
        if ( d < (1.0 * _thredShold) && !_tasks.empty() ) {
            ++countForAdd;
            if ( countForAdd > _config.verify_count
                    && _workers.size() < _config.max_thread
            ) {
                std::thread t(&ThreadPool::WorkerThread, this);
                auto id = t.get_id();
                _workers.emplace(id, std::move(t));
                _workerStat.emplace(id, ACTIVE);
                countForAdd = 0;
            }
        } else if ( d < (1.0 * _thredShold) && _tasks.empty() ) {
            ++countForSub;
            if ( countForSub > _config.verify_count
                    && _workers.size() > _config.min_core_thread
            ) {
                countForSub = 0;
                std::vector<std::thread::id> vec;
                {
                    std::lock_guard<std::mutex> lk(_queueMutex);
                    vec.reserve(_workers.size());
                    for ( auto & pair : _workerStat )
                    {
                        auto & second = pair.second;
                        if ( pair.second == EMPTY && _tasks.empty() )
                        {
                            pair.second = DEAD;
                            vec.emplace_back(pair.first);
                        }
                    }
                    _cv.notify_all();
                }
                for ( auto & id : vec) {
                    _workers.erase(id);
                    _workerStat.erase(id);
                }
            }
        }

    }

private:
    std::unordered_map<std::thread::id, std::thread> _workers;
    std::unordered_map<std::thread::id, Stat> _workerStat;
    std::queue<std::function<void()>> _tasks;
    std::mutex _queueMutex;
    std::condition_variable _cv;
    ThreadPoolConfig _config;
    Timer _timer;
    long _prevIdleTime;
    long _prevTotalTime;
    long _thredShold;
    bool _stop;

};

} // namespace threadpool
} // namespace server

#endif // !THREADPOLL_H
