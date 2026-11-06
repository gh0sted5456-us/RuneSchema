#include "HudInterfaces.h"
#include <cassert>
int main(){
    namespace H=PS::HudInterfaces;using J=H::Json;
    J doc={{"SchemaVersion",1},{"Interfaces",J::array({{{"Id","editor"},{"Name","Example editor"},{"Kind","panel"},{"Keyboard","Ctrl+F7"},{"Controls",J::array({
        {{"Id","enabled"},{"Type","toggle"},{"Label","Enabled"},{"Value",true}},
        {{"Id","size"},{"Type","slider"},{"Label","Size"},{"Min",0},{"Max",2},{"Step",0.1},{"Value",1}},
        {{"Id","preset"},{"Type","choice"},{"Label","Preset"},{"Options",{"One","Two"}}},
        {{"Id","note"},{"Type","text"},{"Label","Note"}},
        {{"Id","apply"},{"Type","button"},{"Label","Apply"}}
    })}},{{"Id","external"},{"Name","Existing editor"},{"Kind","client"},{"Client","Example.Editor"},{"Controller","View+West"}}})}};
    H::Store state;state.Add(H::Parse("ExampleMod",doc));assert(state.Rows.size()==2&&state.Revision==1);
    const auto rejects=[&](auto call){bool failed=false;try{call();}catch(const std::exception&){failed=true;}assert(failed);};
    const auto request=J{{"Id","ExampleMod/editor"},{"Control","enabled"},{"Expected",true},{"Value",false}};
    rejects([&]{state.Queue(request);});state.Register("ExampleMod/editor",{"enabled","size","preset","note","apply"});
    state.Queue(request);rejects([&]{state.Queue(request);});assert(state.Next().at("Value")==false&&state.Next().is_null());
    state.Set("ExampleMod/editor","enabled",false);rejects([&]{state.Queue(request);});
    state.Set("ExampleMod/editor","size",1.14);assert(state.Control(state.Find("ExampleMod/editor"),"size").at("Value")==1.1);
    rejects([&]{state.Set("ExampleMod/editor","size",3);});rejects([&]{state.Set("ExampleMod/editor","size","1");});
    const auto bytes=state.Bytes;state.Bytes=2*1024*1024;rejects([&]{state.Set("ExampleMod/editor","note","X");});state.Bytes=bytes;
    rejects([&]{state.Set("ExampleMod/editor","preset","Three");});rejects([&]{state.Set("ExampleMod/editor","note",std::string(2049,'x'));});
    state.Queue({{"Id","ExampleMod/editor"},{"Control","enabled"},{"Expected",false},{"Value",true}});
    state.Unregister("ExampleMod/editor");assert(state.Commands.empty()&&!state.Find("ExampleMod/editor").at("Ready").get<bool>());
    rejects([&]{state.Add(H::Parse("ExampleMod",doc));});
    for(const auto key:{"Id","Kind","Controls"}){auto bad=doc;bad["Interfaces"][0].erase(key);rejects([&]{H::Parse("ExampleMod",bad);});}
    auto bad=doc;bad["Interfaces"][0]["Keyboard"]="Alt+F4";rejects([&]{H::Parse("ExampleMod",bad);});
    bad=doc;bad["Interfaces"][0]["Controls"][1]["Max"]=0;rejects([&]{H::Parse("ExampleMod",bad);});
    bad=doc;bad["Interfaces"][0]["Controls"][1]["Max"]=std::numeric_limits<double>::max();rejects([&]{H::Parse("ExampleMod",bad);});
    bad=doc;bad["Interfaces"][0]["Controls"][1]["Step"]=std::numeric_limits<double>::denorm_min();rejects([&]{H::Parse("ExampleMod",bad);});
    bad=doc;bad["Interfaces"][0]["Controls"][2]["Options"]={"One","One"};rejects([&]{H::Parse("ExampleMod",bad);});
}
