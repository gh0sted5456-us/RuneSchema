// Production startup orchestration with a host model, not in-game validation.
#include "Core/ManagedModStartup.h"
#include <cassert>
#include <memory>
#include <stdexcept>
#define STR(x) L##x
using StringType=std::wstring;
enum class LogLevel{Warning};
struct Output {template<LogLevel Level=LogLevel::Warning,class... T>static void send(const wchar_t*,T&&...) {}};
inline std::string ensure_str(const char* text){return text;}
namespace RuneSchemaManagedPackages {inline PS::ManagedModStartup::Queue Startup;}
struct Host;
struct Mod {
    std::wstring Name;bool Installed=false,Started=false;virtual ~Mod()=default;
    bool is_installable(){return Name!=L"Broken";}bool is_started(){return Started;}
    void set_installed(bool value){Installed=value;}
    virtual void start_mod(){Started=true;}
};
struct CppMod:Mod {
    CppMod(Host&,StringType&& name,StringType&&){Name=std::move(name);}
    void start_mod()override {
        assert(Installed);Started=true;
        if(Name==L"First")assert(RuneSchemaManagedPackages::Startup.Add({L"Owner",L"Second",L"X:/Owner/ue4ss/Second",false,true})==PS::ManagedModStartup::Result::Queued);
    }
};
struct LuaMod:Mod {LuaMod(Host&,StringType&& name,StringType&&){Name=std::move(name);}void start_mod()override{assert(Installed);Started=true;}};
struct Host {
    enum class IsInstalled{Any,Yes};std::vector<std::unique_ptr<Mod>> m_mods;
    template<class T>T* find_mod_by_name(const std::wstring& name,IsInstalled installed=IsInstalled::Any){for(const auto& mod:m_mods)if(mod->Name==name&&(installed!=IsInstalled::Yes||mod->Installed))if(auto typed=dynamic_cast<T*>(mod.get()))return typed;return nullptr;}
    void register_packages(){
#include "../../ue4ss/managed-host/RegisterNativePackages.inl"
    }
    void start_packages(){
#include "../../ue4ss/managed-host/StartLuaPackages.inl"
    }
};
int main(){
    using namespace PS::ManagedModStartup;auto& queue=RuneSchemaManagedPackages::Startup;queue.Reset();Host host;
    for(auto request:std::vector<Request>{{L"Owner",L"First",L"X:/First",false,true},{L"Owner",L"Hybrid",L"X:/Hybrid",true,true},{L"Owner",L"LuaOnly",L"X:/Lua",true,false},{L"Owner",L"Broken",L"X:/Broken",true,true}})assert(queue.Add(request)==Result::Queued);
    host.register_packages();assert(queue.Snapshot().size()==4); // Broken rejected; First queued Second.
    assert(host.find_mod_by_name<CppMod>(L"Second")->is_started());
    assert(host.find_mod_by_name<CppMod>(L"Hybrid")->is_started());
    assert(!host.find_mod_by_name<LuaMod>(L"Hybrid")->Installed);
    assert(!host.find_mod_by_name<LuaMod>(L"Broken"));
    assert(queue.Add({L"Owner",L"Late",L"X:/Late",true,false})==Result::TooLate);
    host.start_packages();assert(!host.find_mod_by_name<LuaMod>(L"Hybrid")->is_started());
    for(const auto& mod:host.m_mods)mod->Installed=true; // Normal install_lua_mods phase.
    host.start_packages();assert(host.find_mod_by_name<LuaMod>(L"Hybrid")->is_started());assert(host.find_mod_by_name<LuaMod>(L"LuaOnly")->is_started());
    host.start_packages();assert(host.m_mods.size()==5);
}
