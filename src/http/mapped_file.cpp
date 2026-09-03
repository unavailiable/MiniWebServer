#include "../../include/http/mapped_file.hpp"

#include <sys/mman.h>

MappedFile::MappedFile() noexcept : m_address(nullptr), m_size(0) {}

MappedFile::~MappedFile()
{
    reset();
}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : m_address(other.m_address), m_size(other.m_size)
{
    other.m_address = nullptr;
    other.m_size = 0;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept
{
    if (this != &other)
    {
        reset();

        m_address = other.m_address;

        m_size = other.m_size;

        other.m_address = nullptr;

        other.m_size = 0;
    }

    return *this;
}

bool MappedFile::map(int fd, std::size_t size)
{
    reset();

    /*
     * 空文件不需要 mmap。
     */
    if (size == 0)
    {
        return true;
    }

    void* address = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (address == MAP_FAILED)
    {
        return false;
    }

    m_address = static_cast<char*>(address);

    m_size = size;

    return true;
}

void MappedFile::reset() noexcept
{
    if (m_address)
    {
        munmap(m_address, m_size);
    }

    m_address = nullptr;

    m_size = 0;
}

const char* MappedFile::data() const noexcept
{
    return m_address;
}

std::size_t MappedFile::size() const noexcept
{
    return m_size;
}

bool MappedFile::valid() const noexcept
{
    return m_address != nullptr || m_size == 0;
}