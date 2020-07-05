#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

#include "core/exception.h"
#include "core/file.h"

using namespace std::literals;

MappedFile::MappedFile(std::string_view filename) {
    std::string fname;
    fname = filename;
    m_fd = open(fname.c_str(), O_RDONLY);
    if (m_fd == -1) THROW(runtime_error, "open");

    struct stat sb;
    if (fstat(m_fd, &sb) == -1) THROW(runtime_error, "fstat");

    m_length = sb.st_size;
    m_addr = static_cast<const char*>(mmap(NULL, m_length, PROT_READ, MAP_PRIVATE, m_fd, 0u));
    if (m_addr == MAP_FAILED) THROW(runtime_error, "mmap");
}

MappedFile::~MappedFile() {
    munmap(const_cast<char*>(m_addr), m_length);
    close(m_fd);
}

std::string_view FileReader::readline() {
    if (m_pos == m_file.view().end()) return ""sv;

    const char* b = m_pos;
    const char* e = m_file.view().end();

    while (m_pos < e) {
        if (*m_pos == '\n') return std::string_view(b, ++m_pos - b);
        m_pos += 1;
    }
    return std::string_view(b, m_pos - b);
}
