#pragma once
#include "Core/HelpyGameProcess.h"
#include "Core/ModMenuFiles.h"
#include <memory>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace PS {
class OfflineModMenu {
    using Json=nlohmann::json;
    ModMenuFiles::Scope Files;
    std::unique_ptr<lua_State,decltype(&lua_close)> Lua{nullptr,lua_close};
    Json Snapshot;
    int Command=LUA_NOREF;

    static int Exchange(lua_State* lua){
        auto& menu=*static_cast<OfflineModMenu*>(lua_touserdata(lua,lua_upvalueindex(1)));
        std::string reply;
        try{
            size_t length=0;
            const char* input=lua_tolstring(lua,1,&length);
            if(!input||length>4*1024*1024)throw std::runtime_error("Invalid offline Mod Menu request.");
            const auto request=Json::parse(input,input+length);
            const auto op=request.at("Op").get<std::string>();
            Json result;
            if(op=="catalog")result=menu.Files.Catalog();
            else if(op=="read")result=menu.Files.Read(request);
            else if(op=="write"){
                if(!HelpyGameProcess::GameStopped())throw std::runtime_error("Dragonwilds started. Use the live Mod Menu after it connects.");
                result=menu.Files.Write(request);
            }else if(op=="publish"){
                auto snapshot=request.at("Snapshot");
                if(!snapshot.at("Pages").is_array()||snapshot.at("Pages").size()>2048)
                    throw std::runtime_error("Invalid offline Mod Menu snapshot.");
                if(snapshot.dump(-1,' ',false,Json::error_handler_t::replace).size()>RUNESCHEMA_PLUGIN_MAX_MESSAGE-4096)
                    throw std::runtime_error("Offline Mod Menu contains too many settings.");
                menu.Snapshot=std::move(snapshot);
                result={{"Accepted",true}};
            }else throw std::runtime_error("Live Mod Menu actions require Dragonwilds.");
            reply=result.dump(-1,' ',false,Json::error_handler_t::replace);
        }catch(const std::exception& error){
            reply=Json{{"Error",error.what()}}.dump(-1,' ',false,Json::error_handler_t::replace);
        }
        lua_pushlstring(lua,reply.data(),reply.size());
        return 1;
    }
    void Check(int code){
        if(code==LUA_OK)return;
        const auto* error=lua_tostring(Lua.get(),-1);
        std::string message=error?error:"Offline Mod Menu adapter failed.";
        lua_pop(Lua.get(),1);
        throw std::runtime_error(message);
    }
public:
    explicit OfflineModMenu(const std::filesystem::path& root){
        Files.Open(root);
        static int moduleAnchor;
        HMODULE module=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&moduleAnchor),&module))
            throw std::runtime_error("Offline Mod Menu module was not found.");
        const auto resource=FindResourceW(module,MAKEINTRESOURCEW(101),MAKEINTRESOURCEW(10));
        const auto data=resource?LoadResource(module,resource):nullptr;
        const auto* script=data?static_cast<const char*>(LockResource(data)):nullptr;
        if(!script)throw std::runtime_error("Offline Mod Menu adapter was not packaged.");
        Lua.reset(luaL_newstate());
        if(!Lua)throw std::runtime_error("Could not start the offline Mod Menu interpreter.");
        auto* lua=Lua.get();
        for(const auto& [name,open]:std::array<std::pair<const char*,lua_CFunction>,5>{{
                {LUA_GNAME,luaopen_base},{LUA_MATHLIBNAME,luaopen_math},{LUA_STRLIBNAME,luaopen_string},
                {LUA_TABLIBNAME,luaopen_table},{LUA_UTF8LIBNAME,luaopen_utf8}}}){
            luaL_requiref(lua,name,open,1);lua_pop(lua,1);
        }
        // Only the bundled adapter runs. Mod Lua configuration is parsed as text.
        for(const auto* name:{"load","loadfile","dofile"}){lua_pushnil(lua);lua_setglobal(lua,name);}
        lua_pushboolean(lua,1);lua_setglobal(lua,"RuneSchemaModMenuOffline");
        lua_pushlightuserdata(lua,this);lua_pushcclosure(lua,Exchange,1);lua_setglobal(lua,"RuneSchemaModMenuExchange");
        Check(luaL_loadbufferx(lua,script,SizeofResource(module,resource),"@helpy-mod-menu.lua","t"));
        Check(lua_pcall(lua,0,1,0));
        if(!lua_isfunction(lua,-1)||!Snapshot.is_object())throw std::runtime_error("Offline Mod Menu did not publish a catalog.");
        Command=luaL_ref(lua,LUA_REGISTRYINDEX);
    }
    Json TakeSnapshot(){return std::move(Snapshot);}
    Json Run(const Json& command){
        const auto input=command.dump();
        lua_rawgeti(Lua.get(),LUA_REGISTRYINDEX,Command);
        lua_pushlstring(Lua.get(),input.data(),input.size());
        Check(lua_pcall(Lua.get(),1,0,0));
        return TakeSnapshot();
    }
};
}
