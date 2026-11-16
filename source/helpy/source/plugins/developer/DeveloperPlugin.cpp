#include "Core/HelpyBranding.h"
#include "DeveloperToolsKey.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <cstring>
namespace {
using Json=nlohmann::json;
std::filesystem::path PluginRoot;
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor),RUNESCHEMA_PLUGIN_API_VERSION,"RuneSchema.DeveloperTools","Advanced Tooling",PS::HelpyBranding::Version};
int32_t Reply(const Json& value,char* output,uint32_t capacity,uint32_t* size){const auto text=value.dump();*size=uint32_t(text.size()+1);if(!output||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;std::memcpy(output,text.c_str(),*size);return RS_PLUGIN_OK;}
int32_t RS_PLUGIN_CALL Exchange(void*,const char* input,char* output,uint32_t capacity,uint32_t* size){
 if(size)*size=0;
 if(!PS::DeveloperToolsKey::Present(PluginRoot))return RS_PLUGIN_NOT_FOUND;
 if(!input||!size||(!output&&capacity)||strnlen_s(input,4097)>4096)return RS_PLUGIN_INVALID_ARGUMENT;
 try{const auto request=Json::parse(input,[](int depth,Json::parse_event_t,Json&){if(depth>32)throw std::runtime_error("Request nesting limit");return true;});const auto op=request.at("Op").get<std::string>();
  if(op=="read")return Reply({{"Loaded",true},{"Name","Advanced Tooling"},{"Version",Descriptor.Version},{"IndependentImplementation",true},{"ReadOnlyDiscovery",true},{"Groups",Json::array({"Overview","Items","World","Spawns","Buildings","Journal","Compatibility"})}},output,capacity,size);
  if(op=="prepare"){const auto command=request.at("Command").get<std::string>();if(command=="inspect")return Reply({{"Request",{{"Action","InspectDeveloperContracts"}}}},output,capacity,size);if(command=="players")return Reply({{"Request",{{"Action","QuickPlayers"}}}},output,capacity,size);}
  return RS_PLUGIN_INVALID_ARGUMENT;
 }catch(...){return RS_PLUGIN_INVALID_ARGUMENT;}
}
}
extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query()noexcept{return &Descriptor;}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host,void** instance)noexcept{
 if(!host||!instance||host->StructSize<offsetof(RuneSchemaHostApi,FindObject)||host->ApiVersion!=RUNESCHEMA_PLUGIN_API_VERSION||!host->RegisterService||!host->RegisterCapability||!host->CallService)return RS_PLUGIN_INCOMPATIBLE_API;
 *instance=nullptr;try{
  HMODULE module=nullptr;wchar_t image[32768]{};
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&Descriptor),&module))return RS_PLUGIN_FAILED;
  const auto length=GetModuleFileNameW(module,image,32768);if(!length||length>=32768)return RS_PLUGIN_FAILED;
  PluginRoot=std::filesystem::path(image).parent_path().parent_path();
  if(!PS::DeveloperToolsKey::Present(PluginRoot))return RS_PLUGIN_NOT_FOUND;
  char identity[256]{};uint32_t size=0;if(host->CallService(Descriptor.Id,"helpy.host","{}",identity,sizeof(identity),&size)!=RS_PLUGIN_OK||Json::parse(identity).value("Process",std::string{})!="Helpy")return RS_PLUGIN_INCOMPATIBLE_API;
  if(host->RegisterCapability(Descriptor.Id,"helpy.developer-tools")!=RS_PLUGIN_OK||host->RegisterService(Descriptor.Id,"helpy.developer-tools",Exchange,nullptr)!=RS_PLUGIN_OK)return RS_PLUGIN_DUPLICATE;*instance=const_cast<RuneSchemaPluginDescriptor*>(&Descriptor);return RS_PLUGIN_OK;
 }catch(...){return RS_PLUGIN_FAILED;}
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*)noexcept{}
