#ifndef HANDLER_H
#define HANDLER_H

#include <memory>

namespace server {
namespace reactor {

class Channel;

class Handler {
public:
    Handler() = default;
    Handler(Handler &&) = default;
    Handler(const Handler &) = default;
    Handler &operator=(Handler &&) = default;
    Handler &operator=(const Handler &) = default;
    virtual ~Handler() = default;

    virtual void HandleEvent(uint32_t event) =0;
    virtual void SetChannel(std::shared_ptr<Channel> channel) =0;
    virtual std::shared_ptr<Channel> GetChannel() =0;
};

} // namespace reactor
} // namespace server

#endif // !HANDLER_H
