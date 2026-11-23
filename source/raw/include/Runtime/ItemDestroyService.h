#pragma once
#include <cstdint>
namespace PS::ItemDestroyService {
// Core-owned, game-thread only. permitted is set by the loaded caller's capability contract.
int32_t Call(const char* caller,bool permitted,const char* request,char* response,uint32_t capacity,uint32_t* size);
}
