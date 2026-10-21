#include "Core/HelpyStorage.h"
#include <cassert>
#include <iostream>
#include <future>
#include <thread>
using namespace PS;using Json=nlohmann::json;
template<class F>void Reject(F f){bool caught=false;try{f();}catch(...){caught=true;}assert(caught);}
int main(int argc,char**argv){assert(argc==2);const auto root=std::filesystem::path(argv[1])/std::to_string(GetCurrentProcessId());
    const std::string guid="11111111222222223333333344444444";HelpyStorage::Store bin(root,guid);
    const Json item={{"AssetPath","/Game/Mods/Test/Item.Item"},{"PersistenceID","Mod-Item-01"},{"InstanceGuid","aaaaaaaa111111112222222233333333"},{"Count",3},
        {"NativeItem",{{"ItemData","Mod-Item-01"},{"GUID","aaaaaaaa111111112222222233333333"},{"Count",3},{"Durability",42},{"UnknownModField",{{"Keep",true}}}}}};
    std::promise<void> locked,release;auto released=release.get_future();
    std::thread busy([&]{std::lock_guard lock(HelpyStorage::Gate);locked.set_value();released.wait();});locked.get_future().wait();
    const auto start=std::chrono::steady_clock::now();Reject([&]{bin.PrepareDelete("busy",item);});
    const auto elapsed=std::chrono::steady_clock::now()-start;release.set_value();busy.join();
    assert(elapsed<std::chrono::seconds(1)&&bin.Rows()["Items"].empty());
    assert(bin.Rows()["Items"].empty());bin.PrepareDelete("capture1",item);assert(!bin.Rows()["Items"][0]["Restorable"].get<bool>());
    Reject([&]{bin.BeginRestore("capture1","restore1",bin.Read()["Revision"],false);});
    bin.ObserveDelete("capture1",HelpyStorage::DeleteResult::Removed);auto stored=bin.Read();assert(bin.Rows()["Items"][0]["Restorable"]==true&&stored["Items"]["capture1"]["Item"]==item);
    Reject([&]{bin.PrepareDelete("capture1",item);});
    assert(HelpyStorage::ReadFile(bin.File(),guid)==bin.Rows());
    Reject([&]{HelpyStorage::ReadFile(bin.File(),"aaaaaaaa222222223333333344444444");});
    Reject([&]{bin.BeginRestore("capture1","restore1",stored["Revision"],true);});
    bin.BeginRestore("capture1","restore1",stored["Revision"],false);Reject([&]{bin.BeginRestore("capture1","restore2",bin.Read()["Revision"],false);});
    bin.FinishRestore("capture1","restore1",HelpyStorage::RestoreResult::Unknown);assert(!bin.Rows()["Items"][0]["Restorable"].get<bool>());
    Reject([&]{bin.PrepareDelete("duplicate-instance",item);});
    Reject([&]{bin.FinishRestore("capture1","restore1",HelpyStorage::RestoreResult::Confirmed);});
    auto second=item;second["InstanceGuid"]="bbbbbbbb111111112222222233333333";second["NativeItem"]["GUID"]=second["InstanceGuid"];
    bin.PrepareDelete("capture2",second);bin.ObserveDelete("capture2",HelpyStorage::DeleteResult::Retained);assert(bin.Rows()["Items"].size()==1);
    bin.PrepareDelete("capture3",second);bin.ObserveDelete("capture3",HelpyStorage::DeleteResult::Removed);bin.BeginRestore("capture3","restore3",bin.Read()["Revision"],false);bin.FinishRestore("capture3","restore3",HelpyStorage::RestoreResult::Confirmed);assert(bin.Rows()["Items"].size()==1);
    auto third=item;third["InstanceGuid"]="cccccccc111111112222222233333333";third["NativeItem"]["GUID"]=third["InstanceGuid"];
    bin.PrepareDelete("prune-terminal",third);assert(!bin.Read()["Items"].contains("capture3")&&bin.Read()["Items"].contains("capture1"));bin.ObserveDelete("prune-terminal",HelpyStorage::DeleteResult::Retained);
    Reject([&]{bin.FinishRestore("capture3","restore3",HelpyStorage::RestoreResult::Confirmed);});
    auto bad=item;bad["NativeItem"]["Customization"]=Json::object();Reject([&]{bin.PrepareDelete("bad",bad);});Reject([&]{HelpyStorage::Store foreign(root,"../save");});
    HelpyStorage::Store other(root,"aaaaaaaa222222223333333344444444");assert(other.Rows()["Items"].empty()&&other.File()!=bin.File());
    const auto prior=ConfigFiles::Read(bin.File());ConfigFiles::Write(bin.File(),"invalid");Reject([&]{bin.PrepareDelete("capture4",item);});assert(ConfigFiles::Read(bin.File())=="invalid");
    ConfigFiles::Write(bin.File(),prior);assert(bin.Read()["Items"]["capture1"]["Item"]["NativeItem"]["UnknownModField"]["Keep"]==true);
    auto scrap=item;scrap["InstanceGuid"]="00000001000000020000000300000004";scrap["NativeItem"]["GUID"]=scrap["InstanceGuid"];
    Json plan={{"Path",scrap["AssetPath"]},{"PlayerGuid",guid},{"Instance",{1,2,3,4}},{"Quantity",3},{"Delivery","WorldDrops"},
        {"Refunds",Json::array({{{"Path","/Game/Items/Wood.Wood"},{"Count",6}}})}};
    auto invalidPlan=plan;invalidPlan["Instance"]={5,6,7,8};Reject([&]{bin.PrepareScrap("wrong-instance",scrap,invalidPlan);});
    invalidPlan=plan;invalidPlan["Refunds"][0]["Count"]=-1;Reject([&]{bin.PrepareScrap("negative",scrap,invalidPlan);});
    bin.PrepareScrap("scrap1",scrap,plan);assert(bin.Rows()["Items"].size()==1);
    Reject([&]{bin.PrepareScrap("scrap1",scrap,plan);});Reject([&]{bin.PrepareDelete("no-bin-copy",scrap);});
    Reject([&]{bin.FinishScrap("scrap1",true,Json::array());});
    bin.ObserveScrap("scrap1",HelpyStorage::DeleteResult::Removed);
    Reject([&]{bin.BeginRestore("scrap1","duplicate",bin.Read()["Revision"],false);});
    Reject([&]{bin.FinishScrap("scrap1",true,Json::array());});
    const Json drops=Json::array({{{"Path","/Game/Items/Wood.Wood"},{"Count",6},{"Actor","/Game/Maps/World/L_World.L_World:PersistentLevel.Drop_1"}}});
    bin.FinishScrap("scrap1",true,drops);assert(bin.Read()["Items"]["scrap1"]["State"]=="scrapped"&&bin.Rows()["Items"].size()==1);
    Reject([&]{bin.FinishScrap("scrap1",true,drops);});
    plan["Instance"]={9,2,3,4};scrap["InstanceGuid"]="00000009000000020000000300000004";scrap["NativeItem"]["GUID"]=scrap["InstanceGuid"];
    bin.PrepareScrap("scrap2",scrap,plan);bin.ObserveScrap("scrap2",HelpyStorage::DeleteResult::Removed);bin.FinishScrap("scrap2",false,Json::array());
    assert(bin.Rows()["Items"].size()==1);Reject([&]{bin.PrepareScrap("uncertain-retry",scrap,plan);});
    assert(bin.Read()["Items"]["scrap2"]["State"]=="scrap-unconfirmed");
    std::cout<<"Helpy Storage: item-only character isolation, confirmed delete/restore states, duplicate receipts and damaged-file preservation passed.\n";
}
