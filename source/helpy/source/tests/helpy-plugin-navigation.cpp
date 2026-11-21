#include "Core/HelpyPluginNavigation.h"
#include <cassert>
int main(){
    namespace N=PS::HelpyPluginNavigation;
    int calls=0;
    const RuneSchemaNavTabV1 tab{sizeof(RuneSchemaNavTabV1),"GrandExchange.exchange","Grand Exchange","GE",
        [](void* context,const RuneSchemaNavUiV1*)->int32_t{++*static_cast<int*>(context);return RS_PLUGIN_OK;},&calls};
    assert(N::Register("GrandExchange",&tab)==RS_PLUGIN_OK);
    assert(N::Register("GrandExchange",&tab)==RS_PLUGIN_DUPLICATE);
    assert(N::Register("OtherMod",&tab)==RS_PLUGIN_INVALID_ARGUMENT);
    const auto rows=N::Read();assert(rows.size()==1&&rows[0].Route==tab.Route&&!rows[0].Draw);
    RuneSchemaNavUiV1 ui{};assert(N::Draw(tab.Route,ui)==RS_PLUGIN_OK&&calls==1);
    N::Remove("OtherMod");assert(N::Read().size()==1);
    N::Remove("GrandExchange");assert(N::Read().empty());
    assert(N::Draw(tab.Route,ui)==RS_PLUGIN_NOT_FOUND&&calls==1);
    auto invalid=tab;invalid.Route="GrandExchange.../escape";assert(N::Register("GrandExchange",&invalid)==RS_PLUGIN_INVALID_ARGUMENT);
    // Owned events may select only the caller's own registered route (checked in Release too).
    if(N::Register("GrandExchange",&tab)!=RS_PLUGIN_OK)return 10;
    const RuneSchemaNavTabV1 other{sizeof(RuneSchemaNavTabV1),"OtherMod.page","Other","OM",tab.Draw,&calls};
    if(N::Register("OtherMod",&other)!=RS_PLUGIN_OK)return 11;
    if(N::RequestOpen("OtherMod","GrandExchange.exchange")!=RS_PLUGIN_NOT_FOUND||!N::TakePendingRoute().empty())return 12;
    if(N::RequestOpen("GrandExchange","GrandExchange.missing")!=RS_PLUGIN_NOT_FOUND||!N::TakePendingRoute().empty())return 13;
    if(N::RequestOpen("GrandExchange","GrandExchange.exchange")!=RS_PLUGIN_OK)return 14;
    if(N::TakePendingRoute()!="GrandExchange.exchange"||!N::TakePendingRoute().empty())return 15;
    if(N::RequestOpen("GrandExchange","GrandExchange.exchange")!=RS_PLUGIN_OK)return 16;
    N::Remove("GrandExchange");if(!N::TakePendingRoute().empty())return 17;
    N::Remove("OtherMod");
    return 0;
}
