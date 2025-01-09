#ifndef ADDRESS_H
#define ADDRESS_H

#include <cstdint>
#include <string>
#include <sys/socket.h>

class Address
{
public:
    Address() = default;
    Address(char const * const ip, std::uint16_t port, sa_family_t family = AF_INET)
        : _ip(ip)
          , _port(port)
          , _family(family)
    {}
    Address(Address const & addr) = default;
    Address & operator=(Address const & addr) = default;
    Address(Address && addr) = default;
    Address & operator=(Address && addr) = default;
    ~Address() = default;

    sa_family_t Family() const { return _family; };

    std::string const & GetIP() const { return _ip; };

    std::uint16_t GetPort() const { return _port; };

private:
    std::string _ip;
    std::uint16_t _port;
    sa_family_t _family;
};

#endif // !ADDRESS_H
