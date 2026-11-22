#pragma once
#include <cstdint>
// Core-owned service runeschema.item.grant. See Core/ItemGrant.h for the contract.
namespace PS::ItemGrantService {
// Game thread only. permitted is decided by the loaded caller's declared item.grant capability.
int32_t Call(const char* caller,bool permitted,const char* request,char* response,uint32_t capacity,uint32_t* size);
}
