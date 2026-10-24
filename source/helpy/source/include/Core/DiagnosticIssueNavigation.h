#pragma once

#include "Core/DiagnosticIssuePresentation.h"
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace PS::DiagnosticIssueNavigation {
using Group=DiagnosticCenterModel::Group;
using Store=DiagnosticCenterModel::Store;

inline std::uint64_t Adjacent(const std::vector<const Group*>& ordered,
                             std::uint64_t selected,int direction){
    if(ordered.empty()||direction==0)return 0;
    for(std::size_t i=0;i<ordered.size();++i){
        if(ordered[i]->Id!=selected)continue;
        if(direction>0)return i+1<ordered.size()?ordered[i+1]->Id:0;
        return i>0?ordered[i-1]->Id:0;
    }
    return direction>0?ordered.front()->Id:ordered.back()->Id;
}

inline bool Contains(const std::vector<const Group*>& ordered,std::uint64_t selected){
    for(const auto* group:ordered)if(group->Id==selected)return true;
    return false;
}

inline std::uint64_t ParentFor(const Store& store,
                               const std::vector<const Group*>& ordered,
                               std::uint64_t selected){
    if(!selected)return 0;
    const auto keys=DiagnosticIssuePresentation::LatestOperationKeys(store);
    const auto it=keys.find(selected);
    if(it==keys.end())return 0;
    for(const auto* group:ordered){
        const auto candidate=keys.find(group->Id);
        if(candidate!=keys.end()&&candidate->second==it->second)
            return group->Id;
    }
    return 0;
}

inline std::size_t SelectedRowIndex(const std::vector<DiagnosticIssuePresentation::Row>& rows,
                                    std::uint64_t selected){
    for(std::size_t i=0;i<rows.size();++i)
        if(rows[i].Issue&&rows[i].Issue->Id==selected)return i;
    return rows.size();
}
}
