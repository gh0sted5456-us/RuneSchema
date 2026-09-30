#include "Utility/StartupTrace.h"
#include "Utility/Config.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <mutex>

namespace PS::StartupTrace {
namespace {
std::mutex Mutex;
std::ofstream Stream;
std::filesystem::path Folder;
std::chrono::steady_clock::time_point Start;
}
void Begin(const std::filesystem::path& folder) noexcept {
    try {
        std::lock_guard lock(Mutex);
        Folder = folder;
        Start = std::chrono::steady_clock::now();
        std::error_code error;
        std::filesystem::remove(Folder / "startup-failure.log", error);
    } catch (...) {}
}
void Mark(std::string_view stage) noexcept {
    if (!PSConfig::Get()->IsDebugLoggingEnabled()) return;
    try {
        std::lock_guard lock(Mutex);
        if (!Stream.is_open()) {
            std::filesystem::create_directories(Folder);
            const auto current = Folder / "startup-current.log";
            if (std::filesystem::exists(current))
                std::filesystem::copy_file(current, Folder / "startup-previous.log", std::filesystem::copy_options::overwrite_existing);
            Stream.open(current, std::ios::trunc);
            Stream << "RuneSchema startup; PID=" << GetCurrentProcessId() << '\n';
        }
        if (!Stream) return;
        Stream << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Start).count()
               << "ms thread=" << GetCurrentThreadId() << ' ' << stage << '\n';
        Stream.flush();
    } catch (...) {}
}
void Fatal(std::string_view stage, std::string_view reason) noexcept {
    try {
        std::lock_guard lock(Mutex);
        auto folder = Folder;
        if (folder.empty()) {
            std::error_code error;
            folder = std::filesystem::temp_directory_path(error)
                / "RuneSchema" / "startup";
            if (error) return;
        }
        std::filesystem::create_directories(folder);
        std::ofstream failure(folder / "startup-failure.log", std::ios::trunc);
        if (!failure) return;
        failure << "RuneSchema DID-NOT-START\nPID=" << GetCurrentProcessId()
                << "\nStage=" << stage << "\nReason=" << reason << '\n';
        failure.flush();
        const auto debug = std::string("[RuneSchema][DID-NOT-START][")
            + std::string(stage) + "] " + std::string(reason) + "\n";
        OutputDebugStringA(debug.c_str());
    } catch (...) {}
}
}
