#pragma once
#include "Core/DiagnosticCenterModel.h"
#include "Core/DiagnosticCenterHistory.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace PS::DiagnosticCenterMutes {
enum class Scope { Exact, Category };
enum class Duration { Session, Persistent };
struct Rule {
    Scope Kind=Scope::Exact;
    Duration Lifetime=Duration::Session;
    LiveLog::Level Severity=LiveLog::Level::Warning;
    std::string Match;
    std::uint64_t Suppressed=0;
    // Human-readable name saved alongside the exact matching identity.
    std::string Label;
};
inline const char* Name(Scope value){return value==Scope::Exact?"Specific issue":"Category";}
inline const char* Name(Duration value){return value==Duration::Session?"This session":"Until unmuted";}
class Manager {
    std::filesystem::path m_file;
    std::vector<Rule> m_rules;
    std::string m_error;
public:
    void Open(const std::filesystem::path& parent){
        m_file=parent/"diagnostic-mutes.json";m_rules.clear();m_error.clear();
        try{
            std::error_code ec;if(!std::filesystem::is_regular_file(m_file,ec))return;
            if(std::filesystem::file_size(m_file,ec)>1024*1024||ec)throw std::runtime_error("Mute file exceeds safety limit");
            std::ifstream in(m_file,std::ios::binary);if(!in)return;
            const auto doc=nlohmann::json::parse(in);
            if(doc.value("version",0)!=1||!doc.contains("rules")||!doc["rules"].is_array())return;
            for(const auto& item:doc["rules"]){
                if(!item.is_object()||m_rules.size()>=256)break;
                const auto match=item.value("match",std::string{});
                const auto severity=item.value("severity",-1);
                const auto scope=item.value("scope",std::string{});
                if(match.empty()||match.size()>32768||severity<0||severity>3||(scope!="exact"&&scope!="category"))continue;
                auto label=item.value("label",std::string{});
                if(label.size()>240)label.resize(240);
                if(label.empty())label=scope=="category"?match:"Specific issue (saved before labels)";
                m_rules.push_back({scope=="category"?Scope::Category:Scope::Exact,Duration::Persistent,
                    static_cast<LiveLog::Level>(severity),match,0,label});
            }
        }catch(const std::exception& e){m_error=e.what();}
    }
    const std::vector<Rule>& Rules()const{return m_rules;}
    const std::string& Error()const{return m_error;}
    bool Add(const DiagnosticCenterModel::Group& group,Scope kind,Duration duration){
        if(m_rules.size()>=256)return false;
        const std::string match=kind==Scope::Exact?group.Key:group.Category;
        if(match.empty())return false;
        for(const auto& rule:m_rules)if(rule.Kind==kind&&rule.Lifetime==duration&&rule.Severity==group.Severity&&rule.Match==match)return false;
        std::string label=kind==Scope::Exact?group.Title:group.Category;
        if(label.size()>240)label.resize(240);
        m_rules.push_back({kind,duration,group.Severity,match,0,std::move(label)});
        if(duration==Duration::Persistent)Save();
        return true;
    }
    bool Suppress(const LiveLog::Entry& entry){
        const auto identity=DiagnosticCenterModel::IdentityOf(entry);
        const auto category=DiagnosticCenterModel::CategoryOf(entry);
        bool suppressed=false;
        for(auto& rule:m_rules){
            if(rule.Severity!=entry.Severity)continue;
            if((rule.Kind==Scope::Exact&&rule.Match==identity)||(rule.Kind==Scope::Category&&rule.Match==category)){
                ++rule.Suppressed;suppressed=true;
            }
        }
        return suppressed;
    }
    bool SuppressStructured(const DiagnosticCenterModel::Group& group){
        bool suppressed=false;
        for(auto& rule:m_rules){
            if(rule.Severity!=group.Severity)continue;
            if((rule.Kind==Scope::Exact&&rule.Match==group.Key)||
               (rule.Kind==Scope::Category&&rule.Match==group.Category)){
                ++rule.Suppressed;suppressed=true;
            }
        }
        return suppressed;
    }
    bool Remove(std::size_t index){
        if(index>=m_rules.size())return false;
        const bool persist=m_rules[index].Lifetime==Duration::Persistent;
        m_rules.erase(m_rules.begin()+static_cast<std::ptrdiff_t>(index));
        if(persist)Save();
        return true;
    }
    void Save(){
        if(m_file.empty())return;
        nlohmann::json doc={{"version",1},{"rules",nlohmann::json::array()}};
        for(const auto& rule:m_rules)if(rule.Lifetime==Duration::Persistent)
            doc["rules"].push_back({{"scope",rule.Kind==Scope::Exact?"exact":"category"},
                {"severity",static_cast<int>(rule.Severity)},{"match",rule.Match},{"label",rule.Label}});
        if(!DiagnosticCenterHistory::AtomicReplace(m_file,doc.dump(2)))m_error="Failed to persist notification mute rules";
        else m_error.clear();
    }
};
}
