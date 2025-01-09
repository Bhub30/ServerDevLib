#ifndef LOGMESSAGE_H
#define LOGMESSAGE_H

#include "LogStream.h"
#include <cstdlib>
#include <execinfo.h>
#include <functional>
#include <iomanip>
#include <thread>

namespace server {
namespace log {

enum LogLevel
{
    INFO = 0,
    WARN = 1,
    ERROR = 2,
    FATAL = 3,
};

class LogMessage
{
    constexpr const static uint64_t MAX_MESSAGE_LEN = 3000;

    using SendToCb = std::function<void(LogMessage &&)>;

    inline static char const * const LeftSeparator          = "[ ";
    inline static char const * const MiddleSeparator        = " ";
    inline static char const * const RightSeparator         = " ]";
    inline static char const * const BoundSeparator         = " --- ";
    inline static char const * const LevelToString[]        = {
        "INFO ",
        "WARN",
        "ERROR",
        "FATAL",
    };

    struct Data {
        Data()
            : _has_been_flushed(false)
              , _stream(_message, MAX_MESSAGE_LEN)
              , _id(std::this_thread::get_id())
        {}

        char _message[MAX_MESSAGE_LEN];
        bool _has_been_flushed;
        LogStream _stream;
        std::thread::id _id;
    };

public:
    LogMessage(int level, char const * filename, char const * func, int line)
        : _level(level)
          , _filename(filename)
          , _func(func)
          , _line(line)
          , _data(nullptr)
    {
        Init();
    }
    LogMessage(LogMessage const & msg) = delete;
    LogMessage & operator=(LogMessage const & msg) = delete;
    LogMessage(LogMessage && msg)
        : _level(msg._level)
          , _filename(msg._filename)
          , _func(msg._func)
          , _line(msg._line)
          , _data(msg._data)
    {
        msg._data = nullptr;
    }
    LogMessage & operator=(LogMessage && msg)
    {
        if ( &msg == this )
            return *this;

        _level      = msg._level;
        _filename   = msg._filename;
        _func       = msg._func;
        _line       = msg._line;
        _data       = msg._data;
        msg._data   = nullptr;

        return *this;
    }
    ~LogMessage()
    {
        Flush();
        delete _data;
        _data = nullptr;
    }

    int const & GetLogLevel() const { return _level; }

    std::thread::id const & GetTID() const { return _data->_id; }

    LogStream & Stream() { return _data->_stream; }

    static void SetSentToCallback(SendToCb sendto)
    {
        _sendto = std::move(sendto);
    }

private:
    void Init()
    {
        _data = new Data;

        auto now = std::chrono::system_clock::now();
        std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&timestamp, &tm);
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        auto id = std::this_thread::get_id();

        Stream() << LeftSeparator << LevelToString[_level] << MiddleSeparator
                 << _data->_id << MiddleSeparator
                << ( 1900 + tm.tm_year ) << '-' << std::setw(2) << std::setfill('0')
                << ( tm.tm_mon + 1 ) << '-'  << std::setw(2)
                << tm.tm_mday << MiddleSeparator << std::setw(2)
                << tm.tm_hour << ':' << std::setw(2)
                << tm.tm_min << ':' << std::setw(2)
                << tm.tm_sec << '.' << std::setw(3)
                << static_cast<int>(milliseconds.count())
                << MiddleSeparator<< std::setw(-20) << std::setfill(' ')
                << _filename << ':' << std::setw(15)
                << _func << ':' << _line
                << RightSeparator << BoundSeparator;
    }

    void Flush()
    {
        if ( !_data || _data->_has_been_flushed )
            return;
        _data->_stream << "\n\0";
        if ( _level == FATAL )
            StackTrace();
        if ( _sendto )
        {
            _data->_has_been_flushed = true;
            _sendto(std::move(*this));
        }
    }

    void StackTrace()
    {
        constexpr static int MAX_FRAME = 70;
        void *addr_list[MAX_FRAME];
        int nptrs = backtrace(addr_list, sizeof(addr_list) / sizeof(void *));
        if ( nptrs == 0 )
            return;

        char **symbols_list = backtrace_symbols(addr_list, nptrs);
        Stream() << "Stack trace of thread " << GetTID() << ":\n";
        for (int i = 0; i < nptrs; i++) {
            Stream() << "\t" << symbols_list[i] << "\n";
        }
        free(symbols_list);
    }

private:
    int _level;
    char const * _filename;
    char const * _func;
    int _line;
    Data * _data;
    inline static SendToCb _sendto;
};

} // namespace log
} // namespace server

#endif // !LOGMESSAGE_H
