#include "Core/HelpyZip.h"
#include "miniz.h"
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace PS::HelpyZip {
bool SafeEntry(std::string_view name) {
    if (name.empty() || name.size() > 240 || name.front() == '/') return false;
    size_t start = 0;
    while (start < name.size()) {
        const auto end = name.find('/', start);
        const auto segment = name.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        // Windows trims trailing spaces/dots; a disguised parent component must not escape.
        if (segment.empty()||segment=="."||segment==".."||segment.back()==' '||segment.back()=='.') return false;
        for (const unsigned char c : segment) if (c < 32 || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false;
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return true;
}

void Extract(const std::filesystem::path& zip, const std::filesystem::path& folder, Limits limits) {
    if (std::filesystem::exists(folder)) throw std::runtime_error("The extraction folder already exists.");
    // Read through a path-aware stream: miniz's own file opening can't take every Windows path.
    std::vector<char> bytes;
    {
        std::ifstream in(zip, std::ios::binary | std::ios::ate);
        if (!in) throw std::runtime_error("The download could not be opened.");
        const auto size = static_cast<uint64_t>(in.tellg());
        if (size < 22 || size > limits.MaxBytes) throw std::runtime_error("The download has an unexpected size.");
        bytes.resize(static_cast<size_t>(size)); in.seekg(0);
        if (!in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error("The download could not be read.");
    }
    mz_zip_archive archive{};
    if (!mz_zip_reader_init_mem(&archive, bytes.data(), bytes.size(), 0)) throw std::runtime_error("The download is not a readable ZIP.");
    const std::unique_ptr<mz_zip_archive, void (*)(mz_zip_archive*)> close(&archive, [](mz_zip_archive* a) { mz_zip_reader_end(a); });
    const auto count = mz_zip_reader_get_num_files(&archive);
    if (count == 0 || count > limits.MaxFiles) throw std::runtime_error("The ZIP has an unexpected number of files.");
    uint64_t total = 0;
    // Check every entry before writing anything.
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, i, &stat)) throw std::runtime_error("The ZIP has an unreadable entry.");
        if (!SafeEntry(stat.m_filename)) throw std::runtime_error(std::string("The ZIP has an unsafe entry: ") + stat.m_filename);
        if (stat.m_is_encrypted || !stat.m_is_supported) throw std::runtime_error("The ZIP has an encrypted or unsupported entry.");
        if (stat.m_uncomp_size>limits.MaxBytes-total) throw std::runtime_error("The ZIP is larger than allowed.");
        total += stat.m_uncomp_size;
    }
    std::error_code ignored;
    try {
        std::filesystem::create_directories(folder);
        for (mz_uint i = 0; i < count; ++i) {
            mz_zip_archive_file_stat stat{};
            mz_zip_reader_file_stat(&archive, i, &stat);
            const auto target = folder / std::filesystem::u8path(stat.m_filename);
            if (mz_zip_reader_is_file_a_directory(&archive, i)) { std::filesystem::create_directories(target); continue; }
            std::filesystem::create_directories(target.parent_path());
            size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&archive, i, &size, 0);
            if (!data) throw std::runtime_error(std::string("Could not extract ") + stat.m_filename);
            const std::unique_ptr<void, void (*)(void*)> owned(data, [](void* p) { mz_free(p); });
            if (size != stat.m_uncomp_size) throw std::runtime_error("A ZIP entry has the wrong size.");
            std::ofstream out(target, std::ios::binary | std::ios::trunc);
            out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
            out.close();
            if (!out) throw std::runtime_error(std::string("Could not write ") + stat.m_filename);
        }
    } catch (...) {
        std::filesystem::remove_all(folder, ignored);
        throw;
    }
}
}
