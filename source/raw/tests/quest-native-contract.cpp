#include "Loader/QuestNativeContract.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cassert>
#include <vector>
using namespace DragonWilds::QuestNative;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(int argc,char** argv){
    for(const auto method:{Method::Give,Method::Complete,Method::Initialize,Method::IsInitialized,Method::State,Method::Objective,Method::GetInt,Method::SetInt}) {
        const auto spec=Get(method);
        std::vector<Field> fields(spec.Fields.begin(),spec.Fields.end());
        Validate(spec,spec.Size,fields);
        assert(Rejects([&]{Validate(spec,spec.Size+1,fields);}));
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
            for(const auto& [name,size]:{std::pair{"Client_OnQuestUpdated",114},std::pair{"Client_OnQuestsUpdated",17},std::pair{"Client_NotifyTrackedQuestLoaded",8}}) {
                bool found=false;
                for(const auto& fn:object.at("functions"))if(fn.at("type")=="/Script/Dominion.QuestProgressComponent:"+std::string(name)) {
                    assert(fn.at("parameterSize")==size);
                    found=true;++deliveries;
                }
                assert(found);
            }
        assert(deliveries==3);
    }
}
