#include "Core/AutoReloadScope.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>
using PS::AutoReloadScope::Key;
int main()try{
    const auto check=[](bool ok){if(!ok)throw std::runtime_error("Auto-reload scope regression");};
    check(Key("mods/A/players/one.json","players")==Key("mods/A/players/two.json","players"));
    check(Key("mods/A/nameplates/one.json","nameplates")==Key("mods/A/nameplates/two.json","nameplates"));
    check(Key("mods/A/items/one.json","items")!=Key("mods/A/items/two.json","items"));
    check(Key("mods/A/players/one.json","players")!=Key("mods/B/players/one.json","players"));
    check(Key("mods/A/players/one.json","players")!=Key("mods/A/players/nested/two.json","players"));
    check(Key("mods/A/players/one.json","players")!=Key("mods/A/nameplates/one.json","nameplates"));
    std::vector<std::filesystem::path> queued;
    for(int i=0;i<100;++i){const auto key=Key("mods/A/players/"+std::to_string(i)+".json","players");if(std::find(queued.begin(),queued.end(),key)==queued.end())queued.push_back(key);}
    check(queued.size()==1);
    std::cout<<"Whole-folder reloads coalesce 100 file events into one scope; other files, owners and nested scopes remain distinct.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
