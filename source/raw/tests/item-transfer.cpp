#include "Core/ItemTransfer.h"
#include "Core/RecipeWorkspace.h"
#include "Generator/ItemCloneRequest.h"
#include <cassert>
template<class F> bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    using namespace PS;using Json=nlohmann::json;
    Json row={{"Path","/Game/Items/A.A"},{"Name","Item A"},{"Icon","/Game/UI/A.A"},{"Count",4},{"LargeNativeMetadata",std::string(20000,'x')}};
    const auto payload=ItemTransfer::Compact(row).dump();assert(payload.size()<8192);
    const auto item=ItemTransfer::Decode(payload.c_str(),payload.size()+1);assert(item["Count"]==4&&!item.contains("LargeNativeMetadata"));
    auto inputs=Json::array(),outputs=Json::array();RecipeWorkspace::AddItem(inputs,item);RecipeWorkspace::AddItem(outputs,item);
    assert(inputs[0]["Count"]==4&&outputs[0]["Count"]==4);RecipeWorkspace::AddItem(inputs,item);assert(inputs[0]["Count"]==8);
    inputs[0]["NativeExtra"]=7;RecipeWorkspace::ReplaceItem(inputs,0,"/Game/Items/B.B");assert(inputs[0]["Count"]==8&&inputs[0]["NativeExtra"]==7);
    const auto request=Authoring::CloneRequest(item["Path"],"Clone","",Json::object(),true,false,1,true,"Player","RuneSchema","","/Game/Items/B.B");
    assert(request["Source"]==item["Path"]&&request["AppearanceSource"]=="/Game/Items/B.B");
    const auto saved=Authoring::CloneRequest(item["Path"],"Clone","",Json::object(),true,false,1,true,"Player","MyClones","","/Game/Items/B.B","","","MyClones");assert(saved["OutputMod"]=="MyClones"&&!saved["Give"].get<bool>());
    for(const auto* name:{"","..","../Dye","C:\\Other","x/y","NUL","COM1","a.b","trailing ","a\nb"})assert(!Authoring::SafeModFolder(name));
    for(const auto* name:{"MyClones","my-clones_1","Dye"})assert(Authoring::SafeModFolder(name));
    const auto root=std::filesystem::temp_directory_path()/"RuneSchemaTransferTests";
    const auto destination=Authoring::AuthoredDirectory(root,"MyClones","assets");assert(destination.filename()=="assets"&&destination.parent_path().filename()=="MyClones");
    assert(Rejects([&]{Authoring::AuthoredDirectory(root,"../Other","assets");}));assert(Rejects([&]{Authoring::AuthoredDirectory(root,"Safe","../Other");}));
    assert(Rejects([&]{Authoring::CloneRequest(item["Path"],"Clone","",Json::object(),false,false,1,true,"Player","RuneSchema","","","","","MyClones");}));
    assert(Rejects([&]{ItemTransfer::Decode(nullptr,1);}));assert(Rejects([&]{ItemTransfer::Decode(payload.data(),payload.size());}));
    const char embedded[]="{}\0junk\0";assert(Rejects([&]{ItemTransfer::Decode(embedded,sizeof(embedded));}));
    for(const auto& count:{Json(0),Json(10001),Json(true),Json(1.5),Json("2")}){row["Count"]=count;assert(Rejects([&]{ItemTransfer::Compact(row);}));}
    row["Count"]=1;row["Path"]="invalid";assert(Rejects([&]{ItemTransfer::Compact(row);}));
    row["Path"]=std::string("/Game/A.A\0bad",13);assert(Rejects([&]{ItemTransfer::Compact(row);}));
}
