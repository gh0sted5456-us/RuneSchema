#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>
namespace PS::HelpyDependencies {
struct Node {std::vector<std::string> aliases;std::string source;};
struct Result {std::vector<std::size_t> ordered,blocked;};
inline std::string Key(std::string text) {for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);return text;}
// Iterative, stable topological ordering. Cycles (and their descendants) are
// isolated instead of recursively overflowing or blocking unrelated definitions.
inline Result Order(const std::vector<Node>& nodes) {
    std::unordered_map<std::string,std::vector<std::size_t>> owners;
    for(std::size_t i=0;i<nodes.size();++i)for(const auto& name:nodes[i].aliases) {
        if(name.empty())continue;
        auto& row=owners[Key(name)];
        if(std::find(row.begin(),row.end(),i)==row.end())row.push_back(i);
    }
    std::vector<std::size_t> degree(nodes.size());
    std::vector<std::vector<std::size_t>> children(nodes.size());
    for(std::size_t i=0;i<nodes.size();++i)if(!nodes[i].source.empty()) {
        const auto owner=owners.find(Key(nodes[i].source));if(owner==owners.end())continue;
        for(const auto parent:owner->second){++degree[i];children[parent].push_back(i);}
    }
    std::priority_queue<std::size_t,std::vector<std::size_t>,std::greater<>> ready;
    for(std::size_t i=0;i<nodes.size();++i)if(degree[i]==0)ready.push(i);
    Result result;result.ordered.reserve(nodes.size());
    while(!ready.empty()) {
        const auto i=ready.top();ready.pop();result.ordered.push_back(i);
        for(const auto child:children[i])if(--degree[child]==0)ready.push(child);
    }
    for(std::size_t i=0;i<nodes.size();++i)if(degree[i])result.blocked.push_back(i);
    return result;
}
}
