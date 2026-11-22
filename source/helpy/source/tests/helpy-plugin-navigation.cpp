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
    // A plugin built before the Home tile fields passes the original, shorter struct.
    auto legacy=tab;legacy.StructSize=static_cast<uint32_t>(N::BaseTabSize);legacy.IconImage="ignored.png";
    if(N::Register("GrandExchange",&legacy)!=RS_PLUGIN_OK||!N::Read()[0].IconPath.empty())return 20;
    N::Remove("GrandExchange");
    auto short_=tab;short_.StructSize=static_cast<uint32_t>(N::BaseTabSize-1);if(N::Register("GrandExchange",&short_)!=RS_PLUGIN_INVALID_ARGUMENT)return 21;
    // Home tile fields: a description, and one PNG directly in the plugin's own folder.
    auto tile=tab;tile.Description="Trade with other players.";tile.IconImage="ge.png";tile.IconFlags=RS_NAV_ICON_FULL_COLOR;
    if(N::Register("GrandExchange",&tile)!=RS_PLUGIN_INVALID_ARGUMENT)return 22; // an icon needs the plugin folder
    if(N::Register("GrandExchange",&tile,"plugins/GrandExchange")!=RS_PLUGIN_OK)return 23;
    const auto tiles=N::Read();
    if(tiles.size()!=1||tiles[0].Description!=tile.Description||tiles[0].IconPath!=std::filesystem::path("plugins/GrandExchange/ge.png")||tiles[0].IconFlags!=RS_NAV_ICON_FULL_COLOR)return 24;
    N::Remove("GrandExchange");
    for(const char* bad:{"../ge.png","sub/ge.png","sub\\ge.png","C:ge.png",".png","ge.jpg","ge","ge.png#x"}){
        auto wrong=tile;wrong.IconImage=bad;if(N::Register("GrandExchange",&wrong,"plugins/GrandExchange")!=RS_PLUGIN_INVALID_ARGUMENT)return 25;
    }
    auto upper=tile;upper.IconImage="GE.PNG";if(N::Register("GrandExchange",&upper,"plugins/GrandExchange")!=RS_PLUGIN_OK)return 26;
    N::Remove("GrandExchange");
    auto flags=tile;flags.IconFlags=2;if(N::Register("GrandExchange",&flags,"plugins/GrandExchange")!=RS_PLUGIN_INVALID_ARGUMENT)return 27;
    auto words=tile;words.Description="line\nbreak";if(N::Register("GrandExchange",&words,"plugins/GrandExchange")!=RS_PLUGIN_INVALID_ARGUMENT)return 28;
    auto plain=tile;plain.IconImage=nullptr;plain.Description=nullptr;if(N::Register("GrandExchange",&plain)!=RS_PLUGIN_OK||!N::Read()[0].IconPath.empty())return 29;
    N::Remove("GrandExchange");
    // The legal notice shown from the top bar's "i": optional, line breaks allowed, other controls and overlong refused.
    auto notice=tile;notice.Legal="Trading terms.\nNot endorsed by Jagex.";
    if(N::Register("GrandExchange",&notice,"plugins/GrandExchange")!=RS_PLUGIN_OK||N::Read()[0].Legal!=notice.Legal)return 30;
    N::Remove("GrandExchange");
    for(const std::string bad:{std::string("tab\there"),std::string("bell\a"),std::string(N::MaxLegalBytes+1,'x')}){
        auto wrong=tile;wrong.Legal=bad.c_str();if(N::Register("GrandExchange",&wrong,"plugins/GrandExchange")!=RS_PLUGIN_INVALID_ARGUMENT)return 31;
    }
    {const std::string full(N::MaxLegalBytes,'x');auto most=tile;most.Legal=full.c_str();if(N::Register("GrandExchange",&most,"plugins/GrandExchange")!=RS_PLUGIN_OK)return 32;N::Remove("GrandExchange");}
    // A plugin built before the notice field (Home tile size) still registers, without a notice.
    auto older=tile;older.StructSize=static_cast<uint32_t>(N::TileTabSize);older.Legal="ignored";
    if(N::Register("GrandExchange",&older,"plugins/GrandExchange")!=RS_PLUGIN_OK||!N::Read()[0].Legal.empty())return 33;
    N::Remove("GrandExchange");
    return 0;
}
