#include "Runtime/HostServices.h"
#include "UE4SSProgram.hpp"

namespace PS::HostServices {
    std::filesystem::path WorkingDirectory() {
        return RC::UE4SSProgram::get_program().get_working_directory();
    }
    bool GuiEnabled() {
        return RC::UE4SSProgram::settings_manager.Debug.DebugConsoleEnabled;
    }
    void InitializeGui() {
        using RC::UE4SSProgram;
        UE4SS_ENABLE_IMGUI()
    }
}
