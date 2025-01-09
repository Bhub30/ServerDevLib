#ifndef LOGGER_H
#define LOGGER_H

#include "LogFile.h"
#include "LogSink.h"
#include "LogMessage.h"
#include "SysLog.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

#define LOG(level) server::log::LogMessage(server::log::level, __FILE_NAME__, __FUNCTION__, __LINE__).Stream()

#define LOG_IF(level, condition)    \
    static_cast<void>(0),           \
        !(condition)                \
            ? (void)0               \
            : server::log::internal::Voidfy() & LOG(level)

namespace server {
namespace log {

using server::log::LogMessage;

namespace internal {

struct Voidfy
{
    constexpr void operator&(std::ostream &) const noexcept {}
};

class Logger
{
public:
    Logger()
        : _stop(false)
          , _log_with_waiting(false)
          , _waiting_ms(0)
          , _level(LogLevel::INFO)
          , _buf()
          , _backup_buf()
          , _dest_vec()
          , _mx()
          , _cv()
          , _flush_thread(&Logger::Flush, this)
    {
        LogMessage::SetSentToCallback([this] (LogMessage && msg) { Buffering(std::move(msg)); });
    }
    ~Logger()
    {
        _stop = true;
        _cv.notify_all();
        _flush_thread.join();
    }

    void SetLogLevel(LogLevel level) { _level = level; }

    LogLevel GetLogLevel() { return _level; }

    static Logger * GetLogger()
    {
        static std::unique_ptr<Logger> logger;
        if ( !logger )
        {
            static std::mutex mx;
            std::lock_guard<std::mutex> lk(mx);
            if ( !logger )
                logger = std::make_unique<Logger>();
        }
        return logger.get();
    }

    bool LogWithWaiting() const { return _log_with_waiting; }

    void LogWithWaiting(int ms) { _log_with_waiting = ms; }

    void Buffering(LogMessage && msg)
    {
        {
            std::lock_guard<std::mutex> lk(_mx);
            _buf.emplace_back(std::move(msg));
        }
        if ( !_log_with_waiting )
            _cv.notify_one();
    }

    void AddLogSink(std::shared_ptr<sink::Sink> const & sink)
    {
        _dest_vec.emplace_back(std::move(sink));
    }

private:
    void Flush()
    {
        while ( !_stop )
        {
            {
                std::unique_lock<std::mutex> lk(_mx);
                if ( _log_with_waiting )
                    _cv.wait_for(lk, std::chrono::milliseconds(_waiting_ms), [this] { return _buf.size() > 0 ; });
                else
                    _cv.wait(lk, [this] { return _stop || _buf.size() > 0 ; });
                if ( _stop )
                    return;
                if ( _buf.size() > 0 )
                    _buf.swap(_backup_buf);
            }

            for ( auto & msg : _backup_buf )
            {
                for ( auto & dest : _dest_vec )
                    dest->Flush(msg.Stream().str(), msg.Stream().pcount());
                if ( msg.GetLogLevel() == FATAL )
                {
                    _stop = true;
                    _buf.clear();
                    _backup_buf.clear();
                    _dest_vec.clear();
                    std::abort();
                }
            }
            _backup_buf.clear();
        }
    }

private:
    bool _stop;
    bool _log_with_waiting;
    int _waiting_ms;
    LogLevel _level;
    std::vector<LogMessage> _buf;
    std::vector<LogMessage> _backup_buf;
    std::vector<std::shared_ptr<sink::Sink>> _dest_vec;
    std::mutex _mx;
    std::condition_variable _cv;
    std::thread _flush_thread;
};

} // namespace internal

inline static void InitializeLogger()
{
    internal::Logger::GetLogger()->AddLogSink(std::make_shared<sink::SysLog>());
}

inline static void SetLogLevel(LogLevel level)
{
    internal::Logger::GetLogger()->SetLogLevel(level);
}

inline static LogLevel GetLogLevel()
{
    return internal::Logger::GetLogger()->GetLogLevel();
}

inline bool LogWithWaiting()
{
    return internal::Logger::GetLogger()->LogWithWaiting();
}

inline void LogWithWaiting(int ms)
{
    return internal::Logger::GetLogger()->LogWithWaiting(ms);
}

inline void SetLogFileDir(std::string dir)
{
    if ( dir.back() != '/' )
        dir.push_back('/');
    using namespace std::filesystem;
    sink::GlobalConfig._logs_dir = dir;
    auto existed = exists(dir);
    if ( !existed )
        create_directory(dir);
}

inline void SetPerFileMaxSize(uint64_t maxkb)
{
    sink::GlobalConfig._per_file_size = maxkb;
}

inline void SetLogFileBaseName(sink::LogFileBaseNameHandler handler)
{
    sink::LogFile::SetLogFileBaseNameHandler(std::move(handler));
}

inline void SetLogFileConfig(sink::LogFileConfig const & config)
{
    SetLogFileDir(config._logs_dir);
    sink::GlobalConfig._per_file_size = config._per_file_size;
}

} // namespace log
} // namespace server

#endif // !LOGGER_H
