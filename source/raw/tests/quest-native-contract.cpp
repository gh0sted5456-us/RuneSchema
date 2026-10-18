#include "Loader/QuestNativeContract.h"
#include "Loader/QuestObjectiveSchema.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cassert>
#include <vector>
using namespace DragonWilds::QuestNative;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(int argc,char** argv){
    using DragonWilds::Quests::ObjectivePayload;
    const auto objectives=nlohmann::json::array({{{"Key","first"},{"Value","Find the bard."}},
        {{"Key","__ready"},{"Value","Return for your reward."}}});
    assert(ObjectivePayload(objectives,false)==objectives);
    const auto structured=ObjectivePayload(objectives,true);
    assert(structured.size()==2&&structured[0]["Key"]=="first"
        &&structured[0]["Value"]==nlohmann::json({{"Description","Find the bard."}})
        &&structured[1]["Value"]["Description"]=="Return for your reward.");
    assert(objectives[0]["Value"].is_string()); // No mutation of the caller's legacy payload.
    assert(ObjectivePayload(nlohmann::json::array(),true).empty());
    assert(Rejects([&]{ObjectivePayload(nlohmann::json::object(),true);}));
    assert(Rejects([&]{ObjectivePayload(nlohmann::json::array({{{"Key",7},{"Value","x"}}}),true);}));
    assert(Rejects([&]{ObjectivePayload(nlohmann::json::array({{{"Key","x"},{"Value",7}}}),true);}));
    assert(Rejects([&]{ObjectivePayload(nlohmann::json::array({{{"Key","x"}}}),false);}));
    assert(Rejects([&]{ObjectivePayload(nlohmann::json::array({"x"}),true);}));
    const auto delivery=[](std::string_view name,int size,std::initializer_list<Field> fields) {
        ValidateDelivery(name,size,std::span(fields.begin(),fields.size()));
    };
    delivery("Client_NotifyTrackedQuestLoaded",8,{{"QuestData",0,8,Kind::Quest}});
    delivery("Client_NotifyTrackedQuestLoaded",9,{{"QuestData",0,8,Kind::Quest},{"QuestType",8,1,Kind::QuestType}});
    assert(Rejects([&]{delivery("Client_NotifyTrackedQuestLoaded",8,{{"QuestData",0,8,Kind::Quest},{"QuestType",8,1,Kind::QuestType}});}));
    assert(Rejects([&]{delivery("Client_NotifyTrackedQuestLoaded",9,{{"QuestData",0,8,Kind::Quest},{"QuestType",8,1,Kind::State}});}));
    assert(Rejects([&]{delivery("Client_NotifyTrackedQuestLoaded",9,{{"QuestData",0,8,Kind::Quest},{"WrongType",8,1,Kind::QuestType}});}));
    assert(Rejects([&]{delivery("Client_NotifyTrackedQuestLoaded",9,{{"QuestData",0,8,Kind::Quest},{"QuestType",7,1,Kind::QuestType}});}));
    assert(Rejects([&]{delivery("Client_NotifyTrackedQuestLoaded",8,{{"QuestData",0,8,Kind::Quest,true}});}));
    assert(Rejects([&]{delivery("Client_NotifyTrackedQuestLoaded",8,{{"QuestData",0,8,Kind::Quest},{"QuestData",0,8,Kind::Quest}});}));
    delivery("Client_OnQuestsUpdated",17,{{"UpdatedQuests",0,16,Kind::ProgressArray},{"bFromLoadedState",16,1,Kind::Boolean}});
    assert(Rejects([&]{delivery("Client_OnQuestsUpdated",17,{{"UpdatedQuests",0,16,Kind::Progress},{"bFromLoadedState",16,1,Kind::Boolean}});}));
    for(const int progressSize:{56,64}) {
        std::vector<Field> fields={{"OldQuest",0,progressSize,Kind::Progress},{"UpdatedQuest",progressSize,progressSize,Kind::Progress},
            {"bSendNotification",2*progressSize,1,Kind::Boolean},{"bNewlyGiven",2*progressSize+1,1,Kind::Boolean}};
        ValidateDelivery("Client_OnQuestUpdated",2*progressSize+2,fields);
        fields[1].Size++;assert(Rejects([&]{ValidateDelivery("Client_OnQuestUpdated",2*progressSize+2,fields);}));
    }
    assert(Rejects([&]{delivery("Unknown",8,{{"QuestData",0,8,Kind::Quest}});}));
    for(const auto method:{Method::Give,Method::Complete,Method::Initialize,Method::IsInitialized,Method::State,Method::Objective,Method::GetInt,Method::SetInt}) {
        const auto spec=Get(method);
        std::vector<Field> fields(spec.Fields.begin(),spec.Fields.end());
        Validate(spec,spec.Size,fields);
        Validate(spec,spec.Size+16,fields); // Harmless trailing alignment is not a semantic change.
        auto relocated=fields;
        for(auto& field:relocated)field.Offset+=16;
        Validate(spec,spec.Size+32,relocated);
        for(size_t i=0;i<fields.size();++i) {
            const auto original=fields[i];
            fields[i].Size++;assert(Rejects([&]{Validate(spec,spec.Size,fields);}));fields[i]=original;
            fields[i].Offset++;assert(Rejects([&]{Validate(spec,spec.Size,fields);}));fields[i]=original;
            fields[i].Return=!original.Return;assert(Rejects([&]{Validate(spec,spec.Size,fields);}));fields[i]=original;
            fields[i].Type=original.Type==Kind::Name?Kind::Boolean:Kind::Name;
            assert(Rejects([&]{Validate(spec,spec.Size,fields);}));fields[i]=original;
            fields[i].Name="Wrong";assert(Rejects([&]{Validate(spec,spec.Size,fields);}));fields[i]=original;
        }
        fields.push_back(fields[0]);assert(Rejects([&]{Validate(spec,spec.Size,fields);}));
    }
    delivery("Client_NotifyTrackedQuestLoaded",32,{{"QuestType",24,1,Kind::QuestType},{"QuestData",8,8,Kind::Quest}});
    delivery("Client_OnQuestsUpdated",48,{{"bFromLoadedState",40,1,Kind::Boolean},{"UpdatedQuests",8,16,Kind::ProgressArray}});
    if(argc==2){
        std::ifstream file(argv[1]);nlohmann::json capture;file>>capture;
        assert(capture.at("errors").empty());int checked=0;
        for(const auto& object:capture.at("objects"))if(object.value("type","")=="/Script/Dominion.QuestProgressComponent")
            for(const auto method:{Method::Give,Method::Complete,Method::Initialize,Method::IsInitialized,Method::State,Method::Objective,Method::GetInt,Method::SetInt}) {
                const auto spec=Get(method);bool found=false;
                for(const auto& fn:object.at("functions"))if(fn.at("type")=="/Script/Dominion.QuestProgressComponent:"+std::string(spec.Name)) {
                    std::vector<Field> fields;std::vector<std::string> names;names.reserve(8);
                    for(const auto& field:fn.at("fields")){
                        const auto type=field.at("type").get<std::string>();
                        const auto kind=type=="UQuestData*"?Kind::Quest:type=="bool"?Kind::Boolean:type=="FName"?Kind::Name:type=="int32"?Kind::Integer:Kind::State;
                        assert(type=="UQuestData*" || type=="bool" || type=="FName" || type=="EQuestState" || type=="int32");
                        assert(field.at("arrayDim")==1);
                        const auto flags=field.at("flags").get<uint64_t>();assert((flags&128)!=0);
                        names.push_back(field.at("name"));
                        fields.push_back({names.back(),field.at("offset"),field.at("elementSize"),kind,(flags&1024)!=0});
                    }
                    Validate(spec,fn.at("parameterSize"),fields);found=true;++checked;
                }
                assert(found);
            }
        assert(checked==8);
        int deliveries=0;
        for(const auto& object:capture.at("objects"))if(object.value("type","")=="/Script/Dominion.QuestProgressComponent")
            for(const auto* name:{"Client_OnQuestUpdated","Client_OnQuestsUpdated","Client_NotifyTrackedQuestLoaded"}) {
                bool found=false;
                for(const auto& fn:object.at("functions"))if(fn.at("type")=="/Script/Dominion.QuestProgressComponent:"+std::string(name)) {
                    std::vector<std::string> names;names.reserve(4);std::vector<Field> fields;
                    for(const auto& field:fn.at("fields")) {
                        const auto type=field.at("type").get<std::string>();
                        const auto kind=type=="UQuestData*"?Kind::Quest:type=="bool"?Kind::Boolean:type=="FQuestProgress"?Kind::Progress:
                            type=="EQuestType"?Kind::QuestType:Kind::ProgressArray;
                        assert(type=="UQuestData*" || type=="bool" || type=="FQuestProgress" || type=="EQuestType" || type=="TArray");
                        assert(field.at("arrayDim")==1);
                        if(kind==Kind::ProgressArray)assert(field.at("elementType")=="FQuestProgress");
                        if(kind==Kind::Progress)assert(field.at("struct")=="/Script/Dominion.QuestProgress");
                        names.push_back(field.at("name"));
                        fields.push_back({names.back(),field.at("offset"),field.at("elementSize"),kind,
                            (field.at("flags").get<uint64_t>()&1024)!=0});
                    }
                    ValidateDelivery(name,fn.at("parameterSize"),fields);
                    found=true;++deliveries;
                }
                assert(found);
            }
        assert(deliveries==3);
    }
}
