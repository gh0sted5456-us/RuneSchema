#include "Loader/VisualPolicy.h"
#include "Utility/SoftPathParts.h"
#include <iostream>
#include <stdexcept>
#include <string>
using nlohmann::json;
void require(bool value) { if(!value)throw std::runtime_error("Authoring regression failed"); }
int main() {
    using PS::SplitSoftPath;
    auto p=SplitSoftPath(L"/Game/Spell.Spell:DamageModule.Shape");
    require(p.valid && p.package==L"/Game/Spell" && p.asset==L"Spell" && p.subobject==L"DamageModule.Shape");
    p=SplitSoftPath(L"/Game/Items/Rune.Rune");
    require(p.valid && p.asset==L"Rune" && p.subobject.empty());
    p=SplitSoftPath(L"/Game/Items/Rune");require(p.valid && p.asset.empty());
    for(auto bad:{L"",L"None",L"Spell",L"/Game/Spell.",L"/Game/Spell.Spell:",L"/Game/Spell..Spell",L"/Game/Spell:Module",L"/Game/Spell.Spell:.Module"})
        require(!SplitSoftPath(bad).valid);
    std::wstring bounded=L"/Game/Spell.Spell:ModuleTRAILING";
    p=SplitSoftPath(std::wstring_view(bounded).substr(0,bounded.size()-8));
    require(p.valid && p.subobject==L"Module");
    for(auto good:{json::object(),json{{"Overlay",true},{"BodyMaterial",false}},json{{"Overlay",false},{"BodyMaterial",true}},json{{"Overlay",true},{"BodyMaterial",true}}})
        DragonWilds::ValidateVisualLayers(good);
    for(auto bad:{json{{"Overlay",false},{"BodyMaterial",false}},json{{"Overlay",1}},json{{"BodyMaterial","true"}},json{{"Overlay",nullptr}}}) {
        bool rejected=false;try {DragonWilds::ValidateVisualLayers(bad);}catch(...){rejected=true;}
        require(rejected);
    }
    std::cout<<"PASS: soft subobject paths, bounded views, legacy defaults and independent appearance layers\n";
}
