#include "Core/ItemTransfer.h"
#include "Core/RecipeWorkspace.h"
#include "Generator/ItemCloneRequest.h"
#include "Check.h"
template<class F> bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
void RunChecks(){
    using namespace PS;using Json=nlohmann::json;
    Json row={{"Path","/Game/Items/A.A"},{"Name","Item A"},{"Icon","/Game/UI/A.A"},{"Count",4},{"Masterwork",true},{"PowerLevel",7},{"MaxStackSize",100},{"LargeNativeMetadata",std::string(20000,'x')}};
    const auto payload=ItemTransfer::Compact(row).dump();CHECK(payload.size()<8192);
    auto tagged=row;tagged["SourceKind"]="modded";tagged["SourceName"]="ColorsOfMoney";tagged["SourceRoot"]="/ColorsOfMoney";
    const auto taggedPayload=ItemTransfer::Compact(tagged).dump();const auto transferred=ItemTransfer::Decode(taggedPayload.c_str(),taggedPayload.size()+1);
    CHECK(transferred["SourceKind"]=="modded"&&transferred["SourceName"]=="ColorsOfMoney"&&transferred["SourceRoot"]=="/ColorsOfMoney");
    const auto item=ItemTransfer::Decode(payload.c_str(),payload.size()+1);CHECK(item["Count"]==4&&!item.contains("LargeNativeMetadata"));CHECK(item["Masterwork"]==true&&item["PowerLevel"]==7&&item["MaxStackSize"]==100);
    auto inputs=Json::array(),outputs=Json::array();RecipeWorkspace::AddItem(inputs,item);RecipeWorkspace::AddItem(outputs,item);
    CHECK(inputs[0]["Count"]==4&&outputs[0]["Count"]==4);RecipeWorkspace::AddItem(inputs,item);CHECK(inputs[0]["Count"]==8);
    inputs[0]["NativeExtra"]=7;RecipeWorkspace::ReplaceItem(inputs,0,"/Game/Items/B.B");CHECK(inputs[0]["Count"]==8&&inputs[0]["NativeExtra"]==7);
    const auto request=Authoring::CloneRequest(item["Path"],"Clone","",Json::object(),true,false,1,true,"Player","RuneSchema","","/Game/Items/B.B");
    CHECK(request["Source"]==item["Path"]&&request["AppearanceSource"]=="/Game/Items/B.B");
    const auto saved=Authoring::CloneRequest(item["Path"],"Clone","",Json::object(),true,false,1,true,"Player","MyClones","","/Game/Items/B.B","","","MyClones");CHECK(saved["OutputMod"]=="MyClones"&&!saved["Give"].get<bool>());
    for(const auto* name:{"","..","../Dye","C:\\Other","x/y","NUL","COM1","a.b","trailing ","a\nb"})CHECK(!Authoring::SafeModFolder(name));
    for(const auto* name:{"MyClones","my-clones_1","Dye"})CHECK(Authoring::SafeModFolder(name));
    const auto root=std::filesystem::temp_directory_path()/"RuneSchemaTransferTests";
    const auto destination=Authoring::AuthoredDirectory(root,"MyClones","assets");CHECK(destination.filename()=="assets"&&destination.parent_path().filename()=="MyClones");
    CHECK(Rejects([&]{Authoring::AuthoredDirectory(root,"../Other","assets");}));CHECK(Rejects([&]{Authoring::AuthoredDirectory(root,"Safe","../Other");}));
    CHECK(Rejects([&]{Authoring::CloneRequest(item["Path"],"Clone","",Json::object(),false,false,1,true,"Player","RuneSchema","","","","","MyClones");}));
    CHECK(Rejects([&]{ItemTransfer::Decode(nullptr,1);}));CHECK(Rejects([&]{ItemTransfer::Decode(payload.data(),payload.size());}));
    const char embedded[]="{}\0junk\0";CHECK(Rejects([&]{ItemTransfer::Decode(embedded,sizeof(embedded));}));
    for(const auto& count:{Json(0),Json(10001),Json(true),Json(1.5),Json("2")}){row["Count"]=count;CHECK(Rejects([&]{ItemTransfer::Compact(row);}));}
    row["Count"]=1;row["Path"]="invalid";CHECK(Rejects([&]{ItemTransfer::Compact(row);}));
    row["Path"]=std::string("/Game/A.A\0bad",13);CHECK(Rejects([&]{ItemTransfer::Compact(row);}));
}

int main(){try{RunChecks();return 0;}catch(const std::exception& e){std::fprintf(stderr,"Unexpected exception: %s\n",e.what());return 2;}}
