#pragma once

#include "Core/DiagnosticCenterModel.h"
#include <algorithm>
#include <string_view>
#include <vector>

namespace PS::DiagnosticIssueFilters {
inline bool Contains(std::string_view text,const std::string& needle){
    return !text.empty()&&LiveLog::Lower(text).find(needle)!=std::string::npos;
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
}
