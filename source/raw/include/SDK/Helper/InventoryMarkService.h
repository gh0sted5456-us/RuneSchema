#pragma once
#include "SDK/Helper/EquipmentDelivery.h"
#include "Runtime/PluginHost.h"
namespace PS::InventoryMarkService {
using Json=nlohmann::json;
inline Json Call(const Json& request){
    auto reply=Json::parse(PluginHost::Call("RuneSchema.Core","helpy.inventory",request.dump()));
    if(reply.contains("Error"))throw std::runtime_error(reply.at("Error").get<std::string>());
    return reply;
}
}
