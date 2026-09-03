#ifndef UNIQUE_FD_HPP
#define UNIQUE_FD_HPP

class UniqueFd
{
  public:
    UniqueFd() noexcept;

    explicit UniqueFd(int fd) noexcept;

    ~UniqueFd();

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept;
    UniqueFd& operator=(UniqueFd&& other) noexcept;

  public:
    int get() const noexcept;

    bool valid() const noexcept;

    explicit operator bool() const noexcept;

    int release() noexcept;

    void reset(int fd = -1) noexcept;

  private:
    int m_fd;
};

#endif