#ifndef SYSLOG_H
#define SYSLOG_H

#include "LogSink.h"
#include <algorithm>
#include <cstddef>
#include <iostream>

namespace server {
namespace log {
namespace sink {

class SysLog : public Sink
{
public:
    SysLog() {}
    ~SysLog() {}

    void Flush(char * str, std::size_t n) override
    {
        std::copy_n(str, n, std::ostreambuf_iterator<char>{std::cout});
    }
};

} // namespace dest
} // namespace log
} // namespace server

#endif // !SYSLOG_H
