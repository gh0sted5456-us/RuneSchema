#include "Utility/AssetAliases.h"
#include <cassert>
int main() {
    PS::AssetAliases::Registry aliases;
    const std::wstring source=L"/Game/Mods/Set/Hood.Hood",runtime=L"/Game/RuneSchema/Armor/Items/hood.hood";
    assert(aliases.Resolve(source)==source);
    aliases.Add(source,runtime);
    assert(aliases.Resolve(source)==runtime);
    assert(aliases.Resolve(runtime)==runtime);
    assert(aliases.Resolve(L"/Game/Vanilla.Item")==L"/Game/Vanilla.Item");
    aliases.Add(source,runtime);
    bool conflict=false;try{aliases.Add(source,L"/Game/Other.Item");}catch(const std::exception&){conflict=true;}
    assert(conflict && aliases.Resolve(source)==runtime);
    aliases.Clear();assert(aliases.Resolve(source)==source);
}
