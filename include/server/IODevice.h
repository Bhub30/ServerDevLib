#ifndef IODEVICE_H
#define IODEVICE_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace server {
namespace io {

enum Mode : uint8_t {
    R = 1,
    W = 2,
    RW = 4,
    NONE = 8,
};

class IODevice {
    constexpr static std::size_t BUFSIZE = 1024;
public:
    IODevice()
        : _mode(Mode::NONE)
          , _open(true)
          , _head(0)
          , _tail(0)
          , _size(0)
          , _buf(BUFSIZE, ' ')
    {}

    IODevice(IODevice &&) = default;
    IODevice(const IODevice &) = default;
    IODevice &operator=(IODevice &&) = default;
    IODevice &operator=(const IODevice &) = default;
    virtual ~IODevice() = default;

    bool IsWritable() { return HasWriteMode(); }

    bool IsReadable() { return HasReadMode(); }

    std::size_t Peek(char * data, std::size_t size)
    {
        if ( data == nullptr || !HasReadMode() || _size == 0 || IsSequential() )
            return 0;

        auto exactSize = ( _size > size ? size : _size );
        auto len = ( _tail >= _head ? _tail - _head : BUFSIZE - _head );
        auto validSize = ( exactSize < len ? exactSize : len );
        std::memcpy(data, &_buf.at(_head), validSize);
        if ( _tail < _head && validSize < exactSize )
            std::memcpy(data + len, &_buf.at(0), exactSize - validSize);

        return exactSize;
    }

    std::vector<char> Peek(std::size_t size)
    {
        if ( _size == 0 || !HasReadMode() || IsSequential() )
            return {};

        auto validSize = ( _size > size ? size : _size );
        std::vector<char> data(validSize, ' ');
        Peek(data.data(), validSize);

        return data;
    }

    std::size_t Read(char * data, std::size_t size)
    {
        if ( data == nullptr|| !HasReadMode() || size == 0 )
            return 0;

        if ( BUFSIZE > _size )
        {
            std:;size_t readed = 0;
            auto len = ( _tail >= _head ) ? BUFSIZE - _tail : _head - _tail;
            readed = ReadData(&_buf.at(_tail), len);
            if ( _tail > _head && readed >= len )
                    readed += ReadData(&_buf.at(0), _head);

            _tail = ( _tail + readed ) % BUFSIZE;
            _size += readed;
        }

        auto validSize = ( size < _size ? size : _size );
        Peek(data, validSize);
        _head = ( _head + validSize ) % BUFSIZE;
        _size -= validSize;

        return validSize;
    }

    std::vector<char> Read(std::size_t size)
    {
        if ( !HasReadMode() || size == 0 )
            return {};

        std::vector<char> data(size, ' ');
        Read(data.data(), size);
        if ( data.size() < size )
            data.reserve(data.size());

        return data;
    }

    std::vector<char> ReadAll()
    {
        return Read(_size);
    }

    std::size_t Write(char const * const data, std::size_t size)
    {
        if ( data == nullptr || !HasWriteMode() || size == 0)
            return 0;
        return WriteData(data, size);
    }

    std::size_t Write(std::vector<char> const & data)
    {
        return Write(data.data(), data.size());
    }

    std::size_t Skip(std::size_t size) { return SkipSome(size); }

    Mode GetMode() const { return _mode; }

    bool IsOpen() const { return _open; }

    virtual bool Open(Mode mode) { return _open; }

    virtual std::size_t AvailabelBytesToRead() const { return _size; }

    virtual std::size_t AvailabelBytesToWrtie() const { return 0; }

    virtual void Close() {}

    virtual bool IsSequential() const { return false; }

    virtual std::size_t ReceivedSize() const
    {
        if ( IsSequential() )
            return 0;
        return _size;
    }

    virtual std::size_t Pos() const
    {
        if ( IsSequential() )
            return 0;
        return _head;
    }

    virtual bool Seek(std::size_t pos)
    {
        if ( IsSequential() )
            return false;
        if ( !HasReadMode() )
            return false;
        if ( (_head + pos) % BUFSIZE >= _tail )
            return false;

        _head = (_head + pos) % BUFSIZE;
        return true;
    }

protected:
    virtual ssize_t ReadData(char * data, std::size_t size) = 0;

    virtual ssize_t WriteData(char const * const data, std::size_t size) = 0;

    virtual std::size_t SkipSome(std::size_t size)
    {
        if ( !HasReadMode() )
            return 0;

        auto validSize = ( size < _size ) ? size : _size;
        _head = ( _head + validSize ) % BUFSIZE;

        return validSize;
    }

    void SetOpenMode(Mode mode)
    {
        _mode = mode;
        _open = ( _mode & Mode::RW ||  _mode & Mode::R ||  _mode & Mode::W );
    }

private:
    bool HasReadMode() const { return _mode & Mode::RW || _mode & Mode::R; }

    bool HasWriteMode() const { return _mode & Mode::RW || _mode & Mode::W; }

private:
    Mode        _mode;
    bool        _open;
    std::size_t _head;
    std::size_t _tail;
    std::size_t _size;
    std::string _buf;
};

} // namespace io
} // namespace server

#endif // !IODEVICE_H
