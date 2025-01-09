#ifndef FILE_H
#define FILE_H

#include "IODevice.h"
#include <condition_variable>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace server {
namespace io {

typedef std::vector<char> Buffer;

class File : IODevice
{
public:
    File(std::string const & filename)
        : _filename(filename)
          , _file(_filename)
          , _access(false)
          , _writing(false)
          , _mutex()
          , _cv()
    {
        if ( _file.is_open() )
        {
            _access = true;
            IODevice::SetOpenMode(Mode::RW);
        }
    }

    ~File() { Close(); }

    std::size_t write(Buffer const & buf, std::size_t size)
    {
        return write(buf.data(), 0, size);
    }

    std::size_t write(Buffer const & buf, std::size_t offset, std::size_t size)
    {
        return write(buf.data(), offset, size);
    }

    Buffer read(std::size_t size)
    {
        if ( !_access )
            return {};
        return read(0, size);
    }

    Buffer readAll()
    {
        std::size_t totalSize = 0;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            totalSize = size();
        }

        return read(totalSize);
    }

    Buffer read(std::size_t offset, std::size_t size)
    {
        if ( !_access )
            return {};

        Buffer buf(size, ' ');
        auto got = read(buf.data(), offset, size);
        if ( got < size )
            buf.reserve(size);

        return buf;
    }

    void Close() override
    {
        if ( _access )
        {
            _file.flush();
            _file.close();
        }
    }

    std::size_t size() const
    {
        if ( !_access )
            return 0;

        namespace fs = std::filesystem;
        return fs::file_size(_filename);
    }

    std::string filename() const { return _filename; }

    void disableWrite()
    {
        if ( !_access )
            return;
        IODevice::SetOpenMode(Mode::R);
    }

    void disableRead()
    {
        if ( !_access )
            return;
        IODevice::SetOpenMode(Mode::W);
    }

    void disableAll()
    {
        if ( !_access )
            return;
        IODevice::SetOpenMode(Mode::NONE);
    }

    bool Open(Mode mode) override
    {
        if ( !_access )
            return false;

        IODevice::SetOpenMode(mode);
        return _access;
    }

protected:
    std::size_t write(char const * const buf, std::size_t offset, std::size_t size)
    {
        std::unique_lock<std::mutex> lk(_mutex);
        if ( !_access || size <= 0 )
            return 0;

        _writing = true;
        _cv.wait(lk, [this] () { return _writing; });

        if ( offset > 0 )
            _file.seekp(offset, std::ios::beg);
        auto beginPos = _file.tellp();
        auto getPos = _file.tellg();
        _file.write(buf, size);
        if ( _file.fail() )
            return 0;
        auto endPos = _file.tellp();
        _file.seekg(getPos, std::ios::beg);
        if ( offset > 0 )
            _file.seekp(0, std::ios::end);

        _writing = false;
        lk.unlock();
        _cv.notify_one();

        return endPos - beginPos;
    }

    std::size_t read(char * const buf, std::size_t offset, std::size_t size)
    {
        std::unique_lock<std::mutex> lk(_mutex);
        if ( !_access || size <= 0 )
            return 0;

        auto totalSize= this->size();
        if ( offset > totalSize )
            return {};

        auto validSize = ( size < totalSize ? size : totalSize );

        _cv.wait(lk, [this] () { return !_writing; });

        if ( offset > 0 )
            _file.seekg(offset, std::ios::beg);
        auto beginPos = _file.tellg();
        _file.read(buf, validSize);
        if ( _file.fail() )
            return 0;
        auto endPos = _file.tellp();

        lk.unlock();

        return endPos - beginPos;
    }

    ssize_t WriteData(char const * const data, std::size_t size) override
    {
        return write(data, 0, size);
    }

    ssize_t ReadData(char * data, std::size_t size) override
    {
        return read(data, 0, size);
    }

    std::size_t SkipSome(std::size_t size) override
    {
        return 0;
    }


private:
    std::string _filename;  // absolute path
    std::fstream _file;
    bool _access;
    bool _writing;
    std::mutex _mutex;
    std::condition_variable_any _cv;
};

} // namespace io
} // namespace server
#endif // !FILE_H
