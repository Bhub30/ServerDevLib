#ifndef LOGSTREAM_H
#define LOGSTREAM_H

#include <ostream>
#include <streambuf>

namespace server {
namespace log {

class LogStreamBuf : public std::streambuf
{
public:
    // "len" must be >= 2 to account for the '\n' and '\0'.
    LogStreamBuf(char * buf, int len) { setp(buf, buf + len - 2); }

    LogStreamBuf(LogStreamBuf const &) = default;
    LogStreamBuf & operator=(LogStreamBuf const &) = default;
    LogStreamBuf(LogStreamBuf && buf) = default;
    LogStreamBuf & operator=(LogStreamBuf &&) = default;

    int_type overflow(int_type ch) { return ch; }

    size_t pcount() const { return static_cast<size_t>(pptr() - pbase()); }

    char* pbase() const { return std::streambuf::pbase(); }
};

class LogStream : public std::ostream
{
public:
    LogStream(char * buf, int len) : std::ostream(nullptr)
        , _streambuf(buf, len)
    {
        rdbuf(&_streambuf);
    }
    LogStream & operator=(LogStream &&) = default;

    std::size_t pcount() const { return _streambuf.pcount(); }
    char* pbase() const { return _streambuf.pbase(); }
    char* str() const { return pbase(); }

private:
    LogStreamBuf _streambuf;
};

} // namespace log
} // namespace server

#endif // !LOGSTREAM_H
