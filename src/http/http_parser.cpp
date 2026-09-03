#include "../../include/http/http_parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

HttpRequest::HttpRequest() : method(HttpMethod::UNKNOWN), contentLength(0), keepAlive(false) {}

HttpParser::HttpParser() {}

void HttpParser::reset()
{
    m_request = HttpRequest();
}

const HttpRequest& HttpParser::request() const noexcept
{
    return m_request;
}

ParseResult HttpParser::parse(const char* buffer, std::size_t length)
{
    reset();

    if (!buffer || length == 0)
    {
        return ParseResult::NoRequest;
    }

    std::string data(buffer, length);

    std::size_t headerEnd = data.find("\r\n\r\n");

    if (headerEnd == std::string::npos)
    {
        return ParseResult::NoRequest;
    }

    std::size_t requestLineEnd = data.find("\r\n");

    if (requestLineEnd == std::string::npos)
    {
        return ParseResult::BadRequest;
    }

    std::string requestLine = data.substr(0, requestLineEnd);

    std::istringstream stream(requestLine);

    std::string method;

    if (!(stream >> method >> m_request.url >> m_request.version))
    {
        return ParseResult::BadRequest;
    }

    if (method == "GET")
    {
        m_request.method = HttpMethod::GET;
    }
    else if (method == "POST")
    {
        m_request.method = HttpMethod::POST;
    }
    else
    {
        return ParseResult::BadRequest;
    }

    if (m_request.url.empty() || m_request.url[0] != '/')
    {
        return ParseResult::BadRequest;
    }

    if (m_request.version != "HTTP/1.1" && m_request.version != "HTTP/1.0")
    {
        return ParseResult::BadRequest;
    }

    /*
     * HTTP/1.1 默认 Keep-Alive。
     */
    m_request.keepAlive = (m_request.version == "HTTP/1.1");

    std::size_t current = requestLineEnd + 2;

    while (current < headerEnd)
    {
        std::size_t lineEnd = data.find("\r\n", current);

        if (lineEnd == std::string::npos || lineEnd > headerEnd)
        {
            return ParseResult::BadRequest;
        }

        std::string line = data.substr(current, lineEnd - current);

        std::size_t colon = line.find(':');

        if (colon == std::string::npos)
        {
            return ParseResult::BadRequest;
        }

        std::string name = toLower(trim(line.substr(0, colon)));

        std::string value = trim(line.substr(colon + 1));

        if (name == "host")
        {
            m_request.host = value;
        }
        else if (name == "content-length")
        {
            try
            {
                m_request.contentLength = static_cast<std::size_t>(std::stoul(value));
            }
            catch (...)
            {
                return ParseResult::BadRequest;
            }
        }
        else if (name == "connection")
        {
            std::string lowerValue = toLower(value);

            if (lowerValue == "close")
            {
                m_request.keepAlive = false;
            }
            else if (lowerValue == "keep-alive")
            {
                m_request.keepAlive = true;
            }
        }

        current = lineEnd + 2;
    }

    std::size_t bodyStart = headerEnd + 4;

    std::size_t bodyLength = length - bodyStart;

    if (bodyLength < m_request.contentLength)
    {
        return ParseResult::NoRequest;
    }

    return ParseResult::Complete;
}

std::string HttpParser::trim(const std::string& value)
{
    std::size_t first = value.find_first_not_of(" \t");

    if (first == std::string::npos)
    {
        return "";
    }

    std::size_t last = value.find_last_not_of(" \t");

    return value.substr(first, last - first + 1);
}

std::string HttpParser::toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return value;
}