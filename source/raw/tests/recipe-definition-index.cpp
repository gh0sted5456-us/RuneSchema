// The recipe definition index must return exactly what the linear scans it
// replaces returned, for duplicates, ambiguity and appended definitions.
#include "Core/RecipeDefinitionIndex.h"
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Def { std::string Owner,Key; };
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}

std::optional<std::size_t> LinearFirst(const std::vector<Def>& defs,const std::string& owner,const std::string& key) {
    for(std::size_t i=0;i<defs.size();++i)if(defs[i].Owner==owner&&defs[i].Key==key)return i;
    return std::nullopt;
}
// DragonWildsRecipeModLoader::ResolveReference before the index.
std::optional<std::size_t> LinearResolve(const std::vector<Def>& defs,const std::string& owner,const std::string& key,bool qualified) {
    std::optional<std::size_t> match;
    for(std::size_t i=0;i<defs.size();++i)if(defs[i].Owner==owner&&defs[i].Key==key){if(match)return std::nullopt;match=i;}
    if(!match&&!qualified)for(std::size_t i=0;i<defs.size();++i)if(defs[i].Key==key){if(match)return std::nullopt;match=i;}
    return match;
}
std::optional<std::size_t> IndexedResolve(const PS::RecipeDefinitionIndex::Definitions<std::string>& index,const std::string& owner,const std::string& key,bool qualified) {
    const auto owned=index.Owned(owner,key);
    if(owned.Ambiguous)return std::nullopt;
    if(owned.Index||qualified)return owned.Index;
    return index.AnyOwner(key).Index;
}
}

int main() try {
    using Index=PS::RecipeDefinitionIndex::Definitions<std::string>;
    std::vector<Def> defs;Index index;
    const auto sync=[&]{index.Sync(defs.size(),[&](std::size_t i)->const std::string&{return defs[i].Owner;},[&](std::size_t i)->const std::string&{return defs[i].Key;});};

    // Owner and key are compared separately: "a:b"+"c" must not match "a"+"b:c".
    defs={{"a:b","c"},{"a","b:c"}};sync();
    Check(index.First("a:b","c")==0&&index.First("a","b:c")==1&&!index.First("a","c"),"owner/key pairs never collide through a separator");
    Check(index.AnyOwner("c").Index==0&&index.AnyOwner("b:c").Index==1,"keys are indexed separately");

    // Randomised comparison against the linear scans, growing the list in steps.
    std::mt19937 random(20261119);
    const std::vector<std::string> owners{"ModA","ModB","ModC","RuneSchema.QuestCancellations"};
    defs.clear();index.Reset();
    for(int round=0;round<40;++round) {
        for(int add=0;add<25;++add)defs.push_back({owners[random()%owners.size()],"Recipe_"+std::to_string(random()%120)});
        sync();
        Check(index.Indexed()==defs.size(),"every appended definition is indexed");
        for(const auto& owner:owners)for(int k=0;k<130;++k){
            const auto key="Recipe_"+std::to_string(k);
            Check(index.First(owner,key)==LinearFirst(defs,owner,key),"First matches std::find_if");
            for(const bool qualified:{false,true})
                Check(IndexedResolve(index,owner,key,qualified)==LinearResolve(defs,owner,key,qualified),"resolution matches the linear scan, including ambiguity");
        }
    }
    // A shrunken list (never produced by the loader) rebuilds instead of returning stale positions.
    defs.resize(3);sync();
    Check(index.Indexed()==3,"a shorter list is reindexed");
    for(const auto& def:defs)Check(*index.First(def.Owner,def.Key)<3,"no stale position survives");

    PS::RecipeDefinitionIndex::PlacementTargets targets;
    Check(targets.Dirty(),"targets start dirty");
    targets.Rebuild([](auto add){add("CraftingTable",std::string{});add("ignored","/Game/Data/DT_Workbench.DT_Workbench");});
    Check(!targets.Dirty()&&targets.TargetsName("CraftingTable")&&!targets.TargetsName("ignored"),"name-only targets match by table name");
    Check(targets.TargetsPath("/Game/Data/DT_Workbench.DT_Workbench")&&!targets.TargetsPath("CraftingTable"),"path targets match by object path");
    targets.MarkDirty();Check(targets.Dirty(),"edits mark targets dirty");

    PS::RecipeDefinitionIndex::PassCache<std::string> cache;int resolved=0;
    for(int i=0;i<5;++i)Check(cache.Get("/Game/Items/Wood.Wood",[&]{++resolved;return std::string("/Game/Items/Wood.Wood");})=="/Game/Items/Wood.Wood","cached value returned");
    Check(resolved==1&&cache.Hits==4&&cache.Misses==1,"a repeated reference resolves once per pass");
    int failures=0;
    for(int i=0;i<3;++i)try{(void)cache.Get("/Game/Missing.Missing",[&]()->std::string{++failures;throw std::runtime_error("missing");});}catch(const std::runtime_error&){}
    Check(failures==3,"failures are never cached and are reported every time");
    std::cout<<"recipe-definition-index: ok\n";return 0;
} catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
