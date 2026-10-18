#include <lua.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>
static void Run(lua_State* lua,const char* text){if(luaL_dostring(lua,text)!=LUA_OK)throw std::runtime_error(lua_tostring(lua,-1));}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Expected helper and fixture directory");
    namespace fs=std::filesystem;const auto root=fs::absolute(argv[2])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directories(root);
    const auto request=root/"0123456789abcdef.request";{std::ofstream out(request,std::ios::binary);out<<"0123456789abcdef\n";}
    auto* lua=luaL_newstate();luaL_openlibs(lua);lua_pushstring(lua,request.generic_string().c_str());lua_setglobal(lua,"request_file");
    Run(lua,"os.getenv=function(name) if name=='RUNESCHEMA_AUTO_MAPPING_REQUEST' then return request_file end end; calls=0; DumpUSMAP=function(blueprints) assert(blueprints==true);calls=calls+1 end; ExecuteInGameThread=function(cb) cb() end; ExecuteWithDelay=function(ms,cb) assert(ms==15000);pending=cb end");
    if(luaL_loadfile(lua,argv[1])!=LUA_OK||lua_pcall(lua,0,1,0)!=LUA_OK)throw std::runtime_error(lua_tostring(lua,-1));lua_setglobal(lua,"loader");
    Run(lua,"loader();assert(calls==0);pending();assert(calls==1);local f=assert(io.open(request_file..'.result','rb'));assert(f:read('*a')=='0123456789abcdef\\nok\\n');f:close()");
    Run(lua,"DumpUSMAP=nil;loader();local f=assert(io.open(request_file..'.result','rb'));assert(f:read('*a')=='0123456789abcdef\\nunavailable\\n');f:close();assert(calls==1)");
    Run(lua,"DumpUSMAP=function() error('fixture failed dump') end;loader();pending();local f=assert(io.open(request_file..'.result','rb'));assert(f:read('*a')=='0123456789abcdef\\nerror\\n');f:close()");
    {std::ofstream out(request,std::ios::binary);out<<"bad request";}Run(lua,"pending=nil;loader();assert(pending==nil)");
    lua_close(lua);std::cout<<"PASS: native USMAP handoff, game-thread scheduling, capability fallback, failed dump and malformed request\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
