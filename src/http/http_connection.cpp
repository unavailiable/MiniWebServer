#include "../../include/http/http_connection.hpp"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>
#include <unistd.h>

HttpConnection::HttpConnection(int fd, const sockaddr_in& address, const std::string& docRoot)
    : m_address(address), m_socket(fd), m_docRoot(docRoot), m_readIndex(0), m_writeIndex(0),
      m_iovCount(0), m_bytesToSend(0), m_keepAlive(false)
{
    std::memset(m_iov, 0, sizeof(m_iov));
}

int HttpConnection::fd() const noexcept
{
    return m_socket.get();
}

bool HttpConnection::read()
{
    while (true)
    {
        if (m_readIndex >= m_readBuffer.size())
        {
            return false;
        }

        ssize_t bytes = recv(m_socket.get(), m_readBuffer.data() + m_readIndex,
                             m_readBuffer.size() - m_readIndex, 0);

        if (bytes > 0)
        {
            m_readIndex += static_cast<std::size_t>(bytes);

            continue;
        }

        if (bytes == 0)
        {
            return false;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return true;
        }

        return false;
    }
}

HttpConnection::ProcessResult HttpConnection::process()
{
    ParseResult result = m_parser.parse(m_readBuffer.data(), m_readIndex);

    if (result == ParseResult::NoRequest)
    {
        return ProcessResult::NeedMoreData;
    }

    if (result == ParseResult::BadRequest)
    {
        m_keepAlive = false;

        buildResponse(HttpCode::BadRequest);

        return ProcessResult::ReadyToWrite;
    }

    m_keepAlive = m_parser.request().keepAlive;

    HttpCode code = doRequest();

    buildResponse(code);

    return ProcessResult::ReadyToWrite;
}

HttpConnection::HttpCode HttpConnection::doRequest()
{
    const HttpRequest& request = m_parser.request();

    /*
     * 当前 v2 第一版仍只提供静态 GET。
     */
    if (request.method != HttpMethod::GET)
    {
        return HttpCode::BadRequest;
    }

    std::string url = request.url;

    /*
     * 去掉 query string。
     */
    std::size_t query = url.find('?');

    if (query != std::string::npos)
    {
        url.erase(query);
    }

    if (url == "/")
    {
        url = "/index.html";
    }

    std::string fullPath = m_docRoot + url;

    char realPath[PATH_MAX];

    if (!realpath(fullPath.c_str(), realPath))
    {
        return HttpCode::NoResource;
    }

    std::size_t rootLength = m_docRoot.size();

    /*
     * 防止目录穿越。
     */
    if (std::strncmp(realPath, m_docRoot.c_str(), rootLength) != 0)
    {
        return HttpCode::Forbidden;
    }

    if (realPath[rootLength] != '/' && realPath[rootLength] != '\0')
    {
        return HttpCode::Forbidden;
    }

    if (stat(realPath, &m_fileStat) < 0)
    {
        return HttpCode::NoResource;
    }

    if (S_ISDIR(m_fileStat.st_mode))
    {
        return HttpCode::Forbidden;
    }

    UniqueFd fileFd(open(realPath, O_RDONLY));

    if (!fileFd)
    {
        return HttpCode::Forbidden;
    }

    if (!m_file.map(fileFd.get(), static_cast<std::size_t>(m_fileStat.st_size)))
    {
        return HttpCode::InternalError;
    }

    return HttpCode::FileRequest;
}

