#include "Loader/QuestRegistryPlan.h"
#include "Loader/QuestNetworkId.h"
#include "Loader/QuestObjectReference.h"
#include "Loader/QuestIdentityPolicy.h"
#include <cassert>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
using namespace DragonWilds::QuestRegistry;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(int argc,char** argv){
    using DragonWilds::Quests::RestoreUnregisteredIdentity;
    using DragonWilds::Quests::RootedQuestLeaseMatches;
    assert(RootedQuestLeaseMatches(true,true,true,true,0,0));
    assert(RootedQuestLeaseMatches(true,true,true,true,0,731));
    assert(RootedQuestLeaseMatches(true,true,true,true,731,731));
    assert(!RootedQuestLeaseMatches(true,true,true,true,731,732));
    assert(!RootedQuestLeaseMatches(true,true,true,true,731,0));
    assert(!RootedQuestLeaseMatches(false,true,true,true,0,731));
    assert(!RootedQuestLeaseMatches(true,false,true,true,0,731));
    assert(!RootedQuestLeaseMatches(true,true,false,true,0,731));
    assert(!RootedQuestLeaseMatches(true,true,true,false,0,731));
    assert(!RootedQuestLeaseMatches(true,true,true,true,0,-1));
    assert(RestoreUnregisteredIdentity("","expected",false));
    assert(!RestoreUnregisteredIdentity("expected","expected",false));
    assert(!RestoreUnregisteredIdentity("expected","expected",true));
    assert(Rejects([]{RestoreUnregisteredIdentity("","expected",true);}));
    assert(Rejects([]{RestoreUnregisteredIdentity("different","expected",false);}));
    assert(Rejects([]{RestoreUnregisteredIdentity("different","expected",true);}));
    assert(Rejects([]{RestoreUnregisteredIdentity("","",false);}));
    for(const auto type:{"ObjectProperty","ObjectPtrProperty"})ValidateObjectReferenceLayout(type,sizeof(void*),1,sizeof(void*));
    for(const auto type:{"WeakObjectProperty","SoftObjectProperty","ClassProperty","Int64Property"})
        assert(Rejects([&]{ValidateObjectReferenceLayout(type,sizeof(void*),1,sizeof(void*));}));
    assert(Rejects([&]{ValidateObjectReferenceLayout("ObjectProperty",4,1,8);}));
    assert(Rejects([&]{ValidateObjectReferenceLayout("ObjectProperty",8,2,8);}));
    int object=3;int* destination=nullptr;int copies=0;
    const auto copy=[&](void* target,const void* source){++copies;std::memcpy(target,source,sizeof(void*));};
    const auto read=[](void* target){int* pointer;std::memcpy(&pointer,target,sizeof(pointer));return pointer;};
    CopyResolvedReference(&object,&destination,copy,read);assert(destination==&object && copies==1);
    assert(Rejects([&]{CopyResolvedReference(&object,&destination,copy,[](void*){return static_cast<int*>(nullptr);});}));
    assert(Rejects([&]{CopyResolvedReference(static_cast<int*>(nullptr),&destination,copy,read);}));
    const std::string wrapper="/Script/Dominion.DominionDataAssetNetId";
    const std::vector<NetworkIdField> valid{{"NetId","UInt16Property",2,0,1}};
    ValidateNetworkIdWrapper(wrapper,2,1,valid);
    assert(Rejects([&]{ValidateNetworkIdWrapper("/Script/Dominion.Other",2,1,valid);}));
    assert(Rejects([&]{ValidateNetworkIdWrapper(wrapper,4,1,valid);}));
    assert(Rejects([&]{ValidateNetworkIdWrapper(wrapper,2,2,valid);}));
    assert(Rejects([&]{ValidateNetworkIdWrapper(wrapper,2,1,{});}));
    auto bad=valid;bad.push_back(valid[0]);assert(Rejects([&]{ValidateNetworkIdWrapper(wrapper,2,1,bad);}));
    for(int i=0;i<5;++i){bad=valid;
        if(i==0)bad[0].Name="Other";if(i==1)bad[0].Type="Int16Property";
        if(i==2)bad[0].Size=4;if(i==3)bad[0].Offset=1;if(i==4)bad[0].Dim=2;
        assert(Rejects([&]{ValidateNetworkIdWrapper(wrapper,2,1,bad);}));
    }
    if(argc==2){
        std::ifstream input(argv[1]);assert(input.good());const auto capture=nlohmann::json::parse(input);
        assert(capture.at("type")==wrapper && capture.at("fields").size()==1);
        const auto& field=capture.at("fields")[0];
        assert(field.at("type")=="uint16");
        const std::vector<NetworkIdField> actual{{field.at("name"),"UInt16Property",field.at("elementSize"),field.at("offset"),field.at("arrayDim")}};
        ValidateNetworkIdWrapper(capture.at("type").get<std::string>(),2,1,actual);
    }
    for(const auto type:{"UInt16Property","Int16Property","UInt32Property","IntProperty"}) {
        const int size=std::string_view(type).find("16")!=std::string_view::npos?2:4;
        NetworkIdLayout layout(type,size,1);
        unsigned char bytes[8];std::memset(bytes,0xA5,sizeof(bytes));
        layout.Write(bytes,123);assert(layout.Read(bytes)==123);
        for(int i=size;i<8;++i)assert(bytes[i]==0xA5);
        const uint16_t max=std::string_view(type)=="Int16Property"?32767:65534;
        layout.Write(bytes,max);assert(layout.Read(bytes)==max);
        assert(Rejects([&]{layout.Write(bytes,65535);}));
        assert(Rejects([&]{NetworkIdLayout wrong(type,size==2?4:2,1);}));
        assert(Rejects([&]{NetworkIdLayout wrong(type,size,2);}));
    }
    NetworkIdLayout signed16("Int16Property",2,1),signed32("IntProperty",4,1),unsigned32("UInt32Property",4,1);
    int16_t negative16=-1;int32_t negative32=-1;uint32_t oversized=65536;
    assert(Rejects([&]{signed16.Read(&negative16);}));
    assert(Rejects([&]{signed32.Read(&negative32);}));
    assert(Rejects([&]{unsigned32.Read(&oversized);}));
    assert(Rejects([&]{signed16.Write(&negative16,32768);}));
    for(const auto type:{"ByteProperty","BoolProperty","FloatProperty","EnumProperty","StructProperty","UInt64Property",""})
        assert(Rejects([&]{NetworkIdLayout wrong(type,4,1);}));
    std::vector<uintptr_t> entries{10,20,30};
    auto plan=NetworkPlan(40,entries,{});assert(plan.Append && plan.Id==3);
    entries.push_back(40);plan=NetworkPlan(40,entries,3);assert(!plan.Append && plan.Id==3);
    assert(Rejects([&]{NetworkPlan(40,entries,2);}));
    assert(Rejects([&]{NetworkPlan(40,entries,{});}));
    assert(Rejects([&]{NetworkPlan(50,entries,3);}));
    assert(Rejects([&]{NetworkPlan(0,entries,{});}));
    entries.push_back(40);assert(Rejects([&]{NetworkPlan(40,entries,3);}));
    entries.assign(65535,10);assert(Rejects([&]{NetworkPlan(40,entries,{});}));
    entries.back()=40;plan=NetworkPlan(40,entries,65534);assert(!plan.Append && plan.Id==65534);
    entries.push_back(50);assert(Rejects([&]{NetworkPlan(40,entries,65534);}));
    entries.clear();plan=NetworkPlan(40,entries,{});assert(plan.Append && plan.Id==0);
}
