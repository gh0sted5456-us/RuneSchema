#pragma once
#include "Core/ItemSnapshot.h"
#include <cstdint>
namespace RC::Unreal { class UObject; }
namespace PS::ItemSnapshotService {
struct LiveItem { RC::Unreal::UObject* Controller{}; RC::Unreal::UObject* Container{}; RC::Unreal::UObject* Item{}; RC::Unreal::UObject* Data{}; nlohmann::json Error; };
LiveItem Resolve(const ItemSnapshot::Request& request);
nlohmann::json Read(const ItemSnapshot::Request& request);
int32_t Call(const char* request,char* response,uint32_t capacity,uint32_t* size);
}
