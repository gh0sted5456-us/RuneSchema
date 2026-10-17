#include "Core/AssetPreloadPlan.h"
#include "Core/DefinitionNameIndex.h"
#include <cassert>
int main(){
    PS::AssetPreload::Plan<std::wstring> plan;
    assert(plan.Add(L"/Mod/Items/A.A"));assert(!plan.Add(L"/Mod/Items/A.A"));
    assert(!plan.Add(L"bare"));assert(!plan.Add(L"/Mod/Package"));
    assert(!plan.Add(L"/Script/Engine.DataAsset"));assert(!plan.Add(L"/Game/RuneSchema/Mod/Items/A.A"));assert(!plan.Add(L"/Engine/Transient.A"));
    for(int i=0;i<50000;++i)assert(plan.Add(L"/Mod/Recipe/R"+std::to_wstring(i)+L".R"));
    assert(plan.Paths().size()==50001);
    PS::DefinitionNameIndex<std::wstring> names;
    for(int i=0;i<50000;++i)names.Add(L"recipe"+std::to_wstring(i));
    assert(!names.Ambiguous(L"recipe0"));names.Add(L"recipe0");assert(names.Ambiguous(L"recipe0"));
    assert(!names.Ambiguous(L"missing"));names.Clear();assert(!names.Ambiguous(L"recipe0"));
}
