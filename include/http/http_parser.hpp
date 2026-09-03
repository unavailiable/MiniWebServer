#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <cstddef>
#include <string>

enum class HttpMethod
{
    GET,
    POST,
    UNKNOWN
};

struct HttpRequest
{
    HttpMethod method;

    std::string url;

    std::string version;

    std::string host;

    std::size_t contentLength;

    bool keepAlive;

    HttpRequest();
};

enum class ParseResult
{
    NoRequest,
    BadRequest,
    Complete
};

class HttpParser
{
  public:
    HttpParser();

  public:
    ParseResult parse(const char* buffer, std::size_t length);

    void reset();

    const HttpRequest& request() const noexcept;

  private:
    static std::string trim(const std::string& value);

    static std::string toLower(std::string value);

  private:
    HttpRequest m_request;
};

#endif