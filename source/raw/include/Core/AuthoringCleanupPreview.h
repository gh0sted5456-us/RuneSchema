#pragma once
#include "Core/SaveCleanup.h"

namespace PS::SaveCleanup {
// A preview binds consent to the source, options and complete registry used to
// produce the displayed removals. File metadata is checked separately by the host.
class AuthoringPreview {
    Json source,options,removed;
    std::shared_ptr<const RegistrySnapshot> registry;
    std::string token;
    uint64_t sequence=0;
    static bool SameRegistry(const RegistrySnapshot& a,const RegistrySnapshot& b) {
        return a.Items==b.Items&&a.Recipes==b.Recipes&&a.Quests==b.Quests&&a.Journals==b.Journals
            &&a.QuestsComplete==b.QuestsComplete&&a.JournalsComplete==b.JournalsComplete;
    }
public:
    void Reset(){source=nullptr;options=nullptr;removed=nullptr;registry.reset();token.clear();}
    void Inspect(const Json& document){Reset();RequireCharacter(document);source=document;}
    Json Preview(const Json& current,const Json& arguments,std::shared_ptr<const RegistrySnapshot> live) {
        token.clear();
        if(source.is_null()||current!=source)throw std::runtime_error("Source changed; inspect and preview again");
        const bool prune=arguments.value("PruneInvalid",false);
        const auto owners=arguments.value("Owners",std::set<std::string>{});
        if(owners.empty()&&!prune)throw std::runtime_error("Select owners or registry cleanup before preview");
        if(prune&&(!live||!live->Ready()||!live->QuestsComplete||!live->JournalsComplete))
            throw std::runtime_error("Complete item, recipe, quest and journal registries are required for invalid-ID cleanup");
        const auto plan=Plan(source,owners,arguments.value("EraseProgress",false),prune?live.get():nullptr,false,prune);
        options=arguments;removed=plan.Removed;registry=std::move(live);
        token="cleanup-"+std::to_string(++sequence);
        return {{"Token",token},{"Removed",removed},{"Count",removed.size()},{"Status","Review these exact removals before exporting a new copy."}};
    }
    PS::SaveCleanup::Preview Consume(const Json& current,const std::string& receipt,bool confirmed,std::shared_ptr<const RegistrySnapshot> live) {
        if(!confirmed||token.empty()||receipt!=token)throw std::runtime_error("Cleanup preview is missing, changed or unconfirmed");
        token.clear(); // Any attempted confirmed export consumes the preview.
        if(current!=source)throw std::runtime_error("Source changed; inspect and preview again");
        const bool prune=options.value("PruneInvalid",false);
        if(prune&&(!live||!live->Ready()||!live->QuestsComplete||!live->JournalsComplete||!registry||!SameRegistry(*live,*registry)))throw std::runtime_error("Registry changed; preview again");
        auto plan=Plan(source,options.value("Owners",std::set<std::string>{}),options.value("EraseProgress",false),prune?live.get():nullptr,false,prune);
        if(plan.Removed.empty()||plan.Removed!=removed)throw std::runtime_error("No unchanged, nonempty preview is available");
        return plan;
    }
};
}
