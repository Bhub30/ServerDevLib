#ifndef LOGFILE_H
#define LOGFILE_H

#include "LogSink.h"
#include "LogStream.h"
#include <ctime>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

namespace server {
namespace log {
namespace sink {

struct LogFileConfig {
    std::string _logs_dir;
    uint64_t _per_file_size;
};

static LogFileConfig GlobalConfig = {
    "/tmp/",
    250 * 1024, // KB
};

inline static void GetProgramName(std::ostream & stream)
{
    char result[PATH_MAX];
    auto count = readlink("/proc/self/exe", result, PATH_MAX);
    if ( readlink("/proc/self/exe", result, PATH_MAX) == -1 )
        return;
    stream << basename(result);
}

inline static void GetHostName(std::ostream & stream)
{
    char hostname[HOST_NAME_MAX];
    auto count = gethostname(hostname, HOST_NAME_MAX);
    if ( count != 0 )
        return;
    stream << hostname;
}

inline static void GetLocalTime(std::ostream & stream)
{
    std::time_t now = std::time(nullptr);
    std::tm *local_time = std::localtime(&now);

    stream << ( local_time->tm_year + 1900 ) << std::setw(2) << std::setfill('0')
            << ( local_time->tm_mon + 1 ) << std::setw(2) << std::setfill('0')
            << local_time->tm_mday << '-' << std::setw(2) << std::setfill('0')
            << local_time->tm_hour << std::setw(2) << std::setfill('0')
            << local_time->tm_min << std::setw(2) << std::setfill('0')
            << local_time->tm_sec;
}

inline static void GetPID(std::ostream & stream) { stream << getpid(); }

using LogFileBaseNameHandler = std::function<void(std::string & stream)>;

class LogFile : public Sink
{
    constexpr static int MAX_FILENAME_SIZE = 60;
    char filename[MAX_FILENAME_SIZE];

public:
    LogFile()
        : _filename()
          , _total_size(0)
          , _file()
    {
        InitFileName();
    }

    ~LogFile()
    {
        _file.flush();
        _file.close();
    }

    void Flush(char * str, std::size_t n) override
    {
        if ( _total_size > GlobalConfig._per_file_size )
            Rotate();

        _total_size += n;
        _file.write(str, n);
        if ( _file.fail() )
            throw std::runtime_error("can't append log to file.");
    }

    static void SetLogFileBaseNameHandler(LogFileBaseNameHandler handler)
    {
        _handler = std::move(handler);
    }

private:
    void InitFileName()
    {
        LogStream stream(filename, MAX_FILENAME_SIZE);
        stream << GlobalConfig._logs_dir;
        GetProgramName(stream);
        stream << '_';
        GetHostName(stream);
        stream << '_';
        GetLocalTime(stream);
        stream << '_';
        GetPID(stream);
        stream << ".log\0";
        _filename = stream.str();
        std::cout << _filename << "\n";

        if ( _file.is_open() )
        {
            _file.flush();
            _file.close();
        }
        _file.open(_filename, std::ios::out | std::ios::app);
        if ( !_file.is_open() )
            throw std::runtime_error("can't open log file");
    }

    void Rotate()
    {
        InitFileName();
        _total_size = 0;
    }

private:
    std::string _filename;
    uint64_t _total_size;
    std::ofstream _file;
    static LogFileBaseNameHandler _handler;
};

} // namespace dest
} // namespace log
} // namespace server

#endif // !LOGFILE_H
