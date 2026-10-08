#include "Generator/LoaderTemplate.h"
#include "Core/JsonPatchDirective.h"
#include <cassert>
using nlohmann::json;
template<class F>void Rejects(F operation){bool caught=false;try{operation();}catch(const std::exception&){caught=true;}assert(caught);}
int main() {
    using namespace PS::LoaderTemplate;
    const json entry={{"Path","/Game/Test.Test"},{"Target","DT_Test"},{"Row","Goblin"},{"Name","Tester"},{"Table","ST_Test"},{"Text","Goblin pack"}};
    for(const auto* loader:{"assets","players","blueprints","raw","strings","buildings","recipes","journal","courses","spawns","enums","equipment"}) {
        const auto ref=Build(loader,entry,{{"Weight",{{"type","number"}}}},3,"","","");
        assert(ref["properties"]["Weight"]["type"]=="number");
        const auto parsed=json::parse(Jsonc(loader,3,ref),nullptr,true,true);assert(parsed==ref);
    }
    const auto raw=Build("raw",entry,{{"Amount",2}},1,"","","");
    assert(DragonWilds::JsonPatchDirective::Parse(raw,{},"raw")->Reference=="DT_Test:Goblin");
    assert(Build("raw",entry,{{"Amount",2}},0,"","","")["DT_Test"]["Goblin"]["Amount"]==2);
    assert(Build("blueprints",entry,{{"Value",1}},1,"","","")["Patch"]["$Patch"]=="DT_Test");
    const auto building=Build("buildings",entry,{{"PersistenceID","old"},{"DisplayName","Test"}},2,"Mod","Mint","");
    assert(building["Mint"]["$Clone"]==entry["Path"] && !building["Mint"]["Properties"].contains("PersistenceID"));
    assert(building["Mint"]["Unlock"]==false);
    assert(Build("players",entry,{{"Nameplate",{{"Mode","Name"}}}},0,"Mod","TestRule","")[0]["PlayerName"]=="Tester");
    assert(Build("strings",entry,{{"Replacement","New text"}},0,"","","")["ST_Test"]["Goblin pack"]=="New text");
    for(const auto* loader:{"players","blueprints","raw","strings"})Rejects([&]{Build(loader,entry,json::object(),2,"Mod","Name","");});
    for(const auto* loader:{"players","strings","buildings"})Rejects([&]{Build(loader,entry,json::object(),1,"Mod","Name","");});
    Rejects([&]{Build("unknown",entry,json::object(),3,"","","");});
    const json vendor={{"Id","Cow"},{"Location",{0,0,0}},{"Mesh","/Game/Cow.Cow"},{"RowName","Fellhollow"}};
    for(const auto& doc:{vendor,json::array({vendor}),json{{"Vendors",json::array({vendor})}},json{{"Cow",vendor}}}) {
        const auto rows=PS::AuthoredStarters::Entries(doc,"vendors");
        assert(rows.size()==1 && rows[0]["Target"]=="Cow");
        assert(Build("vendors",rows[0],rows[0]["Body"],0,"","","")==json::array({vendor}));
    }
    const json authored={{"Id","Existing"},{"Amount",2}};
    for(const auto* loader:{"recipes","journal","courses","spawns","enums","equipment"}) {
        assert(!Clone(loader) && !Append(loader));
        Rejects([&]{Build(loader,entry,authored,2,"Mod","New","");});
        const auto fields=std::string(loader)=="enums"?json{{"Values",json::array({"Existing"})}}:authored;
        const auto draft=Build(loader,entry,fields,0,"Mod","New","");
        assert(json::parse(Jsonc(loader,0,draft),nullptr,true,true)==draft);
        if(std::string(loader)=="spawns")assert(draft[0]["Id"]=="Existing");
        else if(std::string(loader)=="courses")assert(draft["Id"]=="Existing");
        else if(std::string(loader)!="enums")assert(draft["DT_Test"]["Id"]=="Existing");
    }
    for(const auto* loader:{"assets","raw","blueprints"}) {
        const json fields={{"$Append",{{"Requirements",json::array({{{"Count",1}}})}}}};
        auto draft=Build(loader,entry,json::object(),1,"Mod","New","");
        auto& target=std::string(loader)=="raw"?draft["$Target"]:draft["Patch"]["$Target"];
        target=fields;
        const auto text=Jsonc(loader,1,draft);
        assert(text.find("does not deduplicate")!=std::string::npos);
        assert(json::parse(text,nullptr,true,true)==draft);
        const auto& patch=std::string(loader)=="raw"?draft:draft.at("Patch");
        assert(patch["$Target"]["$Append"]==fields["$Append"]);
    }
    const auto rows=PS::AuthoredStarters::Entries(json::array({authored,json{{"$Patch","Existing"},{"Id","Patch"}},json{{"Amount",2}}}),"spawns");
    assert(rows.size()==1 && rows[0]["Target"]=="Existing");
    json many=json::array();
    for(int i=0;i<150;++i)many.push_back({{"Id","Entry"+std::to_string(i)}});
    const auto late=PS::AuthoredStarters::Entries(many,"spawns","Entry149");
    assert(late.size()==1 && late[0]["Target"]=="Entry149");
    assert(PS::AuthoredStarters::Entries(many,"spawns","Entry").size()==100);
    assert(PS::AuthoredStarters::Entries(many,"spawns","absent").empty());
}
