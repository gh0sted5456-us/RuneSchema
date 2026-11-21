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
}
