#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include <array>
#include <cstddef>
#include <netinet/in.h>
#include <string>
#include <sys/stat.h>
#include <sys/uio.h>

#include "../core/unique_fd.hpp"
#include "http_parser.hpp"
#include "mapped_file.hpp"

class HttpConnection
{
  public:
    enum class ProcessResult
    {
        NeedMoreData,
        ReadyToWrite
    };

    enum class WriteResult
    {
        NeedWrite,
        NeedRead,
        Close
    };

  private:
    enum class HttpCode
    {
        FileRequest,
        BadRequest,
        NoResource,
        Forbidden,
        InternalError
    };

  public:
    HttpConnection(int fd, const sockaddr_in& address, const std::string& docRoot);

    ~HttpConnection() = default;

    HttpConnection(const HttpConnection&) = delete;

    HttpConnection& operator=(const HttpConnection&) = delete;

  public:
    int fd() const noexcept;

    bool read();

    ProcessResult process();

    WriteResult write();

  private:
    HttpCode doRequest();

    bool buildResponse(HttpCode code);

    bool appendResponse(const char* format, ...);

    void setupIov();

    void consumeWritten(std::size_t bytes);

    void resetForNextRequest();

    const char* contentType() const noexcept;

  private:
    static constexpr std::size_t READ_BUFFER_SIZE = 8192;

    static constexpr std::size_t WRITE_BUFFER_SIZE = 4096;

  private:
    sockaddr_in m_address;

    UniqueFd m_socket;

    std::string m_docRoot;

    HttpParser m_parser;

    std::array<char, READ_BUFFER_SIZE> m_readBuffer;

    std::array<char, WRITE_BUFFER_SIZE> m_writeBuffer;

    std::size_t m_readIndex;

    std::size_t m_writeIndex;

    struct iovec m_iov[2];

    int m_iovCount;

    std::size_t m_bytesToSend;

    MappedFile m_file;

    struct stat m_fileStat;

    bool m_keepAlive;
};

#endif