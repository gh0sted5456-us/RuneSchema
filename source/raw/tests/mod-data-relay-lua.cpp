extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include <iostream>
int main(int argc,char** argv){if(argc!=3)return 2;auto* state=luaL_newstate();if(!state)return 2;luaL_openlibs(state);lua_newtable(state);lua_pushstring(state,argv[2]);lua_rawseti(state,-2,1);lua_setglobal(state,"arg");const int result=luaL_dofile(state,argv[1]);if(result)std::cerr<<lua_tostring(state,-1)<<'\n';lua_close(state);return result?1:0;}
