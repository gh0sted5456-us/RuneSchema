#include <lua.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <chrono>
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static void Run(lua_State* lua,const std::string& code){if(luaL_dostring(lua,code.c_str())!=LUA_OK)throw std::runtime_error(lua_tostring(lua,-1));}
static void Write(const std::filesystem::path& path,const std::string& text){std::filesystem::create_directories(path.parent_path());std::ofstream out(path);out<<text;Check(out.good(),"Fixture write failed");}
int main(int argc,char** argv){
    try {
        Check(argc>=3,"Expected helper source and private fixture directory");namespace fs=std::filesystem;
        const auto root=fs::absolute(argv[2])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto session="# Session\tprivate-fixture\n";
        const auto manifest=std::string(session)+"A\tAlpha\tA/ue4ss/Alpha\tScripts/main.lua\tlua\nB\tBeta\tB/ue4ss/Beta\tScripts/main.lua\tlua\nN\tNative\tN/ue4ss/Native\tScripts/main.lua\tnative\nH\tHostOwned\tH/ue4ss/HostOwned\tScripts/main.lua\thost\n";
        Write(root/"settings/ue4ss.generated.txt",manifest);
        for(const auto& [owner,name,value]:{std::tuple{"A","Alpha",17},std::tuple{"B","Beta",42}}){
            const auto package=root/"mods"/owner/"ue4ss"/name;
            Write(package/"config.txt",std::to_string(value));
            Write(package/"modmenu.json","{\"name\":\"kept\"}");
            Write(package/"Scripts/helper.lua","return "+std::to_string(value));
            Write(package/"Scripts/main.lua",R"(assert(Leaked==nil);Leaked=true
local c=RuneSchemaUE4SS.Current
local f=assert(io.open(c.Root..'/config.txt','r'));local value=tonumber(f:read('*a'));f:close()
assert(value==require('helper'));record(c.Name,value)
callbacks[#callbacks+1]=function() assert(RuneSchemaUE4SS.Current.Root==c.Root);record(c.Name,require('helper')) end
)");
        }
        Write(root/"mods/N/ue4ss/Native/Scripts/main.lua","error('native fallback must never execute')");
        Write(root/"mods/H/ue4ss/HostOwned/Scripts/main.lua","error('host-owned package must never execute twice')");
        _putenv_s("RUNESCHEMA_UE4SS_PACKAGE_SESSION","private-fixture");
        auto* lua=luaL_newstate();Check(lua,"Lua state allocation failed");luaL_openlibs(lua);
        lua_pushstring(lua,root.generic_string().c_str());lua_setglobal(lua,"fixture_root");
        Run(lua,"records={};callbacks={};function record(name,value) records[#records+1]={name,value} end");
        Check(luaL_loadfile(lua,argv[1])==LUA_OK,"Package helper syntax failed");Check(lua_pcall(lua,0,1,0)==LUA_OK,"Package helper initialization failed");lua_setglobal(lua,"load_packages");
        Run(lua,"assert(load_packages(fixture_root));assert(#records==2);assert(records[1][1]=='Alpha' and records[1][2]==17);assert(records[2][1]=='Beta' and records[2][2]==42);assert(Leaked==nil);for _,callback in ipairs(callbacks) do callback() end;assert(#records==4);load_packages(fixture_root);assert(#records==4)");
        Write(root/"settings/ue4ss.generated.txt","# Session\tprevious-launch\n"+manifest.substr(std::string(session).size()));
        Run(lua,"RuneSchemaUE4SS=nil;records={};load_packages(fixture_root);assert(#records==0)");
        if(argc==4){Check(luaL_loadfile(lua,argv[3])==LUA_OK,"Actual MaxCape main.lua failed syntax-only validation");lua_pop(lua,1);}
        lua_close(lua);std::cout<<"UE4SS_PACKAGE_LUA_PASSED nested_layout config_preserved helper_isolation callback_context no_double_start stale_manifest native_hold retired_host_rejected MaxCape_syntax_only\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
