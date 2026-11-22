#pragma once
#include <cstdint>
// Core-owned service runeschema.item.snapshot (read-only). See Core/ItemSnapshot.h for the record
// and docs/development/API-REFERENCE.md "Item snapshot" for the contract.
namespace PS::ItemSnapshotService {
int32_t Call(const char* request,char* response,uint32_t capacity,uint32_t* size);
}
