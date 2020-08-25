#include "core/exception.h"
#include "core/file.h"

using namespace std::literals;

std::string_view FileReader::readline() {
    const char* end = reinterpret_cast<const char*>(m_region.get_address()) + m_region.get_size();

    if (m_pos == end) return ""sv;

    const char* b = m_pos;
    const char* e = end;

    while (m_pos < e) {
        if (*m_pos == '\n') return std::string_view(b, ++m_pos - b);
        m_pos += 1;
    }
    return std::string_view(b, m_pos - b);
}
