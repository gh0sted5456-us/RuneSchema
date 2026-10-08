#include "Generator/StarterPreflight.h"
#include "Generator/LoaderSchemas.h"
#include <cassert>
#include <iostream>
using nlohmann::json;
template<class F>void Rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main(){
    const auto schemas=PS::JsonSchemaGenerator::LoaderSchemas();
    const json entry={{"Path","/Game/Test.Test"},{"Target","DT_Test"},{"Row","Row"},{"Name","Player"},{"Table","ST_Test"},{"Text","Original"}};
    for(const auto& c:PS::LoaderCapabilities){
        std::cerr << "Checking starter: " << c.Name << std::endl;
        assert(schemas.contains(c.Name));
        assert(schemas[c.Name]["x-runeschema-starter-capabilities"]["clone"]==c.Clone);
        assert(PS::LoaderTemplate::Patch(c.Name)==c.StarterPatch);
        assert(PS::AuthoredStarters::Supports(c.Name)==c.AuthoredSearch);
        json fields={{"Value",1}};
        if(std::string(c.Name)=="strings")fields={{"Replacement","Updated"}};
        if(std::string(c.Name)=="enums")fields={{"Values",json::array({"Value"})}};
        auto draft=PS::LoaderTemplate::Build(c.Name,entry,fields,0,"Mod","New","AAAAAAAAAAAAAAAAAAAAAA");
        auto roundtrip=PS::StarterPreflight::Parse(PS::LoaderTemplate::Jsonc(c.Name,0,draft));
        assert(roundtrip==draft);
        auto report=PS::StarterPreflight::Check(c.Name,roundtrip,draft,[](const auto&){return false;});
        assert(report["Errors"].empty() && report["ChangesFromCapturedStarter"].empty());
        if(c.StarterPatch){
            auto patch=PS::LoaderTemplate::Build(c.Name,entry,{{"Value",2}},1,"Mod","New","");
            auto check=PS::StarterPreflight::Check(c.Name,patch,patch,[](const auto&){return false;});
            assert(check["Errors"].empty());
        }
    }
    auto base=json{{"Item",{{"Icon","/Game/Icons.Coin"},{"Value",1}}}};
    auto draft=base;draft["Item"]["Value"]=2;draft["Item"]["Mesh"]="/Game/Mesh.Mesh";
    int calls=0;
    auto report=PS::StarterPreflight::Check("assets",draft,base,[&](const auto& path){++calls;return path=="/Game/Icons.Coin";});
    assert(calls==2 && report["Dependencies"].size()==2 && report["ChangesFromCapturedStarter"].size()==2);
    assert(report["Dependencies"][0]["Status"]=="loaded");
    assert(!PS::StarterPreflight::Check("strings",{{"text",false}},json::object(),[](const auto&){return false;})["Errors"].empty());
    assert(!PS::StarterPreflight::Check("blueprints",{{"BP",{{"$Clone","/Game/A.A"}}}},json::object(),[](const auto&){return false;})["Errors"].empty());
    json human={{"Id","test"},{"Type","Human"},{"Location",json::array({0,0,0})},{"Appearance",json::object()},
        {"Equipment",{{"MainHand","/Game/Test/Sword.Sword"},{"OffHand","/Game/Test/Shield.Shield"}}}};
    for(const auto* key:DragonWilds::HumanNpc::AppearanceKeys)human["Appearance"][key]="Row";
    assert(PS::StarterPreflight::Check("npc",json::array({human}),json::array({human}),[](const auto&){return true;})["Errors"].empty());
    human["Equipment"]["WrongHand"]="/Game/Test/Bad.Bad";
    assert(!PS::StarterPreflight::Check("npc",json::array({human}),json::array({human}),[](const auto&){return true;})["Errors"].empty());
    Rejects([]{PS::StarterPreflight::Parse(std::string(65536,' '));});
    Rejects([]{PS::StarterPreflight::Parse(std::string(30,'[')+"0"+std::string(30,']'));});
    assert(PS::StarterPreflight::Parse("// comment\n{\"a\":1}")["a"]==1);
}
