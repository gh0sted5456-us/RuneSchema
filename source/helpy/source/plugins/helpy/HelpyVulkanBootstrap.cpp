#include <windows.h>
#include <vulkan/vk_layer.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

// The implicit selector loads the full bridge only in an explicitly enrolled game executable.
VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* version){
    try{
        if(!version||version->loaderLayerInterfaceVersion<2)return VK_ERROR_INITIALIZATION_FAILED;
        wchar_t executable[32768]{},modulePath[32768]{};HMODULE module{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&vkNegotiateLoaderLayerInterfaceVersion),&module))return VK_ERROR_INITIALIZATION_FAILED;
        const auto gameLength=GetModuleFileNameW(nullptr,executable,32768),moduleLength=GetModuleFileNameW(module,modulePath,32768);if(!gameLength||gameLength>=32768||!moduleLength||moduleLength>=32768)return VK_ERROR_INITIALIZATION_FAILED;
        const auto file=std::filesystem::path(modulePath).parent_path()/"targets.json";std::error_code error;const auto size=std::filesystem::file_size(file,error);if(error||size>65536)return VK_ERROR_INITIALIZATION_FAILED;
        std::ifstream input(file,std::ios::binary);const auto bindings=nlohmann::json::parse(input);if(!bindings.is_array()||bindings.size()>64)return VK_ERROR_INITIALIZATION_FAILED;
        for(const auto& binding:bindings){const auto game=std::filesystem::u8path(binding.at("Executable").get<std::string>());if(_wcsicmp(game.c_str(),executable)!=0)continue;
            const auto bridge=std::filesystem::u8path(binding.at("Bridge").get<std::string>());if(!bridge.is_absolute()||bridge.filename()!=L"RuneSchema.Helpy.dll")return VK_ERROR_INITIALIZATION_FAILED;
            const auto library=LoadLibraryExW(bridge.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!library)return VK_ERROR_INITIALIZATION_FAILED;
            const auto negotiate=reinterpret_cast<PFN_vkNegotiateLoaderLayerInterfaceVersion>(GetProcAddress(library,"vkNegotiateLoaderLayerInterfaceVersion"));if(!negotiate){FreeLibrary(library);return VK_ERROR_INITIALIZATION_FAILED;}const auto result=negotiate(version);if(result!=VK_SUCCESS)FreeLibrary(library);return result;
        }
    }catch(...){}
    return VK_ERROR_INITIALIZATION_FAILED;
}
#pragma comment(linker, "/EXPORT:vkNegotiateLoaderLayerInterfaceVersion")
