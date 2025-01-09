#ifndef SINK_H
#define SINK_H

#include <cstddef>

namespace server {
namespace log {
namespace sink {

class Sink
{
public:
    Sink() = default;
    virtual ~Sink() = default;

    virtual void Flush(char * str, std::size_t n) =0;
};

} // namespace dest
} // namespace log
} // namespace server

#endif // !SINK_H
