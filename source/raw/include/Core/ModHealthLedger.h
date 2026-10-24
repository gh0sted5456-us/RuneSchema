#pragma once
#include "Core/ModHealth.h"
#include <map>
#include <set>
namespace PS::ModHealth {
class Ledger {
    nlohmann::json Totals=nlohmann::json::object();
    std::map<std::string,State> Current;
public:
    void Load(const nlohmann::json& document){
        if(!document.is_object()||document.value("Format",0)!=1||!document.value("Mods",nlohmann::json{}).is_object()||document.at("Mods").size()>1024)throw std::runtime_error("Unsupported mod-health ledger");
        auto rows=document.at("Mods");
        for(auto& [owner,row]:rows.items()){
            if(owner.empty()||owner.size()>96||!row.is_object())throw std::runtime_error("Invalid mod-health owner");
            for(const auto* key:{"Errors","Warnings","OptionalProbes"})if(!row.value(key,nlohmann::json{}).is_number_unsigned())throw std::runtime_error("Invalid mod-health counter");
            if(!row.value("LastIssues",nlohmann::json::array()).is_array()||row.value("LastIssues",nlohmann::json::array()).size()>8)throw std::runtime_error("Invalid mod-health details");
            for(const auto& detail:row.value("LastIssues",nlohmann::json::array()))if(!detail.is_string()||detail.get_ref<const std::string&>().size()>1024)throw std::runtime_error("Invalid mod-health detail");
        }Totals=std::move(rows);Current.clear();
    }
    bool Reconcile(const std::set<std::string>& present){
        bool changed=false;for(auto it=Totals.begin();it!=Totals.end();)if(!present.contains(it.key())){Current.erase(it.key());it=Totals.erase(it);changed=true;}else ++it;
        for(const auto& owner:present)if(!Totals.contains(owner)){Totals[owner]={{"Errors",uint64_t{}},{"Warnings",uint64_t{}},{"OptionalProbes",uint64_t{}},{"LastIssues",nlohmann::json::array()}};changed=true;}
        return changed;
    }
    bool Observe(const std::string& owner,const LiveLog::Entry& row,std::string_view stream){
        if(!Totals.contains(owner))return false;State delta;ModHealth::Observe(delta,owner,row,stream);
        if(!delta.Errors&&!delta.Warnings&&!delta.OptionalProbes)return false;
        auto& live=Current[owner];live.Errors+=delta.Errors;live.Warnings+=delta.Warnings;live.OptionalProbes+=delta.OptionalProbes;
        auto& totals=Totals[owner];
        for(const auto& [key,count]:{std::pair{"Errors",delta.Errors},std::pair{"Warnings",delta.Warnings},std::pair{"OptionalProbes",delta.OptionalProbes}}){const auto before=totals.at(key).get<uint64_t>();totals[key]=before>UINT64_MAX-count?UINT64_MAX:before+count;}
        for(const auto& detail:delta.Details){if(live.Details.size()<24)live.Details.push_back(detail);
            auto& issues=totals["LastIssues"];issues.push_back(detail.substr(0,1024));while(issues.size()>8)issues.erase(issues.begin());}
        return true;
    }
    nlohmann::json Document()const{return {{"Format",1},{"Mods",Totals}};}
    nlohmann::json Read(const std::string& owner)const{
        const auto found=Current.find(owner);const State state=found==Current.end()?State{}:found->second;
        const auto total=Totals.find(owner);
        return {{"Errors",state.Errors},{"Warnings",state.Warnings},{"OptionalProbes",state.OptionalProbes},{"Details",state.Details},
            {"Lifetime",total==Totals.end()?nlohmann::json::object():*total}};
    }
};
}
