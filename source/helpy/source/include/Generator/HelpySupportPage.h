#pragma once
#include <windows.h>
#if defined(HELPY_PREVIEW)
#include <filesystem>
#endif
struct ID3D11Device;

namespace PS::HelpySupportPage {
class Frame {
public:
    Frame(HWND window,HINSTANCE module,ID3D11Device* device,bool browserAllowed);
    ~Frame();
    Frame(const Frame&)=delete;
    Frame& operator=(const Frame&)=delete;
};
void Render();
void Close();
void ReleaseImages();
#if defined(HELPY_PREVIEW)
bool BrowserReady();
bool ProbeBrowser(const std::filesystem::path& result);
#endif
}