bool HttpConnection::buildResponse(HttpCode code)
{
    m_writeIndex = 0;

    m_bytesToSend = 0;

    m_iovCount = 0;

    std::memset(m_iov, 0, sizeof(m_iov));

    switch (code)
    {
    case HttpCode::FileRequest:
    {
        if (!appendResponse("HTTP/1.1 200 OK\r\n"))
        {
            return false;
        }

        if (!appendResponse("Content-Length: %zu\r\n", m_file.size()))
        {
            return false;
        }

        if (!appendResponse("Content-Type: %s\r\n", contentType()))
        {
            return false;
        }

        if (!appendResponse("Connection: %s\r\n", m_keepAlive ? "keep-alive" : "close"))
        {
            return false;
        }

        if (!appendResponse("\r\n"))
        {
            return false;
        }

        break;
    }

    case HttpCode::BadRequest:
    {
        const char* body = "400 Bad Request\n";

        appendResponse("HTTP/1.1 400 Bad Request\r\n");

        appendResponse("Content-Length: %zu\r\n", std::strlen(body));

        appendResponse("Content-Type: text/plain\r\n");

        appendResponse("Connection: close\r\n\r\n");

        appendResponse("%s", body);

        m_keepAlive = false;

        break;
    }

    case HttpCode::NoResource:
    {
        const char* body = "404 Not Found\n";

        appendResponse("HTTP/1.1 404 Not Found\r\n");

        appendResponse("Content-Length: %zu\r\n", std::strlen(body));

        appendResponse("Content-Type: text/plain\r\n");

        appendResponse("Connection: %s\r\n\r\n", m_keepAlive ? "keep-alive" : "close");

        appendResponse("%s", body);

        break;
    }

    case HttpCode::Forbidden:
    {
        const char* body = "403 Forbidden\n";

        appendResponse("HTTP/1.1 403 Forbidden\r\n");

        appendResponse("Content-Length: %zu\r\n", std::strlen(body));

        appendResponse("Content-Type: text/plain\r\n");

        appendResponse("Connection: %s\r\n\r\n", m_keepAlive ? "keep-alive" : "close");

        appendResponse("%s", body);

        break;
    }

    case HttpCode::InternalError:
    {
        const char* body = "500 Internal Server Error\n";

        appendResponse("HTTP/1.1 500 Internal Server Error\r\n");

        appendResponse("Content-Length: %zu\r\n", std::strlen(body));

        appendResponse("Content-Type: text/plain\r\n");

        appendResponse("Connection: close\r\n\r\n");

        appendResponse("%s", body);

        m_keepAlive = false;

        break;
    }
    }

    setupIov();

    return true;
}

bool HttpConnection::appendResponse(const char* format, ...)
{
    if (m_writeIndex >= m_writeBuffer.size())
    {
        return false;
    }

    std::size_t remaining = m_writeBuffer.size() - m_writeIndex;

    va_list arguments;

    va_start(arguments, format);

    int length = std::vsnprintf(m_writeBuffer.data() + m_writeIndex, remaining, format, arguments);

    va_end(arguments);

    if (length < 0 || static_cast<std::size_t>(length) >= remaining)
    {
        return false;
    }

    m_writeIndex += static_cast<std::size_t>(length);

    return true;
}

void HttpConnection::setupIov()
{
    m_iov[0].iov_base = m_writeBuffer.data();

    m_iov[0].iov_len = m_writeIndex;

    m_iovCount = 1;

    m_bytesToSend = m_writeIndex;

    if (m_file.size() > 0)
    {
        m_iov[1].iov_base = const_cast<char*>(m_file.data());

        m_iov[1].iov_len = m_file.size();

        m_iovCount = 2;

        m_bytesToSend += m_file.size();
    }
}

HttpConnection::WriteResult HttpConnection::write()
{
    while (m_bytesToSend > 0)
    {
        ssize_t bytes = writev(m_socket.get(), m_iov, m_iovCount);

        if (bytes > 0)
        {
            std::size_t written = static_cast<std::size_t>(bytes);

            consumeWritten(written);

            m_bytesToSend -= written;

            continue;
        }

        if (bytes < 0 && errno == EINTR)
        {
            continue;
        }

        if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return WriteResult::NeedWrite;
        }

        m_file.reset();

        return WriteResult::Close;
    }

    m_file.reset();

    if (m_keepAlive)
    {
        resetForNextRequest();

        return WriteResult::NeedRead;
    }

    return WriteResult::Close;
}

void HttpConnection::consumeWritten(std::size_t bytes)
{
    for (int i = 0; i < m_iovCount && bytes > 0; ++i)
    {
        std::size_t length = m_iov[i].iov_len;

        if (bytes >= length)
        {
            bytes -= length;

            m_iov[i].iov_base = static_cast<char*>(m_iov[i].iov_base) + length;

            m_iov[i].iov_len = 0;
        }
        else
        {
            m_iov[i].iov_base = static_cast<char*>(m_iov[i].iov_base) + bytes;

            m_iov[i].iov_len -= bytes;

            bytes = 0;
        }
    }
}

void HttpConnection::resetForNextRequest()
{
    m_parser.reset();

    m_readIndex = 0;

    m_writeIndex = 0;

    m_bytesToSend = 0;

    m_iovCount = 0;

    m_keepAlive = false;

    std::memset(m_iov, 0, sizeof(m_iov));
}

const char* HttpConnection::contentType() const noexcept
{
    const std::string& url = m_parser.request().url;

    if (url.find(".html") != std::string::npos)
    {
        return "text/html; charset=utf-8";
    }

    if (url.find(".jpg") != std::string::npos || url.find(".jpeg") != std::string::npos)
    {
        return "image/jpeg";
    }

    if (url.find(".png") != std::string::npos)
    {
        return "image/png";
    }

    if (url.find(".css") != std::string::npos)
    {
        return "text/css";
    }

    if (url.find(".js") != std::string::npos)
    {
        return "application/javascript";
    }

    return "application/octet-stream";
}