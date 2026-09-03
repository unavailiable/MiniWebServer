#ifndef MAPPED_FILE_HPP
#define MAPPED_FILE_HPP

#include <cstddef>

class MappedFile
{
  public:
    MappedFile() noexcept;

    ~MappedFile();

    MappedFile(const MappedFile&) = delete;

    MappedFile& operator=(const MappedFile&) = delete;

    MappedFile(MappedFile&& other) noexcept;

    MappedFile& operator=(MappedFile&& other) noexcept;

  public:
    bool map(int fd, std::size_t size);

    void reset() noexcept;

    const char* data() const noexcept;

    std::size_t size() const noexcept;

    bool valid() const noexcept;

  private:
    char* m_address;

    std::size_t m_size;
};

#endif