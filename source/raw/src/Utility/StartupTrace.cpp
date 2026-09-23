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
}
