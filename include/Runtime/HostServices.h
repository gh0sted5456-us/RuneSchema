#pragma once
#include <filesystem>

namespace PS::HostServices {
    std::filesystem::path WorkingDirectory();
    bool GuiEnabled();
    void InitializeGui();
}
