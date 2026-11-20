#pragma once

#include "Core/DiagnosticCenterModel.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace PS::DiagnosticIssueFilters {
// Case-insensitive substring test against an already lower-cased needle. Allocation-free: the issue
// list runs this for up to 14 fields of every group whenever a search is active.
inline bool Contains(std::string_view text,const std::string& needle){
    if(text.empty())return false;
    if(needle.empty())return true;
    if(needle.size()>text.size())return false;
    static const auto table=[]{std::array<unsigned char,256> value{};for(int c=0;c<256;++c)value[c]=static_cast<unsigned char>(std::tolower(c));return value;}();
    const auto* data=reinterpret_cast<const unsigned char*>(text.data());
    const auto* want=reinterpret_cast<const unsigned char*>(needle.data());
    const std::size_t last=text.size()-needle.size();
    for(std::size_t at=0;at<=last;++at){
        if(table[data[at]]!=want[0])continue;
        std::size_t index=1;while(index<needle.size()&&table[data[at+index]]==want[index])++index;
        if(index==needle.size())return true;
    }
    return false;
}
inline bool Matches(const DiagnosticCenterModel::Group& group,std::string_view query,
                    bool showAll,bool onlyNew,int severityScope=0){
    if(severityScope==1&&group.Severity!=LiveLog::Level::Error)return false;
    if(severityScope==2&&group.Severity!=LiveLog::Level::Warning)return false;
    if(severityScope==3&&DiagnosticCenterModel::Actionable(group.Severity))return false;
    if(!showAll&&!DiagnosticCenterModel::Actionable(group.Severity))return false;
    if(onlyNew&&(group.Viewed||!DiagnosticCenterModel::Actionable(group.Severity)))return false;
    if(query.empty())return true;
    const auto lower=LiveLog::Lower(query);
    return Contains(group.Title,lower)||Contains(group.Key,lower)||
           Contains(group.Source,lower)||Contains(group.Category,lower)||
           Contains(group.Code,lower)||Contains(group.Message,lower)||
           Contains(group.ModId,lower)||Contains(group.File,lower)||
           Contains(group.Target,lower)||Contains(group.Property,lower)||
           Contains(group.Phase,lower)||Contains(group.Outcome,lower)||
           Contains(group.CauseDescription,lower)||
           (group.Help>=0&&group.Help<static_cast<int>(LiveLog::Guides.size())&&
            Contains(LiveLog::Guides[group.Help].Code,lower));
}
inline void Sort(std::vector<const DiagnosticCenterModel::Group*>& groups,int mode){
    std::sort(groups.begin(),groups.end(),[mode](const auto* a,const auto* b){
        if(mode==2&&a->Severity!=b->Severity){
            const auto rank=[](LiveLog::Level severity){
                return severity==LiveLog::Level::Error?0:severity==LiveLog::Level::Warning?1:2;
            };
            return rank(a->Severity)<rank(b->Severity);
        }
        if(mode==3&&a->Count!=b->Count)return a->Count>b->Count;
        if(a->LastSeen!=b->LastSeen)return mode==1?a->LastSeen<b->LastSeen:a->LastSeen>b->LastSeen;
        return a->Id<b->Id;
    });
}
// Filtered and sorted issue list, rebuilt only when the store or the filters change. The key includes the
// group buffer address and size, so cached pointers can never outlive a reallocation or a different store.
struct View {
    std::vector<const DiagnosticCenterModel::Group*> Groups;
    std::size_t Rebuilds=0;
    const std::vector<const DiagnosticCenterModel::Group*>& Update(const DiagnosticCenterModel::Store& store,std::string_view query,
        bool showAll,bool onlyNew,int severityScope,int sortMode){
        const auto& groups=store.Groups();
        // Unchanged frame: compare in place, without building a key (and a query copy) every frame.
        if(m_valid&&m_key.Data==groups.data()&&m_key.Size==groups.size()&&m_key.Revision==store.Revision()
            &&m_key.Query==query&&m_key.ShowAll==showAll&&m_key.OnlyNew==onlyNew&&m_key.Scope==severityScope
            &&m_key.SortMode==sortMode)return Groups;
        const Key next{groups.data(),groups.size(),store.Revision(),std::string(query),showAll,onlyNew,severityScope,sortMode};
        Groups.clear();Groups.reserve(groups.size());
        for(const auto& group:groups)if(Matches(group,query,showAll,onlyNew,severityScope))Groups.push_back(&group);
        Sort(Groups,sortMode);
        m_key=next;m_valid=true;++Rebuilds;return Groups;
    }
    void Reset(){m_valid=false;Groups.clear();}
private:
    struct Key {
        const void* Data=nullptr;std::size_t Size=0;std::uint64_t Revision=0;std::string Query;
        bool ShowAll=false,OnlyNew=false;int Scope=0,SortMode=0;
        bool operator==(const Key&)const=default;
    };
    Key m_key;bool m_valid=false;
};
}
