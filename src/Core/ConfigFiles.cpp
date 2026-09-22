#include "Core/ConfigFiles.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace PS::ConfigFiles {
    namespace {
        std::string Suffix() {
            static std::atomic<unsigned> sequence{0};
            return std::to_string(std::chrono::system_clock::now().time_since_epoch().count())
                + "-" + std::to_string(sequence++);
        }
    }
    std::string Read(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open configuration for reading");
        std::string contents{std::istreambuf_iterator<char>(file), {}};
        if (file.bad()) throw std::runtime_error("Cannot finish reading configuration");
        return contents;
    }
    std::string Read(const std::filesystem::path& path, size_t maximumBytes) {
        if(maximumBytes>8*1024*1024)throw std::runtime_error("Unsupported bounded-read limit");
        std::ifstream file(path,std::ios::binary);
        if(!file)throw std::runtime_error("Cannot open diagnostic input");
        std::string contents(maximumBytes+1,'\0');
        file.read(contents.data(),static_cast<std::streamsize>(contents.size()));
        if(file.bad())throw std::runtime_error("Cannot finish reading diagnostic input");
        if(static_cast<size_t>(file.gcount())>maximumBytes)throw std::runtime_error("Diagnostic input exceeds byte limit");
        contents.resize(static_cast<size_t>(file.gcount()));
        return contents;
    }
    std::filesystem::path Backup(const std::filesystem::path& path) {
        auto backup = path;
        backup += ".invalid-" + Suffix() + ".bak";
        // No overwrite: a failed backup must prevent recovery from replacing the original.
        if (!std::filesystem::copy_file(path, backup, std::filesystem::copy_options::none))
            throw std::runtime_error("Cannot preserve original configuration");
        return backup;
    }
    void Write(const std::filesystem::path& path, std::string_view contents) {
        std::filesystem::create_directories(path.parent_path());
        auto staging = path;
        staging += ".write-" + Suffix();
        if (!std::filesystem::create_directory(staging))
            throw std::runtime_error("Cannot reserve configuration staging directory");
        const auto temporary = staging / "config.tmp";
        const auto cleanup = [&] {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            std::filesystem::remove(staging, ignored);
        };
        try {
            std::ofstream file(temporary, std::ios::binary);
            file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            file.flush();
            if (!file) throw std::runtime_error("Cannot finish writing configuration");
            file.close();
            if (!file) throw std::runtime_error("Cannot close configuration file");
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot replace configuration");
#else
            std::filesystem::rename(temporary, path);
#endif
        } catch (...) { cleanup(); throw; }
        cleanup();
    }
}
