#pragma once
#include <string_view>
#include <optional>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "core/each.h"

// TODO this can just be a string iterator (detached from file)
class FileReader : public each<FileReader> {
   public:
    FileReader(std::string_view filename)
        : m_file(std::string(filename).data(), boost::interprocess::read_only)
        , m_region(m_file, boost::interprocess::read_only)
        , m_pos(reinterpret_cast<const char*>(m_region.get_address())) {}

    std::string_view readline();

    // removes \n from the end
    std::optional<std::string_view> next() {
        auto line = readline();
        if (line.empty()) return std::nullopt;
        return line.substr(0, line.size() - 1);
    }

   private:
    boost::interprocess::file_mapping m_file;
    boost::interprocess::mapped_region m_region;
    const char* m_pos;
};
