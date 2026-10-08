#pragma once
// Reference-tree traversal is independent of HTTP/JSON/Unreal, so incomplete
// responses and path conversion can be regression-tested without a game host.
#include "Generator/F2CatalogPlan.h"
#include <functional>

namespace PS::F2ReferenceTree {
struct Node {std::string path,sha;bool directory=false;};
struct Tree {std::string sha;bool truncated=false;std::vector<Node> nodes;};
using Fetch=std::function<Tree(const std::string&,bool)>;
inline std::string Child(const Tree& tree,std::string_view name,bool required=true) {
    if(tree.truncated)throw std::runtime_error("Shallow reference tree is incomplete; existing cache retained");
    for(const auto& node:tree.nodes)if(node.directory&&node.path==name)return node.sha;
    if(required)throw std::runtime_error("RSDW reference folder unavailable: "+std::string(name));
    return {};
}
inline std::vector<F2Catalog::Candidate> Collect(const Fetch& fetch,const F2Catalog::Sources& sources,
                                                std::string& revision) {
    const auto root=fetch(sources.reference,false);revision=root.sha;
    const auto dataset=fetch(Child(root,sources.dataset),false);
    const auto exports=fetch(Child(dataset,"json"),false);
    const auto game=fetch(Child(exports,"RSDragonwilds"),false);
    const auto content=fetch(Child(game,"Content"),false);
    F2Catalog::Plan plan;
    std::function<void(const std::string&,const std::string&,unsigned)> visit;
    visit=[&](const std::string& sha,const std::string& prefix,unsigned depth) {
        if(depth>32)throw std::runtime_error("Reference tree nesting exceeds limit; cache retained");
        const auto tree=fetch(sha,true);
        if(tree.truncated) {
            // Never trust the partial recursive listing as a complete result.
            const auto shallow=fetch(sha,false);
            if(shallow.truncated)throw std::runtime_error("Shallow reference listing is incomplete");
            for(const auto& node:shallow.nodes) {
                if(node.directory)visit(node.sha,prefix+node.path+"/",depth+1);
                else if(auto entry=F2Catalog::FromExport(prefix+node.path,sources))plan.Add(*entry);
            }
        }else for(const auto& node:tree.nodes)if(!node.directory)
            if(auto entry=F2Catalog::FromExport(prefix+node.path,sources))plan.Add(*entry);
    };
    visit(Child(content,"Gameplay"),"Content/Gameplay/",0);
    const auto pluginId=Child(game,"Plugins",false);
    if(!pluginId.empty()) {
        const auto plugins=fetch(pluginId,false);
        const auto featuresId=Child(plugins,"GameFeatures",false);
        if(!featuresId.empty()) {
            const auto features=fetch(featuresId,false);
            if(features.truncated)throw std::runtime_error("GameFeatures listing is incomplete");
            // Request each plugin's Gameplay tree, never its textures, audio or
            // map art. This avoids giant recursive asset-tree downloads.
            for(const auto& plugin:features.nodes)if(plugin.directory) {
                if(!F2Catalog::SimpleToken(plugin.path))throw std::runtime_error("Invalid plugin mount name");
                const auto pluginTree=fetch(plugin.sha,false);
                const auto contentId=Child(pluginTree,"Content",false);if(contentId.empty())continue;
                const auto pluginContent=fetch(contentId,false);
                const auto gameplayId=Child(pluginContent,"Gameplay",false);if(gameplayId.empty())continue;
                visit(gameplayId,"Plugins/GameFeatures/"+plugin.path+"/Content/Gameplay/",0);
            }
        }
    }
    return std::move(plan.entries);
}
} // namespace PS::F2ReferenceTree
